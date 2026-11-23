# Groove filling: refractive indices

Material keys: `air`, `gel`, `glue`; sources and assumptions in [provenance.json](provenance.json) and [reference/grooves.md](../../../../../reference/grooves.md).

The CSVs sample the implemented index formula, **not new measured spectra**. Air uses the NIST typical ambient index 1.000277; glue uses Eljen EJ-500's nominal 1.57; these are constant-index approximations. Gel uses the manufacturer Cauchy relation for Thorlabs G608N3: `n(lambda)=1.4338+10520/lambda_nm^2`. Its extension below 402 nm is unvalidated; the same limitation affects UV photons in the FNAL fluorescence profile. Commercial gel and glue composition is not inferred from optical index: charged-particle transport uses explicitly approximate compositions.

[Air PNG](air_refractive_index.png), [gel PNG](gel_refractive_index.png), [glue PNG](glue_refractive_index.png), [comparison PNG](groove_refractive_indices.png). Same 1800×1200 px and 250–900 nm horizontal range as other reference plots; index is dimensionless with common vertical limits 1–1.65. Regenerate with `python3 scripts/plot_mine_references.py`.
