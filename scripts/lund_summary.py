#!/usr/bin/env python3
"""Validate GEMC LUND structure and print: number_of_events max_particles.

Read-only and streaming; the full particle file is never held in memory.
The second particle column is accepted as a general numeric lifetime value.
"""
import argparse
import math
import sys


def _numbers(line, line_number, minimum, kind):
    tokens = line.split()
    if len(tokens) < minimum:
        raise ValueError(
            f"line {line_number}: {kind} requires at least {minimum} numeric columns; "
            f"found {len(tokens)}"
        )
    values = []
    for column, token in enumerate(tokens, 1):
        try:
            value = float(token)
        except ValueError as error:
            raise ValueError(
                f"line {line_number}, column {column}: non-numeric {kind} value {token!r}"
            ) from error
        if not math.isfinite(value):
            raise ValueError(
                f"line {line_number}, column {column}: non-finite {kind} value {token!r}"
            )
        values.append(value)
    return values


def summarize(path):
    events = 0
    maximum_particles = 0
    line_number = 0
    with open(path, encoding="utf-8") as stream:
        while True:
            header = stream.readline()
            if header == "":
                break
            line_number += 1
            if not header.strip():
                # GEMC does not skip empty event headers. Permit them only after
                # every complete event, at the very end of the input file.
                first_blank = line_number
                for trailing in stream:
                    line_number += 1
                    if trailing.strip():
                        raise ValueError(
                            f"line {first_blank}: blank header before data at line "
                            f"{line_number}; blank lines are permitted only at end of file"
                        )
                break
            values = _numbers(header, line_number, 10, "event header")
            count = values[0]
            if not count.is_integer() or count < 0:
                raise ValueError(
                    f"line {line_number}, column 1: particle count must be a nonnegative integer"
                )
            count = int(count)
            for particle in range(count):
                row = stream.readline()
                if row == "":
                    raise ValueError(
                        f"line {line_number + 1}: truncated event {events + 1}; "
                        f"expected particle row {particle + 1} of {count}"
                    )
                line_number += 1
                if not row.strip():
                    raise ValueError(
                        f"line {line_number}: blank particle row {particle + 1} of {count} "
                        f"in event {events + 1}"
                    )
                fields = _numbers(row, line_number, 14, "particle row")
                for index, label in ((0, "index"), (2, "type"), (3, "PID"),
                                     (4, "parent"), (5, "daughter")):
                    if not fields[index].is_integer():
                        raise ValueError(
                            f"line {line_number}, column {index + 1}: particle {label} must be an integer"
                        )
            events += 1
            maximum_particles = max(maximum_particles, count)
    if not events:
        raise ValueError("input contains no complete LUND events")
    return events, maximum_particles


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Read-only streaming LUND preflight; prints event count and maximum particle count."
    )
    parser.add_argument("input", help="existing LUND file")
    args = parser.parse_args(argv)
    try:
        events, maximum_particles = summarize(args.input)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"Invalid LUND {args.input}: {error}", file=sys.stderr)
        return 2
    print(f"{events} {maximum_particles}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
