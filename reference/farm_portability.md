# Native JLab test workflow —23 November2026

User request: preserve this custom GEMC in a pinned submodule of the MINE
workspace and provide native ifarm build/test-job scripts. New umbrella docs,
commands and software messages are English. GEMC progress output is now English;
local/farm parsers retain compatibility with earlier Italian progress records.
This is a logging/packaging change; no physical model or random sampling changed.

The MINE parent `farm/` directory loads a site-selected module config, compiles
this private source with `scons OPT=1`, builds HIPO validators and records source,
binary, environment and checker hashes. Test planning snapshots the config/gcard,
pins runtime files, and submits nothing until an explicit `--submit-plan`.
The worker loads its own environment, runs one GEMC process with a stored seed,
validates event count/optical banks/config metadata, and only then publishes
HIPO and inputs.list. Jobs call the private executable; no Docker is used.

Official sources:
- https://github.com/gemc/clas12Tags#how-to-compile-the-source-code-at-jlab
- https://scicomp.jlab.org/docs/FarmUsersGuide
- https://scicomp.jlab.org/docs/swif2
- https://scicomp.jlab.org/cli/add-job.html
- https://scicomp.jlab.org/scicomp/ (October2026 stdout/stderr /farm_out policy)

Environment defaults reproduce the local Geant410.7.4 baseline but have not been
verified on the user's actual ifarm account. Versioned module availability,
Slurm account/certificate, output quota and the first real SWIF2 job still need
remote verification. Local stub-client tests do not establish farm access.
Generated HIPO/build/results are excluded from version control. Existing local
production data are preserved. Historical MC response tables must be generated
or supplied separately if using the old tabulated mode; native mode has no
runtime dependency on that external MC project.
