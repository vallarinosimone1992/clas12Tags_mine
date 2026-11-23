#!/usr/bin/env python3
"""Preflight tests: no Docker or GEMC event generation."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

REPO = Path(__file__).resolve().parent.parent


class LaunchCardTests(unittest.TestCase):
    def run_card(self, geometry, requested='auto', card='experiments/mine/mine.gcard'):
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            table = temp / 'response.dat'
            table.write_text('MINE_OPTICAL_RESPONSE 1\nMETA 4\n'
                             f'"fiber_geometry" "{geometry}"\n'
                             '"gemc_contract_version" "1"\n'
                             '"sensor_count" "2"\n"number_of_fibers" "15"\n')
            output = temp / 'launch.gcard'
            result = subprocess.run(['python3', str(REPO / 'scripts/mine_launch_card.py'),
                                     '--repo', str(REPO), '--card', card,
                                     '--geometry', requested, '--response', str(table),
                                     '--output', str(output)], capture_output=True, text=True)
            root = ET.parse(output).getroot() if output.exists() else None
            return result, root

    def test_supported_variations(self):
        for table, variation in [('diagonal_corner_fan', 'corner_fan'),
                                 ('parallel_fan_in', 'parallel_fan_in'),
                                 ('compact_fan_in', 'parallel_fan_in'),
                                 ('triple_serpentine', 'triple_serpentine'),
                                 ('panel_double_serpentine', 'panel_double_serpentine')]:
            with self.subTest(table=table):
                result, root = self.run_card(table)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(root.find('detector').get('variation'), variation)
                self.assertEqual(root.find("option[@name='BEAM_P']").get('value'),
                                 'mu-, 10*GeV, 0*deg, 0*deg')

    def test_alias(self):
        result, _ = self.run_card('diagonal_corner_fan', 'diagonal_corner_fan')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_mismatch_and_unknown_are_fatal(self):
        for table, request in [('diagonal_corner_fan', 'triple_serpentine'),
                               ('diagonal_corner_fan', 'not_implemented'),
                               ('not_implemented', 'auto')]:
            result, root = self.run_card(table, request)
            self.assertNotEqual(result.returncode, 0)
            self.assertIsNone(root)

    def run_geant4(self, requested='auto', extra=(), config_text=None):
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            config = temp / 'model.conf'
            config.write_text(config_text or 'model=monochromatic_effective\nwlsAbsorptionLengthMm=0.1\n')
            output = temp / 'launch.gcard'
            result = subprocess.run(['python3', str(REPO / 'scripts/mine_launch_card.py'),
                                     '--repo', str(REPO), '--card', 'experiments/mine/mine.gcard',
                                     '--geometry', requested, '--transport', 'geant4',
                                     '--optical-config', str(config),
                                     '--output', str(output), *extra], capture_output=True, text=True)
            root = ET.parse(output).getroot() if output.exists() else None
            return result, root

    def test_geant4_requires_no_optical_table(self):
        for requested in ('auto', 'corner_fan', 'diagonal_corner_fan'):
            with self.subTest(requested=requested):
                result, root = self.run_geant4(requested)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(root.find('detector').get('variation'), 'corner_fan')
                self.assertEqual(root.find("option[@name='BEAM_P']").get('value'),
                                 'mu-, 10*GeV, 0*deg, 0*deg')

    def test_geant4_rejects_extension_geometries(self):
        for requested in ('parallel_fan_in', 'triple_serpentine', 'panel_double_serpentine'):
            with self.subTest(requested=requested):
                result, root = self.run_geant4(requested)
                self.assertNotEqual(result.returncode, 0)
                self.assertIsNone(root)
                self.assertIn('only corner_fan', result.stderr)

    def test_mini_spiral_uses_its_config_and_generator(self):
        config = (REPO / 'experiments/mine/geant4_optics_mini_panel_spiral.conf').read_text()
        for requested in ('auto', 'mini_panel_spiral'):
            with self.subTest(requested=requested):
                result, root = self.run_geant4(requested, extra=('--default-card',), config_text=config)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(root.find('detector').get('variation'), 'mini_panel_spiral')
                self.assertEqual(root.find("option[@name='SPREAD_V']").get('value'), '(150, 250, 0)mm')
                self.assertIn('geometry=mini_panel_spiral', result.stdout)
                self.assertIn('sipm=ideal', result.stdout)

    def test_mini_spiral_mismatch_and_tabulated_are_rejected(self):
        config = (REPO / 'experiments/mine/geant4_optics_mini_panel_spiral.conf').read_text()
        for request, text in [('corner_fan', config), ('mini_panel_spiral', None)]:
            result, root = self.run_geant4(request, config_text=text)
            self.assertNotEqual(result.returncode, 0)
            self.assertIsNone(root)
            self.assertIn('optical config describes', result.stderr)
        result, root = self.run_card('diagonal_corner_fan', 'mini_panel_spiral')
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(root)
        self.assertIn('only with --transport geant4', result.stderr)

    def test_mini_spiral_hamamatsu_is_reported(self):
        config = (REPO / 'experiments/mine/geant4_optics_mini_panel_spiral_hamamatsu.conf').read_text()
        result, root = self.run_geant4('mini_panel_spiral', config_text=config)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('sipm=hamamatsu', result.stdout)
        self.assertEqual(root.find('detector').get('variation'), 'mini_panel_spiral')

    def test_mini_spiral_script_selects_profile_without_docker(self):
        env = dict(os.environ)
        env.pop('MINE_GEANT4_CONFIG', None)
        result = subprocess.run(['bash', str(REPO / 'scripts/run_mine.sh'), '--dry-run',
                                 '--batch', '--transport', 'geant4', '--geometry', 'mini_panel_spiral',
                                 '-N=1', '-RANDOM=20261122'], capture_output=True, text=True, env=env)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('geometry=mini_panel_spiral', result.stdout)
        self.assertIn('geant4_optics_mini_panel_spiral.conf', result.stdout)

    def test_geant4_rejects_table_argument(self):
        result, root = self.run_geant4(extra=('--response', '/missing/response.dat'))
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(root)
        self.assertIn('--response is only compatible', result.stderr)

    def test_seed_resolution_in_dry_run(self):
        import re
        for value in ('0', 'TIME', 'auto', '23456'):
            result = subprocess.run(['bash', str(REPO / 'scripts/run_mine.sh'),
                                     '--dry-run', '--batch', '--transport', 'geant4',
                                     f'-RANDOM={value}', '-N=1'], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            seed = int(re.search(r'Preflight MINE valido; seed=(\d+)', result.stdout)[1])
            self.assertTrue(1 <= seed <= 2147483646)
            if value == '23456':
                self.assertEqual(seed, 23456)
            self.assertIn(f'-RANDOM={seed}', result.stdout)
        result = subprocess.run(['bash', str(REPO / 'scripts/run_mine.sh'),
                                 '--dry-run', '-RANDOM=-1'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn('Invalid MINE seed', result.stderr)

    def test_invalid_progress_interval_needs_no_docker(self):
        for value in ('0', '-1', 'nan', 'inf', '1e999', 'invalid', ''):
            with self.subTest(value=value):
                result = subprocess.run(['bash', str(REPO / 'scripts/run_mine.sh'),
                                         '--progress-seconds', value], capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn('must be a finite number > 0', result.stderr)
        result = subprocess.run(['bash', str(REPO / 'scripts/run_mine.sh'),
                                 '--progress-seconds'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn('Missing --progress-seconds', result.stderr)

    def test_launcher_help_and_incompatible_flags_need_no_docker(self):
        for options, expected in [(['--help'], 0),
                                  (['--transport', 'unknown'], 2),
                                  (['--transport', 'geant4', '--response', '/missing/table.dat'], 2),
                                  (['--transport', 'tabulated', '--optical-config', '/missing/model.conf'], 2)]:
            with self.subTest(options=options):
                result = subprocess.run(['bash', str(REPO / 'scripts/run_mine.sh'), *options],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, expected, result.stderr)
                self.assertNotIn('docker.sock', result.stderr)

    def test_geant4_launcher_stages_config_without_table(self):
        # Exercise the complete host preflight/archive flow with an inert Docker
        # receiver. Keep logs and all temporary files outside the real output tree.
        with tempfile.TemporaryDirectory() as temp:
            repo = Path(temp) / 'repo'
            scripts = repo / 'scripts'
            scripts.mkdir(parents=True)
            for name in ('run_mine.sh', 'mine_launch_card.py', 'mine_random_seed.py'):
                shutil.copyfile(REPO / 'scripts' / name, scripts / name)
            experiment = repo / 'experiments/mine'
            experiment.mkdir(parents=True)
            for pattern in ('mine.gcard', 'mine__*_corner_fan.txt'):
                for source in (REPO / 'experiments/mine').glob(pattern):
                    shutil.copyfile(source, experiment / source.name)
            config = repo / 'test.conf'
            config.write_text('model=monochromatic_effective\nwlsAbsorptionLengthMm=0.1\n')
            tools = Path(temp) / 'bin'
            tools.mkdir()
            docker = tools / 'docker'
            docker.write_text('#!/usr/bin/env python3\n'
                              'import json,sys,tarfile\n'
                              'with tarfile.open(fileobj=sys.stdin.buffer,mode="r|") as archive:\n'
                              ' names=[member.name for member in archive]\n'
                              'print("MOCK_DOCKER="+json.dumps({"files":names,"arguments":sys.argv[1:]}))\n')
            docker.chmod(0o755)
            env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'],
                       MINE_OPTICAL_RESPONSE='/missing/response.dat')
            result = subprocess.run(['bash', str(scripts / 'run_mine.sh'), '--batch',
                                     '--transport', 'geant4', '--optical-config', str(config), '-N=1'],
                                    capture_output=True, text=True, env=env)
            self.assertEqual(result.returncode, 0, result.stderr)
            manifest = json.loads(next(line.removeprefix('MOCK_DOCKER=') for line in result.stdout.splitlines()
                                       if line.startswith('MOCK_DOCKER=')))
            self.assertEqual(set(manifest['files']), {'launch.gcard', 'geant4_optics.conf'})
            self.assertIn('geant4', manifest['arguments'])
            self.assertIn('-N=1', manifest['arguments'])


if __name__ == '__main__':
    unittest.main()
