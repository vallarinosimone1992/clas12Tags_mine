#!/usr/bin/env python3
"""Positive signed-32-bit GEMC seeds; OS randomness, no ROOT dependency."""
import re
import secrets
import sys

MAX_SEED = 2147483646


def resolve_seed(value='auto'):
    text = str(value).strip()
    if text.lower() in ('auto', 'random', 'time') or re.fullmatch(r'0+', text):
        return secrets.randbelow(MAX_SEED) + 1
    if not re.fullmatch(r'[0-9]+', text) or not 1 <= int(text) <= MAX_SEED:
        raise ValueError(f'RANDOM must be auto/TIME/0 or an integer in 1..{MAX_SEED}')
    return int(text)


def unique_seeds(count, base=0):
    if count < 1 or count > MAX_SEED:
        raise ValueError('Invalid number of independent seeds')
    if base:
        if base < 1 or base + count - 1 > MAX_SEED:
            raise ValueError(f'seed-base + runs - 1 must be <= {MAX_SEED}')
        return list(range(base, base + count))
    result, used = [], set()
    while len(result) < count:
        seed = resolve_seed()
        if seed not in used:
            used.add(seed)
            result.append(seed)
    return result


if __name__ == '__main__':
    try:
        print(resolve_seed(sys.argv[1] if len(sys.argv) > 1 else 'auto'))
    except ValueError as error:
        print(f'Invalid MINE seed: {error}', file=sys.stderr)
        sys.exit(2)
