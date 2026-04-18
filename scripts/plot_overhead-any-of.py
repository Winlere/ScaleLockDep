# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Visualize overhead benchmark results from an any-lock experiment log.

Usage:
    uv run scripts/plot_overhead-any-of.py [LOG_FILE]

Defaults to logs/exp_18_mar_anylock4.txt if no argument is given.
"""

import os
import sys

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


# --- Parsing ---

_DATA_COLUMNS = {"threads", "locks", "iters", "wall_ns", "total_ops", "ops_per_sec"}


def parse_tables(path):
    """Parse raw-data tables from an experiment log.

    Detects tables by finding a line whose whitespace-split tokens are all
    known column names, then reads the preceding non-blank line as the table
    title and subsequent numeric lines as data rows.

    Returns a dict mapping each table title to a list of row-dicts
    with int-typed values keyed by column name.
    """
    with open(path) as f:
        lines = f.readlines()

    tables = {}
    i = 0
    while i < len(lines):
        tokens = lines[i].split()
        if tokens and all(t in _DATA_COLUMNS for t in tokens):
            title = ""
            for j in range(i - 1, -1, -1):
                if lines[j].strip():
                    title = lines[j].strip()
                    break
            cols = tokens
            i += 1
            rows = []
            while i < len(lines) and lines[i].strip():
                vals = lines[i].split()
                rows.append({c: int(v) for c, v in zip(cols, vals)})
                i += 1
            tables[title] = rows
        i += 1
    return tables


def column(table, name):
    return [row[name] for row in table]


# --- Helpers ---

def to_millions(vals):
    return [v / 1e6 for v in vals]


def annotate(ax, xs, ys, fmt="{:.1f}", color="black", offset=(0, 6)):
    """Place a text label above each data point."""
    for i, (x, y) in enumerate(zip(xs, ys)):
        ha = "right" if i == len(xs) - 1 else "center"
        dx = offset[0] - 4 if i == len(xs) - 1 else offset[0]
        ax.annotate(fmt.format(y), (x, y), textcoords="offset points",
                    xytext=(dx, offset[1]), ha=ha, va="bottom",
                    fontsize=7, color=color)


# --- Load data ---

log_file = sys.argv[1] if len(sys.argv) > 1 else "logs/exp_18_mar_anylock4.txt"
tables = parse_tables(log_file)

# Find baseline and lockdep tables by suffix
base_tbl = None
lock_tbl = None
base_title = ""
for key in tables:
    lower = key.lower()
    if "baseline" in lower:
        base_tbl = tables[key]
        base_title = key
    elif "lockdep" in lower:
        lock_tbl = tables[key]

if base_tbl is None or lock_tbl is None:
    print(f"Error: could not find baseline/lockdep tables in {log_file}")
    print(f"Found tables: {list(tables.keys())}")
    sys.exit(1)

# Derive scenario name from baseline title (strip " — baseline")
scenario = base_title.rsplit("—", 1)[0].strip()

threads = column(base_tbl, "threads")
baseline = column(base_tbl, "ops_per_sec")
lockdep = column(lock_tbl, "ops_per_sec")
overhead = [b / l for b, l in zip(baseline, lockdep)]

# --- Style ---
plt.rcParams.update({
    "font.family": "serif",
    "font.size": 10,
    "axes.linewidth": 0.8,
    "axes.grid": True,
    "grid.alpha": 0.3,
    "grid.linewidth": 0.5,
    "lines.linewidth": 1.8,
    "lines.markersize": 5,
    "figure.facecolor": "white",
})

COLOR_BASELINE = "#2d2d2d"
COLOR_LOCKDEP = "#b04040"
COLOR_OVERHEAD = "#4a7ab5"

fig, axes = plt.subplots(1, 2, figsize=(8.5, 3.8))

# --- Left: Throughput ---
ax = axes[0]
base_m = to_millions(baseline)
lock_m = to_millions(lockdep)
ax.plot(threads, base_m, "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, lock_m, "s-", color=COLOR_LOCKDEP, label="lockdep")
annotate(ax, threads, base_m, color=COLOR_BASELINE)
annotate(ax, threads, lock_m, color=COLOR_LOCKDEP)
ax.set_xscale("log", base=2)
ax.set_ylabel("ops/s (millions)")
ax.set_xlabel("threads")
ax.set_title("Throughput")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.08)

# --- Right: Overhead factor ---
ax = axes[1]
ax.plot(threads, overhead, "D-", color=COLOR_OVERHEAD)
annotate(ax, threads, overhead, fmt="{:.1f}×", color=COLOR_OVERHEAD)
ax.set_xscale("log", base=2)
ax.set_ylabel("overhead (×)")
ax.set_xlabel("threads")
ax.set_title("Overhead factor")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.08)

fig.suptitle(f"{scenario} — Overhead", fontsize=13, fontweight="bold", y=0.98)
fig.tight_layout(rect=[0, 0, 1, 0.93])

os.makedirs("plots", exist_ok=True)
basename = os.path.splitext(os.path.basename(log_file))[0]
output = f"plots/{basename}.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
