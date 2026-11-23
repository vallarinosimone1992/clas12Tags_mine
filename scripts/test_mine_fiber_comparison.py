#!/usr/bin/env python3
import csv
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
import json
import math
from pathlib import Path
import tempfile
import unittest

import compare_mine_fiber_benchmarks as comparison


class FiberComparisonTest(unittest.TestCase):
    def test_binomial_tails_against_finite_sum(self):
        for n in (1, 4, 10, 25):
            for probability in (.001, .1, .5, .9, .999):
                for k in range(n + 1):
                    exact = sum(math.comb(n, j) * probability ** j * (1 - probability) ** (n - j)
                                for j in range(k + 1))
                    self.assertAlmostEqual(comparison.binomial_cdf(k, n, probability), exact, places=12)

    def test_exact_interval_endpoints(self):
        empty = comparison.binomial_summary(0, 10, .95)['exact_binomial_interval']
        full = comparison.binomial_summary(10, 10, .95)['exact_binomial_interval']
        self.assertEqual(empty['lower'], 0)
        self.assertAlmostEqual(empty['upper'], 1 - .025 ** .1, places=12)
        self.assertAlmostEqual(full['lower'], .025 ** .1, places=12)
        self.assertEqual(full['upper'], 1)
        central = comparison.binomial_summary(5, 10, .95)['exact_binomial_interval']
        self.assertAlmostEqual(central['lower'], .18708602844739852, places=11)
        self.assertAlmostEqual(central['upper'], .8129139715526015, places=11)

    def test_ecdf_quantiles_and_ks_ties(self):
        result = comparison.distribution([0, 0, 10])
        self.assertEqual(result['empirical_cdf'], [[0, 2 / 3], [10, 1]])
        self.assertEqual(result['q50'], 0)
        self.assertAlmostEqual(result['q90'], 8)
        self.assertAlmostEqual(result['q99'], 9.8)
        self.assertAlmostEqual(comparison.empirical_ks([0, 0, 1], [0, 1, 1]), 1 / 3)
        self.assertEqual(comparison.empirical_ks([0], [1]), 1)
        self.assertEqual(comparison.empirical_ks([0, 1], [0, 1]), 0)
        self.assertIsNone(comparison.empirical_ks([], [1]))

    def test_paired_identical_probabilities_have_zero_ratio_error(self):
        rows = [{'event': i, 'sensor': i % 2, 'time_ns': 1., 'path_mm': 188.5} for i in range(6)]
        result = comparison.compare(rows, rows, 10, .95, paired=True)
        for sensor in ('0', '1'):
            self.assertEqual(result['comparison'][sensor]['bend_to_straight_probability_ratio'], 1)
            self.assertEqual(result['comparison'][sensor]['ratio_standard_error'], 0)
            self.assertEqual(result['comparison'][sensor]['time_ks_distance'], 0)

    def test_cli_and_refusal_to_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            fields = ['event', 'sensor', 'path_mm', 'time_ns', 'wavelength_nm', 'core_path_mm', 'cladding_path_mm']
            for name in ('straight', 'bend'):
                with (directory / f'{name}.csv').open('w', newline='') as stream:
                    writer = csv.writer(stream)
                    writer.writerow(fields)
                    writer.writerow([0, 0, 100, 100 * 1.59 / 299.792458, 476, 100, 0])
            output = directory / 'summary.json'
            args = ['--straight', str(directory / 'straight.csv'), '--bend', str(directory / 'bend.csv'),
                    '--generated', '10', '--output', str(output), '--paired-launches']
            with redirect_stdout(StringIO()):
                self.assertEqual(comparison.main(args), 0)
            result = json.loads(output.read_text())
            self.assertEqual(result['straight']['0']['arrived'], 1)
            self.assertEqual(result['bend']['1']['time_ns']['empirical_cdf'], [])
            previous = output.read_bytes()
            with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
                comparison.main(args)
            self.assertEqual(output.read_bytes(), previous)

    def test_reject_nonfinite_or_duplicate_arrivals(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'input.csv'
            header = 'event,sensor,path_mm,time_ns,wavelength_nm,core_path_mm,cladding_path_mm\n'
            for contents in ('0,0,1,nan,476,1,0\n', '0,0,1,1,476,1,0\n0,1,1,1,476,1,0\n'):
                path.write_text(header + contents)
                with self.assertRaises(ValueError):
                    comparison.read_arrivals(path, 10)


if __name__ == '__main__':
    unittest.main()
