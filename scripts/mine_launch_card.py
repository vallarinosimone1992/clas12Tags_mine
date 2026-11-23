#!/usr/bin/env python3
"""Build an ephemeral MINE gcard after checking transport/variation compatibility."""
import argparse
from pathlib import Path
import shlex
import sys
import xml.etree.ElementTree as ET

# Only variations backed by both the MINE hit process and installed TEXT files.
LAYOUTS = {
    'diagonal_corner_fan': 'corner_fan',
    'parallel_fan_in': 'parallel_fan_in',
    'compact_fan_in': 'parallel_fan_in',
    'triple_serpentine': 'triple_serpentine',
    'panel_double_serpentine': 'panel_double_serpentine',
}
ALIASES = {'diagonal_corner_fan': 'corner_fan', 'compact_fan_in': 'parallel_fan_in'}
EXPLICIT_LAYOUTS = {'corner_fan', 'mini_panel_spiral'}


def metadata(path):
    with path.open() as stream:
        if stream.readline().strip() != 'MINE_OPTICAL_RESPONSE 1':
            raise ValueError('Unsupported optical response header')
        marker, count = stream.readline().split()
        if marker != 'META' or not 0 < int(count) < 10000:
            raise ValueError('Invalid optical metadata header')
        tokens = shlex.shlex(stream, posix=True)
        tokens.whitespace_split = True
        tokens.commenters = ''
        result = {}
        for _ in range(int(count)):
            key, value = tokens.get_token(), tokens.get_token()
            if key is None or value is None or key in result:
                raise ValueError('Invalid or duplicated optical metadata')
            result[key] = value
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ('repo', 'card', 'geometry', 'output'):
        parser.add_argument('--' + flag, required=True)
    parser.add_argument('--transport', choices=('tabulated', 'geant4'), default='tabulated')
    parser.add_argument('--response')
    parser.add_argument('--optical-config')
    parser.add_argument('--default-card', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    repo = Path(args.repo).resolve()
    card = Path(args.card)
    if not card.is_absolute():
        card = repo / card
    requested = ALIASES.get(args.geometry, args.geometry)
    if requested != 'auto' and requested not in set(LAYOUTS.values()) | EXPLICIT_LAYOUTS:
        raise ValueError(f'Geometry {args.geometry!r} is not implemented/available in this MINE launcher')
    if args.transport == 'geant4':
        if args.response:
            raise ValueError('--response is only compatible with --transport tabulated')
        if requested != 'auto' and requested not in EXPLICIT_LAYOUTS:
            raise ValueError('Geant4 optical transport currently supports only corner_fan or mini_panel_spiral, single cladding and two sensors')
        if not args.optical_config or not Path(args.optical_config).is_file():
            raise ValueError('Geant4 transport requires a readable --optical-config file; no MC optical table is used')
        config = {}
        for row in Path(args.optical_config).read_text().splitlines():
            row = row.split('#', 1)[0].strip()
            if not row:
                continue
            if '=' not in row:
                raise ValueError('Optical config expects key=value')
            key, value = (part.strip() for part in row.split('=', 1))
            if key in config:
                raise ValueError(f'Duplicate optical config key: {key}')
            config[key] = value
        variation = config.get('geometry', 'corner_fan')
        if variation not in EXPLICIT_LAYOUTS:
            raise ValueError(f'Explicit geometry {variation!r} is not implemented')
        if requested not in ('auto', variation):
            raise ValueError(f'Requested geometry {requested!r}, but optical config describes {variation!r}; choose a matching --optical-config')
        profile = config.get('fiberModel', 'Y-11')
        if profile not in ('Y-11', 'Y11', 'YS-6', 'YS6', 'YS-4', 'YS4'):
            raise ValueError('Unsupported explicit fiberModel; use Y-11, YS-6 or YS-4')
        if config.get('sipmModel', 'ideal') not in ('ideal', 'hamamatsu'):
            raise ValueError('Unsupported sipmModel; use ideal or hamamatsu')
        if args.default_card and variation == 'mini_panel_spiral':
            card = repo / 'experiments/mine/mine_mini_panel_spiral.gcard'
    else:
        if args.optical_config:
            raise ValueError('--optical-config is only compatible with --transport geant4')
        if requested == 'mini_panel_spiral':
            raise ValueError('mini_panel_spiral is currently implemented only with --transport geant4')
        if not args.response:
            raise ValueError('Tabulated transport requires --response')
        meta = metadata(Path(args.response))
        table_geometry = meta.get('fiber_geometry')
        variation = LAYOUTS.get(table_geometry)
        if variation is None:
            raise ValueError(f'Geometry {table_geometry!r} has no supported MINE launch variation; refusing to substitute another layout')
        if requested not in ('auto', variation):
            raise ValueError(f'Requested geometry {requested!r}, but optical response describes {table_geometry!r}. Select the matching MC table with --response or change optical_response.dat symlink')
        if meta.get('gemc_contract_version') != '1' or meta.get('sensor_count') != '2':
            raise ValueError('Optical table is not exported for the current two-sensor GEMC hit process')
    folder = repo / 'experiments/mine'
    for kind in ('geometry', 'hit', 'parameters'):
        path = folder / f'mine__{kind}_{variation}.txt'
        if not path.is_file():
            raise ValueError(f'Missing TEXT variation file: {path}')
    parameters = (folder / f'mine__parameters_{variation}.txt').read_text()
    settings = {row.split('|')[0].strip(): row.split('|')[1].strip()
                for row in parameters.splitlines() if '|' in row}
    if args.transport == 'tabulated' and settings.get('mine_use_optical_table') != '1':
        raise ValueError('Geometry selection requires mine_use_optical_table=1; legacy response is not interchangeable with MC tables')
    tree = ET.parse(card)
    detectors = [d for d in tree.getroot().findall('detector')
                 if d.get('name') == 'experiments/mine/mine']
    if len(detectors) != 1 or detectors[0].get('factory') != 'TEXT':
        raise ValueError('Gcard must contain exactly one MINE TEXT detector')
    options = {v.get('name'): v.get('value') for v in tree.getroot().findall('option')}
    if options.get('HIT_PROCESS_LIST') != 'mine':
        raise ValueError('Gcard must select HIT_PROCESS_LIST=mine')
    detectors[0].set('variation', variation)
    tree.write(args.output, encoding='unicode', xml_declaration=False)
    if args.transport == 'tabulated':
        print(f'MINE transport: tabulated; geometry={table_geometry}; variation={variation}; fibers={meta.get("number_of_fibers")}; table={args.response}')
    else:
        geometry_name = 'diagonal_corner_fan' if variation == 'corner_fan' else variation
        print(f'MINE transport: geant4; geometry={geometry_name}; variation={variation}; profile={profile} single cladding; sipm={config.get("sipmModel", "ideal")}; sensors=2; config={args.optical_config}')



if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, ET.ParseError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(2)
