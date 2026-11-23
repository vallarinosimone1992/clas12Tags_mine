# Mini-panel numerical validation — 23 November 2026

[Geometry, physics and limitations](../../mini_panel_spiral.md).
[Machine-readable results](final_checks.json), [source/config/binary hashes](source-config-manifest.json).

Geant4 10.7.4: portable model PASS; 434 material-navigation points and 3472
near-wall containment probes PASS. Of 100 optical coupling trials, 99 take
the expected prompt route; one is Fresnel-reflected. Both fiber ends and direct
plastic-to-sensor light are tested independently.

Two complete 10-GeV muon events, same seed 20261123 and centered beam, pass
read-only HIPO checks. Each transports 72/66 arrivals to sensor0/1 and records
2744 WLS absorptions. Ideal detects 72/66; Hamamatsu detects 34/31. Arrival-fiber
counts are 67/70 plus one direct photon. Time range 3.32171–73.5304ns; no artificial
50ns cutoff. One central event costs about126 seconds on the current Docker
host. These are smoke tests, not efficiency statistics or a timing resolution.

Full HIPO files remain under `build/validation/mini_panel_spiral_20261122/`,
`mini_ideal_final.hipo` and `mini_hamamatsu_final.hipo`. The same directory's
preliminary failed/debug files are not validation evidence and must not be used
for physics comparisons. The final outputs use zero-start torus rotations.

Regression response and old corner-fan gel groove tests PASS. Finite-rise
material law tested separately in `build/validation/mini_physics_20261122/`.
The final binary additionally rejects invalid zero gaps/SiPM dimensions; this
input guard does not alter either smoke-test geometry or response.

Reproduce with `scripts/test_mine_mini_panel.sh --integration 1`. The script compiles standalone tests, requires
an already rebuilt GEMC, and writes a fresh artifact directory. The existing
corner-fan overview analysis is explicitly not certified for these files.
