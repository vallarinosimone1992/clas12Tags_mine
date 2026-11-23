#!/usr/bin/env python3
"""Detached process supervisor for independent, single-thread MINE GEMC runs."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time
import uuid

from mine_random_seed import MAX_SEED, unique_seeds

REPO = Path(__file__).resolve().parent.parent
LAUNCHER = REPO / 'scripts/run_mine.sh'
CONTAINER_ROOT = Path('/work/clas12Tags_mine')
TERMINAL = {'completed', 'failed', 'cancelled', 'planned'}


def now():
    return datetime.now(timezone.utc).isoformat(timespec='seconds')


def save(path, value):
    temp = path.with_suffix(path.suffix + '.tmp')
    temp.write_text(json.dumps(value, indent=2) + '\n')
    temp.replace(path)


def positive(value):
    result = int(value)
    if not 1 <= result <= MAX_SEED:
        raise argparse.ArgumentTypeError(f'Usare un intero fra 1 e {MAX_SEED}')
    return result


def batch_path(value):
    p = Path(value).expanduser()
    if not p.is_absolute():
        p = Path.cwd() / p if p.exists() else REPO / p
    return p.resolve()


def read_batch(directory):
    return json.loads((directory / 'manifest.json').read_text())


def show(directory):
    batch = read_batch(directory)
    print(f"Batch: {directory}\nStato: {batch['state']}; simultanei: {batch['jobs']}")
    for job in batch['runs']:
        progress = job.get('progress', 'in attesa')
        print(f"  {job['index']:04d}  {job['state']:10s}  seed={job['seed']}  {progress}")
    print(f"Log generale: {directory / 'batch.log'}")
    if (directory / 'inputs.list').exists():
        print(f"Lista per analisi: {directory / 'inputs.list'}")
    return batch


def launcher_arguments(arguments):
    """Preserve argument boundaries; batch owns N/seed/output/GUI/run directives."""
    args = list(arguments)
    if args and args[0] == '--':
        args.pop(0)
    prohibited = ('-N=', '-RANDOM=', '-OUTPUT=', '-USE_GUI=', '-EXEC_MACRO=')
    for arg in args:
        if arg.startswith(prohibited) or arg in (
            '--gui', '--txt', '--dry-run', '--help', '-h', '--help-gemc', '--help-physics'
        ) or arg.startswith('-help'):
            raise ValueError(f'{arg}: eventi, seed, output e avvio sono gestiti dal batch')
    return args


def snapshot_inputs(arguments, directory):
    # Pin the optical input and gcard for queued jobs, even if a host symlink changes.
    result = list(arguments)
    defaults = {
        '--card': str(REPO / 'experiments/mine/mine.gcard'),
        '--response': os.environ.get('MINE_OPTICAL_RESPONSE', str(REPO / 'experiments/mine/responses/optical_response.dat')),
        '--optical-config': os.environ.get('MINE_GEANT4_CONFIG', str(REPO / 'experiments/mine/geant4_optics.conf')),
    }
    selected = dict(defaults)
    transport = 'tabulated'
    found = set()
    for i, arg in enumerate(result):
        if arg in defaults or arg in ('--transport', '--lund'):
            if i + 1 >= len(result):
                raise ValueError(f'Valore mancante: {arg}')
            if arg == '--transport':
                transport = result[i + 1]
            elif arg in defaults:
                selected[arg] = result[i + 1]
                found.add(arg)
    keys = ['--card', '--optical-config' if transport == 'geant4' else '--response']
    names = {'--card': 'base.gcard', '--optical-config': 'geant4_optics.conf', '--response': 'response.dat'}
    records = []
    folder = directory / 'inputs'
    folder.mkdir()
    for key in keys:
        source = Path(selected[key]).expanduser()
        if not source.is_absolute():
            source = REPO / source if key == '--card' else Path.cwd() / source
        source = source.resolve(strict=True)
        destination = folder / names[key]
        shutil.copyfile(source, destination)
        records.append({'option': key, 'source': str(source), 'snapshot': str(destination),
                        'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()})
        for i, arg in enumerate(result):
            if arg == key:
                result[i + 1] = str(destination)
        if key not in found:
            result += [key, str(destination)]
    # LUND is not duplicated (may be large); every run reads the same first M events.
    for i, arg in enumerate(result):
        if arg == '--lund':
            result[i + 1] = str(Path(result[i + 1]).expanduser().resolve(strict=True))
    return result, records


def prepare(args, forwarded):
    seeds = unique_seeds(args.runs, args.seed_base)
    # Fail invalid geometry/config/LUND requests before starting any process.
    check = subprocess.run(['bash', str(LAUNCHER), '--dry-run', '--batch', *forwarded,
                            f'-N={args.events}', f'-RANDOM={seeds[0]}'],
                           cwd=REPO, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check.returncode:
        raise ValueError('Preflight GEMC fallito:\n' + check.stdout)
    container = os.environ.get('MINE_GEMC_CONTAINER', 'mine-gemc-vis')
    if not args.dry_run:
        check_docker = subprocess.run(['docker', 'inspect', '--format', '{{.State.Running}}', container],
                                      capture_output=True, text=True)
        if check_docker.returncode or check_docker.stdout.strip() != 'true':
            raise ValueError(f'Container {container!r} non disponibile: {check_docker.stderr.strip()}')
    batch_id = 'batch_' + datetime.now().strftime('%Y%m%d_%H%M%S') + '_' + uuid.uuid4().hex[:8]
    directory = batch_path(args.output_dir) if args.output_dir else REPO / 'experiments/mine/output' / batch_id
    try:
        relative = directory.relative_to(REPO)
    except ValueError as error:
        raise ValueError('--output-dir deve essere nel progetto GEMC montato in Docker') from error
    directory.mkdir(parents=True, exist_ok=False)
    (directory / 'preflight.log').write_text(check.stdout)
    forwarded, snapshots = snapshot_inputs(forwarded, directory)
    records = []
    commands = []
    for index, seed in enumerate(seeds, 1):
        stem = f'run_{index:04d}'
        output = directory / f'{stem}.hipo'
        container_output = CONTAINER_ROOT / relative / output.name
        run_id = f'{batch_id}_{index:04d}'
        command = ['bash', str(LAUNCHER), '--batch', *forwarded, f'-N={args.events}',
                   f'-RANDOM={seed}', f'-OUTPUT=hipo, {container_output}']
        records.append({'index': index, 'seed': seed, 'run_id': run_id, 'events_requested': args.events,
                        'state': 'planned' if args.dry_run else 'queued', 'output': str(output),
                        'log': str(directory / f'{stem}.log'),
                        'gemc_log': str(REPO / 'experiments/mine/output' / f'mine_{run_id}.log'),
                        'command': command})
        commands.append(f'MINE_GEMC_CONTAINER={shlex.quote(container)} MINE_RUN_ID={run_id} ' + shlex.join(command))
    batch = {'version': 1, 'id': batch_id, 'created_utc': now(), 'repo': str(REPO),
             'state': 'planned' if args.dry_run else 'starting', 'jobs': min(args.jobs or args.runs, args.runs),
             'events_per_run': args.events, 'seed_mode': 'base + index' if args.seed_base else 'OS randomness, unique within batch',
             'container': container, 'input_snapshots': snapshots, 'runs': records}
    save(directory / 'manifest.json', batch)
    (directory / 'seeds.tsv').write_text('run\tseed\toutput\n' + ''.join(
        f"{j['index']}\t{j['seed']}\t{j['output']}\n" for j in records))
    (directory / 'commands.sh').write_text('# Riproduzione: stesse impostazioni e seed; scegliere NUOVI output prima di eseguire.\n' + '\n'.join(commands) + '\n')
    return directory, batch


PROGRESS = re.compile(r'MINE (?:progress|run completed|run interrupted|progresso|fine run|run interrotta): (?:events|eventi)=(\d+)/(\d+).*?(?:written|scritti)=(\d+).*?ETA=([^\n|]+)')


def update_progress(job):
    try:
        with open(job['log'], 'rb') as stream:
            stream.seek(0, os.SEEK_END)
            stream.seek(max(0, stream.tell() - 16384))
            matches = list(PROGRESS.finditer(stream.read().decode(errors='replace')))
        if matches:
            last = matches[-1]
            job['events_completed'] = int(last[1])
            job['events_written'] = int(last[3])
            job['progress'] = f"eventi={last[1]}/{last[2]}, scritti={last[3]}, ETA={last[4].strip()}"
    except OSError:
        pass


def worker(directory):
    manifest = directory / 'manifest.json'
    batch = read_batch(directory)
    batch['state'] = 'running'
    batch['supervisor_pid'] = os.getpid()
    stopping = False
    active = {}

    def request_stop(_sig, _frame):
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGTERM, request_stop)
    signal.signal(signal.SIGINT, request_stop)
    os.environ['MINE_GEMC_CONTAINER'] = batch['container']
    print(f"{now()} Batch avviato: {len(batch['runs'])} run, {batch['jobs']} simultanee", flush=True)
    try:
        while active or any(j['state'] == 'queued' for j in batch['runs']):
            stopping = stopping or (directory / 'STOP_REQUESTED').exists()
            if stopping:
                batch['state'] = 'stopping'
                for job in batch['runs']:
                    if job['state'] == 'queued':
                        job['state'] = 'cancelled'
                for index, (process, stream) in list(active.items()):
                    job = batch['runs'][index]
                    if process.poll() is None:
                        # This selector cannot affect unrelated simulations.
                        subprocess.run(['bash', str(REPO / 'scripts/stop_mine.sh'), '--run-id', job['run_id'], '--grace', '2'],
                                       stdout=stream, stderr=subprocess.STDOUT, timeout=30)
                        try:
                            os.killpg(process.pid, signal.SIGTERM)
                        except ProcessLookupError:
                            pass
                        try:
                            process.wait(timeout=10)
                        except subprocess.TimeoutExpired:
                            os.killpg(process.pid, signal.SIGKILL)
                            process.wait()
                    stream.close()
                    update_progress(job)
                    job.update(state='cancelled', exit_code=process.returncode, ended_utc=now())
                    del active[index]
            for index, (process, stream) in list(active.items()):
                job = batch['runs'][index]
                update_progress(job)
                code = process.poll()
                if code is None:
                    continue
                stream.close()
                update_progress(job)
                complete = code == 0 and Path(job['output']).is_file() and job.get('events_completed') == job['events_requested']
                job.update(state='completed' if complete else 'failed', exit_code=code, ended_utc=now())
                if not complete:
                    job['error'] = 'Consultare il log: uscita non riuscita, output assente o conteggio eventi incompleto'
                print(f"{now()} Run {job['index']:04d}: {job['state']} (seed={job['seed']}, exit={code})", flush=True)
                del active[index]
            for index, job in enumerate(batch['runs']):
                if stopping or len(active) >= batch['jobs']:
                    break
                if job['state'] != 'queued':
                    continue
                stream = open(job['log'], 'w')
                env = dict(os.environ, MINE_RUN_ID=job['run_id'])
                process = subprocess.Popen(job['command'], cwd=REPO, env=env, stdin=subprocess.DEVNULL,
                                           stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
                active[index] = (process, stream)
                job.update(state='running', host_pid=process.pid, started_utc=now())
                print(f"{now()} Run {job['index']:04d}: avviata, seed={job['seed']}, PID host={process.pid}", flush=True)
            save(manifest, batch)
            if active:
                time.sleep(0.5)
    except BaseException as error:
        batch['state'] = 'failed'
        batch['error'] = str(error)
        for index, (process, stream) in active.items():
            job = batch['runs'][index]
            # Best-effort cleanup of our own launches on a supervisor error.
            try:
                subprocess.run(['bash', str(REPO / 'scripts/stop_mine.sh'), '--run-id', job['run_id'], '--grace', '2'],
                               stdout=stream, stderr=subprocess.STDOUT, timeout=30)
                os.killpg(process.pid, signal.SIGTERM)
            except (OSError, subprocess.TimeoutExpired):
                pass
            stream.close()
            job.update(state='failed', error='Supervisor interrotto; verificare il processo nel container')
        for job in batch['runs']:
            if job['state'] == 'queued':
                job['state'] = 'cancelled'
    else:
        batch['state'] = 'cancelled' if stopping else ('completed' if all(j['state'] == 'completed' for j in batch['runs']) else 'failed')
    batch['ended_utc'] = now()
    successful = [j['output'] for j in batch['runs'] if j['state'] == 'completed']
    (directory / 'completed.list').write_text(''.join(path + '\n' for path in successful))
    if batch['state'] == 'completed':
        (directory / 'inputs.list').write_text(''.join(path + '\n' for path in successful))
    save(manifest, batch)
    print(f"{now()} Batch {batch['state']}: {len(successful)}/{len(batch['runs'])} run completate", flush=True)
    return 0 if batch['state'] == 'completed' else (130 if stopping else 1)


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--worker':
        return worker(Path(sys.argv[2]))
    parser = argparse.ArgumentParser(
        prog="scripts/run_mine_parallel.sh",
        description='Avvia N simulazioni GEMC da M eventi, in processi separati in background, con seed distinti.',
        epilog='Dopo -- inoltrare le opzioni di run_mine.sh (geometria, trasporto, config, generatore). N/RANDOM/OUTPUT/GUI sono gestiti qui. Default: stesso trasporto tabulated del launcher; usare -- --transport geant4 per fotoni espliciti.')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--status', metavar='CARTELLA', help='Stato, progresso ed ETA delle run di un batch')
    mode.add_argument('--stop', metavar='CARTELLA', help='Arresta soltanto le run di questo batch; gli HIPO interrotti possono essere incompleti')
    parser.add_argument('--runs', '-n', type=positive, help='N simulazioni')
    parser.add_argument('--events', '-m', type=positive, help='M eventi per simulazione')
    parser.add_argument('--jobs', '-j', type=positive, help='Massimo di simulazioni simultanee (default N)')
    parser.add_argument('--seed-base', type=int, default=0, help='Seed riproducibili base,base+1,...; default/0: casuali dal sistema operativo')
    parser.add_argument('--output-dir', help='NUOVA cartella batch nel progetto; default experiments/mine/output/batch_DATA_ID')
    parser.add_argument('--wait', action='store_true', help='Attende il batch; Ctrl+C ne richiede lo stop. Default: ritorna subito')
    parser.add_argument('--dry-run', action='store_true', help='Valida e salva piano/seed/comandi; non richiede Docker e non avvia processi')
    parser.add_argument('gemc_options', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.status or args.stop:
        directory = batch_path(args.status or args.stop)
        batch = show(directory)
        if args.stop and batch['state'] not in TERMINAL:
            (directory / 'STOP_REQUESTED').touch()
            print('Arresto richiesto; il supervisore fermera soltanto questo batch.')
        return 0
    if args.runs is None or args.events is None:
        parser.error('Specificare --runs N e --events M')
    directory, batch = prepare(args, launcher_arguments(args.gemc_options))
    print(f"Cartella batch: {directory}")
    print(f"Run: {args.runs}; eventi/run: {args.events}; processi simultanei: {batch['jobs']}")
    if args.dry_run:
        print('Piano salvato; nessuna simulazione avviata.')
        return 0
    with (directory / 'batch.log').open('a') as log:
        process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker', str(directory)],
                                   cwd=REPO, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                                   start_new_session=True)
    print(f"Supervisore PID={process.pid}; continua anche chiudendo il terminale.")
    print('Stato: ' + shlex.join([str(REPO / 'scripts/run_mine_parallel.sh'), '--status', str(directory)]))
    print('Stop:  ' + shlex.join([str(REPO / 'scripts/run_mine_parallel.sh'), '--stop', str(directory)]))
    print(f"Lista inputs.list disponibile quando tutte le run sono completate; log in {directory}")
    if args.wait:
        try:
            return process.wait()
        except KeyboardInterrupt:
            (directory / 'STOP_REQUESTED').touch()
            print('Arresto richiesto; attendo la chiusura dei processi del batch.', flush=True)
            process.wait()
            return 130
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(2)
