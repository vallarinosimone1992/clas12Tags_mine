#!/usr/bin/env python3
"""Supervisor/seed checks using inert Docker processes; no GEMC simulation."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / 'scripts'))
import mine_random_seed


class Seeds(unittest.TestCase):
    def test_ranges_fixed_and_auto(self):
        for mode in ('auto', '0', '000', 'TIME', 'random'):
            self.assertTrue(1 <= mine_random_seed.resolve_seed(mode) <= mine_random_seed.MAX_SEED)
        self.assertEqual(mine_random_seed.resolve_seed('23456'), 23456)
        for invalid in ('-1', 'nan', '1.5', str(mine_random_seed.MAX_SEED + 1)):
            with self.assertRaises(ValueError):
                mine_random_seed.resolve_seed(invalid)
        self.assertEqual(mine_random_seed.unique_seeds(4, 123), [123, 124, 125, 126])
        with self.assertRaises(ValueError):
            mine_random_seed.unique_seeds(2, mine_random_seed.MAX_SEED)

    def test_random_collisions_removed(self):
        with mock.patch.object(mine_random_seed, 'resolve_seed', side_effect=[3, 3, 4, 5]):
            self.assertEqual(mine_random_seed.unique_seeds(3), [3, 4, 5])


class Supervisor(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name) / 'repo with spaces'
        self.scripts = self.repo / 'scripts'
        self.scripts.mkdir(parents=True)
        for name in ('run_mine.sh', 'mine_launch_card.py', 'mine_random_seed.py',
                     'mine_parallel.py', 'run_mine_parallel.sh'):
            shutil.copyfile(REPO / 'scripts' / name, self.scripts / name)
        experiment = self.repo / 'experiments/mine'
        experiment.mkdir(parents=True)
        for pattern in ('mine.gcard', 'mine__*_corner_fan.txt'):
            for source in (REPO / 'experiments/mine').glob(pattern):
                shutil.copyfile(source, experiment / source.name)
        (experiment / 'geant4_optics.conf').write_text('model=monochromatic_effective\nwlsAbsorptionLengthMm=.1\n')
        tools = Path(self.temp.name) / 'bin'
        tools.mkdir()
        docker = tools / 'docker'
        docker.write_text(r'''#!/usr/bin/env python3
import json, os, pathlib, sys, tarfile, time
root=pathlib.Path(os.environ['MOCK_REPO'])
if sys.argv[1]=='inspect':
 print('true');sys.exit(0)
with tarfile.open(fileobj=sys.stdin.buffer, mode='r|') as archive:
 for member in archive:
  if member.isfile(): archive.extractfile(member).read()
args=sys.argv[sys.argv.index('mine-launch')+1:]
run_id=args[0]
options=args[3:]
seed=int(next(a.split('=',1)[1] for a in options if a.startswith('-RANDOM=')))
events=int(next(a.split('=',1)[1] for a in options if a.startswith('-N=')))
output=next(a.split(', ',1)[1] for a in options if a.startswith('-OUTPUT='))
output=root/output.removeprefix('/work/clas12Tags_mine/')
record={'seed':seed,'options':options,'start':time.time()}
start=time.monotonic()
while time.monotonic()-start<float(os.environ.get('MOCK_DELAY','.2')):
 if (root/('stop_'+run_id)).exists(): sys.exit(143)
 time.sleep(.02)
if run_id.endswith(os.environ.get('MOCK_FAIL_SUFFIX','no failure')):
 print('MOCK run failure',flush=True);sys.exit(1)
output.write_bytes(b'closed mock HIPO')
record['end']=time.time()
(root/(run_id+'.json')).write_text(json.dumps(record))
print(f'MINE fine run: eventi={events}/{events} (100.0%) | scritti={events} | trascorso=00:00:01 | velocita=1.00 ev/s | ETA=00:00:00',flush=True)
''')
        docker.chmod(0o755)
        (self.scripts / 'stop_mine.sh').write_text('''#!/usr/bin/env bash
set -euo pipefail
while [[ $# -gt 0 ]]; do
 case "$1" in
 --run-id) touch "$MOCK_REPO/stop_$2"; shift 2 ;;
 *) shift ;;
 esac
done
''')
        self.env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'],
                        MOCK_REPO=str(self.repo))
        self.output = self.repo / 'experiments/mine/output/test batch'
        self.base = ['--runs', '4', '--events', '3', '--jobs', '2', '--output-dir', str(self.output)]

    def run_script(self, options, **kwargs):
        return subprocess.run(['bash', str(self.scripts / 'run_mine_parallel.sh'), *options],
                              env=self.env, cwd=self.repo, capture_output=True, text=True,
                              timeout=30, **kwargs)

    def manifest(self):
        return json.loads((self.output / 'manifest.json').read_text())

    def wait_finished(self, limit=15):
        deadline=time.monotonic()+limit
        while time.monotonic()<deadline:
            batch=self.manifest()
            if batch['state'] in ('completed','failed','cancelled'):
                return batch
            time.sleep(.05)
        self.fail('Supervisor did not finish')

    def test_parallel_limit_unique_seeds_outputs_and_analysis_list(self):
        result=self.run_script([*self.base, '--wait', '--', '--transport', 'geant4',
                                '-BEAM_V=(0, 20, -100)mm'])
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        batch=self.manifest()
        self.assertEqual(batch['state'], 'completed')
        self.assertEqual(len({job['seed'] for job in batch['runs']}), 4)
        self.assertEqual(len((self.output/'inputs.list').read_text().splitlines()), 4)
        points=[]
        for job in batch['runs']:
            record=json.loads((self.repo/(job['run_id']+'.json')).read_text())
            self.assertEqual(job['seed'], record['seed'])
            self.assertEqual(job['events_completed'], 3)
            self.assertEqual(Path(job['output']).read_bytes(), b'closed mock HIPO')
            self.assertIn('-BEAM_V=(0, 20, -100)mm', record['options'])
            points += [(record['start'],1), (record['end'],-1)]
        running=peak=0
        for _,change in sorted(points):
            running+=change;peak=max(peak,running)
        self.assertEqual(peak, 2)
        self.assertTrue(all(Path(x['snapshot']).is_file() for x in batch['input_snapshots']))
        self.assertEqual(self.run_script(['--status', str(self.output)]).returncode, 0)
        second=self.run_script([*self.base, '--dry-run', '--', '--transport', 'geant4'])
        self.assertNotEqual(second.returncode, 0) # Never overwrite an existing batch.

    def test_detached_and_partial_failure(self):
        self.env['MOCK_FAIL_SUFFIX']='_0002'
        self.env['MOCK_DELAY']='.6'
        result=self.run_script([*self.base, '--seed-base', '1234', '--', '--transport', 'geant4'])
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        batch=self.wait_finished()
        self.assertEqual(batch['state'],'failed')
        self.assertEqual([j['seed'] for j in batch['runs']], [1234,1235,1236,1237])
        self.assertEqual([j['state'] for j in batch['runs']].count('failed'), 1)
        self.assertEqual(len((self.output/'completed.list').read_text().splitlines()), 3)
        self.assertFalse((self.output/'inputs.list').exists())

    def test_stop_selects_only_this_batch(self):
        self.env['MOCK_DELAY']='10'
        result=self.run_script([*self.base, '--', '--transport', 'geant4'])
        self.assertEqual(result.returncode, 0, result.stderr)
        deadline=time.monotonic()+10
        while time.monotonic()<deadline:
            if any(j['state']=='running' for j in self.manifest()['runs']): break
            time.sleep(.05)
        result=self.run_script(['--stop', str(self.output)])
        self.assertEqual(result.returncode, 0, result.stderr)
        batch=self.wait_finished()
        self.assertEqual(batch['state'],'cancelled')
        self.assertEqual([j['state'] for j in batch['runs']], ['cancelled']*4)
        requested={x.name.removeprefix('stop_') for x in self.repo.glob('stop_*')}
        self.assertTrue(requested)
        self.assertTrue(requested <= {j['run_id'] for j in batch['runs']})
        self.assertFalse((self.output/'inputs.list').exists())

    def test_dry_run_and_invalid_flags(self):
        result=self.run_script([*self.base, '--dry-run', '--seed-base', '123', '--', '--transport', 'geant4'])
        self.assertEqual(result.returncode, 0, result.stderr)
        batch=self.manifest()
        self.assertEqual(batch['state'],'planned')
        self.assertEqual([j['seed'] for j in batch['runs']], [123,124,125,126])
        self.assertFalse((self.output/'batch.log').exists())
        self.assertFalse(list(self.output.glob('*.hipo')))
        for bad in ('-N=5', '-RANDOM=123', '-OUTPUT=hipo, /tmp/test', '--gui'):
            result=self.run_script(['--runs','2','--events','1','--dry-run','--',bad])
            self.assertNotEqual(result.returncode,0)
            self.assertIn('gestiti dal batch', result.stderr)


if __name__=='__main__':
    unittest.main()
