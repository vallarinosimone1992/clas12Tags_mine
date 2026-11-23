# Default optical config embedded in HIPO — 23 November 2026

**User input:** save the indication of which configuration file the analysis
should use by default. Active scope: explicitly tracked MINE Geant4 optics.
No physical properties or optical probabilities are changed.

New bank `MINE::config`, group4600/item4, contains the original optical-config
path and the complete UTF-8 text actually used from the launch snapshot.
The launcher passes the stable host path through `MINE_GEANT4_SOURCE_CONFIG`;
`MINE_GEANT4_CONFIG` remains the actual immutable snapshot consumed by GEMC.
Direct launches without the source-path variable store `MINE_GEANT4_CONFIG`
as their path hint. If this is temporary, the embedded text remains usable.

HIPO schemas in this installation support numerical columns. The bank therefore
has two byte columns, `sourceByte/B,configByte/B`. Row order is UTF-8 byte order;
the shorter column is padded with NUL bytes. Embedded NULs, empty fields and
payloads over1MiB are rejected. Encoding/decoding is shared in
`source/hitprocess/mine/mine_config_metadata.h`. Each written Geant4 event
contains the bank, so ordinary event selections retain the provenance. Repeated
config text is compressible. The metadata bank gets sufficient event-buffer
space before insertion. Tabulated output does not populate this optical bank.

At first write, the snapshot fingerprint is checked against the initialized
optical model to reject a changed config. The existing `MINE::dgt.opticalHash`
remains the model/config compatibility check. This is not a cryptographic
content signature; no arbitrary external code is executed from the stored path.

Analysis precedence:

1. Explicit `--optical-config`, requiring the matching HIPO fingerprint.
2. Embedded `MINE::config` text, also requiring the matching fingerprint;
   the original path is retained as provenance, not required to exist.
3. For old files, original logs, nearby snapshots and matching known presets.

An incompatible embedded config raises an error instead of silently replacing
it with a current preset. No regeneration of old HIPO data is necessary; only
new GEMC output receives the bank automatically. The old geometry limits remain:
this is the optical config, not a complete capture of all gcard overrides,
panel transforms, compiled spectral datasets or executable version.

## Numerical checks

- GEMC rebuilt successfully in Geant410.7.4 container; launcher14 tests PASS.
- HIPO byte-for-byte path/config round-trip PASS, including UTF-8 source path.
- Missing external source path: automatic analysis from the embedded text PASS.
- Changed embedded bulk-absorption value with unchanged HIPO hash: rejected.
- Old-HIPO geometry/log recovery and mixed-conditions regression PASS.
- One short production smoke: electron momentum0.0001GeV, vertex(0,0,0)mm,
  mini_panel_spiral, seed20261124. New HIPO read back byte-for-byte and analyzed
  automatically. This checks metadata integration, not detector efficiency.

Evidence: `build/validation/hipo_config_20261123/`, analysis verification
`build/verification/hipoconfig.x1NKo54J`,
`build/verification/autogeometry.s4XFrpqY` and
`build/verification/config_production` in the sibling project.
Reproducible synthetic round-trip test:
`../simulation_time_resolution/scripts/test_hipo_config.sh`.
