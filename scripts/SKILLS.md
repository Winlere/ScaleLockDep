# scripts/

End-to-end reproduce scripts and plotting helpers for the five
benchmark experiments described in `logs/SKILL.md`.

## Quick start

From the project root:

```bash
# All five experiments. Logs go to logs/, PDFs go to plots/.
./scripts/reproduce_all.sh

# Just one experiment.
./scripts/reproduce_overhead.sh
./scripts/reproduce_cslen.sh
./scripts/reproduce_latency.sh
./scripts/reproduce_nesting.sh
./scripts/reproduce_pedges.sh
```

Each script must be run from the project root or from `scripts/` —
they `cd "$(dirname "$0")/.."` internally, so either works.

## What gets written

Every reproduce script produces two files, suffixed by run identifier:

```
logs/exp_<SUFFIX>_<name>.txt    — full log (Settings + Parameters + Raw Data)
plots/exp_<SUFFIX>_<name>.pdf   — rendered figure
```

`<SUFFIX>` defaults to today's date in `<DD>_<mon>` form (e.g. `06_may`).
Override it by passing an argument:

```bash
./scripts/reproduce_overhead.sh ablation_v2     # writes ..._ablation_v2_overhead.{txt,pdf}
./scripts/reproduce_all.sh        2026_06_15    # same suffix to all five
```

If a log already exists at the target path it is overwritten without
prompting. Pick a fresh suffix to keep multiple runs side-by-side.

## What each script does

| Script                       | Experiment                       | Conditions             | Runs/cell |
|------------------------------|----------------------------------|------------------------|-----------|
| `reproduce_overhead.sh`      | High/low contention sweep        | baseline, global, rb   | 1         |
| `reproduce_cslen.sh`         | Critical section length sweep    | baseline, global, rb   | 1         |
| `reproduce_latency.sh`       | Per-op latency sweep             | baseline, global, rb   | 1         |
| `reproduce_nesting.sh`       | 3-deep vs 40-deep nesting        | baseline, global, rb   | 5         |
| `reproduce_pedges.sh`        | Disjoint-lock graph construction | baseline, global, rb   | 3         |

Each script:

1. `cd`s to the project root.
2. Sources `_repro_common.sh` and captures machine + software metadata
   (OS, CPU model, core count, memory, git commit SHA).
3. Runs `make build` (idempotent — no-op when up to date).
4. Runs every (condition × parameter) cell in sequence.
5. Streams a SKILL.md-format log to the target path via `tee`, so you
   can also watch progress on stdout.
6. Invokes the matching plot script to render the PDF.

Run the whole suite end-to-end in roughly 5–10 min on a 16-thread CPU
(the cslen sweep dominates: cs_ns=100000 takes ~75 s alone).

## Conditions

All five scripts run three conditions per cell, using `LD_PRELOAD` to
inject the lockdep hooks:

| Label             | Invocation                                                          |
|-------------------|---------------------------------------------------------------------|
| `baseline`        | benchmark binary directly, no preload                               |
| `lockdep global`  | `LOCKDEP_MODE=global LD_PRELOAD=./lockdep/liblockdep.so ...`        |
| `lockdep rb`      | `LOCKDEP_MODE=rb     LD_PRELOAD=./lockdep/liblockdep.so ...`        |

The label-to-prefix mapping lives in `_repro_common.sh` (`prefix_for`,
`label_for`).

## Plot scripts (standalone use)

Each reproduce script ends by calling its plot script. You can also run
the plot scripts on any existing log file:

```bash
uv run scripts/plot_overhead.py       logs/exp_06_may_overhead.txt
uv run scripts/plot_overhead-cslen.py logs/exp_06_may_cslen.txt
uv run scripts/plot_latency.py        logs/exp_06_may_latency.txt
uv run scripts/plot_nesting.py        logs/exp_06_may_nesting.txt
uv run scripts/plot_pedges.py         logs/exp_06_may_pedges.txt
```

Output goes to `plots/<basename>.pdf`. The plot scripts depend on
`matplotlib`; `uv run` resolves that from the inline PEP-723 metadata
at the top of each file, so no separate venv is needed.

The first three plot scripts handle both the legacy 2-condition
(baseline + lockdep) and the current 3-condition (baseline + global +
rb) log formats. They look up tables by substring match on the table
title, so any `baseline` / `global` / `rb` table title works.

## Log format requirements

The plot scripts parse the log by scanning for column-header rows, so a
custom log will re-plot cleanly as long as each data block has:

- a title line on its own (e.g. `lockdep global`, `Baseline (no LD_PRELOAD)`)
- the column-header row immediately below it
- one numeric data row per line, separated by whitespace or tabs
- a blank line ending the block

For pedges and nesting, the title must contain the substring
`baseline`, `global`, or `rb` (case-insensitive). For nesting, the
shallow/deep blocks must start with a line beginning `Shallow` or
`Deep`.

## `_repro_common.sh`

Shared helpers used by every reproduce script. Source this if you write
a new experiment script:

| Function              | Purpose                                            |
|-----------------------|----------------------------------------------------|
| `default_suffix`      | Today's date as `<DD>_<mon>`                       |
| `collect_metadata $1` | Set `SUFFIX`, `COMMIT`, `OS`, `CPU`, `CORES`, ...  |
| `write_preamble`      | Emit the standard Settings + Software sections     |
| `write_lockdep_limits`| Emit the standard lockdep-limits block             |
| `ensure_built`        | `make build` (no-op when up to date)               |
| `prefix_for $cond`    | LD_PRELOAD prefix for `baseline` / `global` / `rb` |
| `label_for $cond`     | Human-readable label for the same                  |

## Adding a new experiment

1. Write a new benchmark binary under `benchmarks/` and add it to
   `benchmarks/Makefile`.
2. Copy one of the existing reproduce scripts as a template; the
   shortest is `reproduce_latency.sh`. Replace the binary path,
   parameter sweep, and column header.
3. Write a plot script under `scripts/` that reads the log; copy from
   `plot_latency.py` if your output is a thread-count sweep, or
   `plot_pedges.py` if it's a (threads, depth) grid.
4. Add the new experiment to `reproduce_all.sh`'s for-loop and to the
   table above.
5. Document the experiment's purpose, parameters, and metric in
   `logs/SKILL.md` so the methodology stays in one place.

## Caveats

- The reproduce scripts emit the Raw Data section but do not write the
  hand-curated "Key Findings" / "What the Experiment Reveals" prose.
  Those are interpretation, not measurement, and are added to the log
  by hand after a run.
- Run-to-run variance on the i7-11700 reference machine is ~5% for
  uncontended cells and ~10% for the cs_ns sweep. Re-run a cell if a
  number looks like an outlier — do not edit it.
- Do not run reproduce scripts in parallel. They contend for CPU and
  pollute each other's measurements. `reproduce_all.sh` runs them
  serially for this reason.
