#!/usr/bin/env python3
"""Compare complete arrival distributions from the isolated G4 fiber benchmark.

The CSVs contain arrivals only: --generated supplies the denominator from each
benchmark invocation. Confidence intervals use exact binomial tail inversion
(Clopper--Pearson), evaluated numerically without scipy. KS values are empirical
distances, not independent-sample p-values. Files are never overwritten.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


def _lower_binomial_tail(k, n, probability):
    """P(X<=k), used only when k < n*p so successive terms decrease."""
    log_term = (math.lgamma(n + 1) - math.lgamma(k + 1) - math.lgamma(n - k + 1)
                + k * math.log(probability) + (n - k) * math.log1p(-probability))
    term = math.exp(log_term)
    total = term
    ratio = (1 - probability) / probability
    for j in range(k, 0, -1):
        term *= j / (n - j + 1) * ratio
        total += term
        if term <= total * 1e-15:
            break
    return min(1.0, total)


def binomial_cdf(k, n, probability):
    if k < 0:
        return 0.0
    if k >= n or probability == 0:
        return 1.0
    if probability == 1:
        return 0.0
    if k < n * probability:
        return _lower_binomial_tail(k, n, probability)
    return max(0.0, 1 - _lower_binomial_tail(n - k - 1, n, 1 - probability))


def _invert_binomial_cdf(k, n, target):
    # CDF decreases monotonically with p. Eighty iterations reach floating-point
    # precision; this numerical tolerance does not censor transported photons.
    lower, upper = 0.0, 1.0
    for _ in range(80):
        middle = (lower + upper) / 2
        if binomial_cdf(k, n, middle) > target:
            lower = middle
        else:
            upper = middle
    return (lower + upper) / 2


def binomial_summary(successes, generated, confidence):
    if generated < 1 or not 0 <= successes <= generated:
        raise ValueError('Invalid binomial numerator or denominator')
    if not 0 < confidence < 1:
        raise ValueError('Confidence level must lie strictly between zero and one')
    probability = successes / generated
    tail = (1 - confidence) / 2
    lower = 0.0 if successes == 0 else _invert_binomial_cdf(successes - 1, generated, 1 - tail)
    upper = 1.0 if successes == generated else _invert_binomial_cdf(successes, generated, tail)
    return {
        'arrived': successes,
        'generated': generated,
        'probability': probability,
        'binomial_standard_error': math.sqrt(probability * (1 - probability) / generated),
        'exact_binomial_interval': {
            'method': 'Clopper-Pearson; binomial-tail inversion in floating point',
            'confidence_level': confidence,
            'lower': lower, 'upper': upper,
            'error_minus': probability - lower, 'error_plus': upper - probability,
        },
    }


def quantile(ordered, probability):
    if not ordered:
        return None
    index = (len(ordered) - 1) * probability
    first = math.floor(index)
    last = math.ceil(index)
    return ordered[first] + (ordered[last] - ordered[first]) * (index - first)


def distribution(values):
    ordered = sorted(values)
    if not ordered:
        return {'count': 0, 'mean': None, 'sample_standard_deviation': None,
                'minimum': None, 'maximum': None,
                'q10': None, 'q50': None, 'q90': None, 'q99': None, 'empirical_cdf': []}
    count = len(ordered)
    mean = math.fsum(ordered) / count
    cdf = []
    for i, value in enumerate(ordered, 1):
        if i == count or value != ordered[i]:
            cdf.append([value, i / count])
    return {
        'count': count, 'mean': mean,
        'sample_standard_deviation': (math.sqrt(math.fsum((x - mean) ** 2 for x in ordered) / (count - 1))
                                      if count > 1 else None),
        'minimum': ordered[0], 'maximum': ordered[-1],
        **{f'q{int(100 * q)}': quantile(ordered, q) for q in (.1, .5, .9, .99)},
        'empirical_cdf': cdf,
    }


def empirical_ks(first, second):
    """sup_x |F_first(x)-F_second(x)| with tied values handled jointly."""
    if not first or not second:
        return None
    first, second = sorted(first), sorted(second)
    i = j = 0
    distance = 0.0
    while i < len(first) or j < len(second):
        value = min(first[i] if i < len(first) else math.inf,
                    second[j] if j < len(second) else math.inf)
        while i < len(first) and first[i] <= value:
            i += 1
        while j < len(second) and second[j] <= value:
            j += 1
        distance = max(distance, abs(i / len(first) - j / len(second)))
    return distance


def read_arrivals(path, generated):
    fields = ('event', 'sensor', 'path_mm', 'time_ns', 'wavelength_nm',
              'core_path_mm', 'cladding_path_mm')
    rows = []
    seen = set()
    with path.open(newline='') as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None or not set(fields).issubset(reader.fieldnames):
            raise ValueError(f'{path}: not a geant4_fiber_benchmark CSV')
        for row_number, raw in enumerate(reader, 2):
            try:
                row = {key: int(raw[key]) if key in ('event', 'sensor') else float(raw[key]) for key in fields}
            except (ValueError, TypeError) as error:
                raise ValueError(f'{path}:{row_number}: invalid arrival row') from error
            if row['sensor'] not in (0, 1) or not 0 <= row['event'] < generated or row['event'] in seen:
                raise ValueError(f'{path}:{row_number}: invalid sensor, event ID, or duplicate photon')
            for field in fields[2:]:
                if not math.isfinite(row[field]) or row[field] < 0:
                    raise ValueError(f'{path}:{row_number}: invalid {field}')
            if row['wavelength_nm'] == 0:
                raise ValueError(f'{path}:{row_number}: wavelength must be positive')
            if not math.isclose(row['path_mm'], row['core_path_mm'] + row['cladding_path_mm'],
                                rel_tol=1e-7, abs_tol=1e-7):
                raise ValueError(f'{path}:{row_number}: path components do not sum to total path')
            seen.add(row['event'])
            rows.append(row)
    return rows


def compare(straight, bend, generated, confidence, paired=False):
    summary = {
        'generated_per_geometry': generated,
        'arrival_definition': 'EndCore only; end-cladding/side escapes are excluded from CSV arrivals',
        'units': {'time': 'ns', 'path': 'mm', 'wavelength': 'nm'},
        'quantile_convention': 'Linear interpolation at (n-1)*q (type 7)',
        'ks_convention': 'Empirical CDF distance only; no independent-sample significance claim',
        'paired_launches_asserted': paired,
        'straight': {}, 'bend': {}, 'comparison': {},
    }
    for sensor in (0, 1):
        selected = {mode: [row for row in rows if row['sensor'] == sensor]
                    for mode, rows in (('straight', straight), ('bend', bend))}
        for mode, rows in selected.items():
            summary[mode][str(sensor)] = {
                **binomial_summary(len(rows), generated, confidence),
                'time_ns': distribution([row['time_ns'] for row in rows]),
                'path_mm': distribution([row['path_mm'] for row in rows]),
            }
        first, second = selected['straight'], selected['bend']
        ratio = len(second) / len(first) if first else None
        result = {
            'bend_to_straight_probability_ratio': ratio,
            # A negative value is an observed enhancement, not silently clamped.
            'fractional_reduction_relative_to_straight': 1 - ratio if ratio is not None else None,
            'absolute_probability_difference_bend_minus_straight': (len(second) - len(first)) / generated,
            'time_ks_distance': empirical_ks([r['time_ns'] for r in first], [r['time_ns'] for r in second]),
            'path_ks_distance': empirical_ks([r['path_mm'] for r in first], [r['path_mm'] for r in second]),
            'ratio_standard_error': None,
            'ratio_uncertainty_note': 'Not assigned without explicit matched-launch assertion',
        }
        if paired:
            both = len({r['event'] for r in first} & {r['event'] for r in second})
            result['same_event_arrivals_in_both'] = both
            if first and generated > 1:
                # Sample variance of Y-r*X divided by n*p_X^2 includes the
                # covariance of paired Bernoulli outcomes. It is a delta-method
                # estimate, not an exact ratio confidence interval.
                numerator = len(second) + ratio * ratio * len(first) - 2 * ratio * both
                variance = max(0.0, numerator) / (generated * (generated - 1) * (len(first) / generated) ** 2)
                result['ratio_standard_error'] = math.sqrt(variance)
                result['ratio_uncertainty_note'] = 'Paired Bernoulli covariance; first-order delta-method estimate'
        summary['comparison'][str(sensor)] = result
    return summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--straight', type=Path, required=True)
    parser.add_argument('--bend', type=Path, required=True)
    parser.add_argument('--generated', type=int, required=True, help='Photons generated in EACH input run')
    parser.add_argument('--confidence', type=float, default=.6826894921370859,
                        help='Central exact-binomial confidence level (default one-Gaussian-sigma coverage)')
    parser.add_argument('--paired-launches', action='store_true',
                        help='Assert the same N source launches/event IDs; enables paired ratio uncertainty')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    if args.generated < 1 or not 0 < args.confidence < 1:
        parser.error('--generated must be positive and --confidence must lie between zero and one')
    if args.output.exists():
        parser.error(f'Refusing to overwrite existing output: {args.output}')
    try:
        straight = read_arrivals(args.straight, args.generated)
        bend = read_arrivals(args.bend, args.generated)
        result = compare(straight, bend, args.generated, args.confidence, args.paired_launches)
        result['inputs'] = {key: {'path': str(path.resolve()), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                            for key, path in (('straight', args.straight), ('bend', args.bend))}
        # Exclusive creation also protects against a competing writer after the
        # preflight. A failed comparison never truncates an earlier artifact.
        with args.output.open('x') as output:
            json.dump(result, output, indent=2, sort_keys=True, allow_nan=False)
            output.write('\n')
    except (ValueError, OSError) as error:
        parser.exit(2, f'{error}\n')
    for sensor in ('0', '1'):
        first, second = result['straight'][sensor], result['bend'][sensor]
        comparison = result['comparison'][sensor]
        print(f'sensor {sensor}: straight={first["arrived"]}/{args.generated}, '
              f'bend={second["arrived"]}/{args.generated}, '
              f'ratio={comparison["bend_to_straight_probability_ratio"]}, '
              f'time_KS={comparison["time_ks_distance"]}, path_KS={comparison["path_ks_distance"]}')
    print(f'Full distributions and exact-binomial intervals: {args.output}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
