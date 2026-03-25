# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Visualize overhead vs critical section length from logs/exp_24_mar_cslen.txt.

Usage:
    uv run scripts/plot_overhead-cslen.py [LOG_FILE]

Defaults to logs/exp_24_mar_cslen.txt if no argument is given.
"""

import os
import sys

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


# --- Parsing ---

_DATA_COLUMNS = {"threads", "locks", "iters", "wall_ns", "total_ops", "ops_per_sec", "cs_ns"}


def parse_tables(path):
    """Parse raw-data tables from an experiment log."""
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


def annotate(ax, xs, ys, fmt="{:.1f}", color="black", offset=(0, 6), fontsize=6.5):
    """Place a text label above each data point."""
    for i, (x, y) in enumerate(zip(xs, ys)):
        ha = "right" if i == len(xs) - 1 else "center"
        dx = offset[0] - 4 if i == len(xs) - 1 else offset[0]
        ax.annotate(fmt.format(y), (x, y), textcoords="offset points",
                    xytext=(dx, offset[1]), ha=ha, va="bottom",
                    fontsize=fontsize, color=color)


# --- Load data ---

log_file = sys.argv[1] if len(sys.argv) > 1 else "logs/exp_24_mar_cslen.txt"
tables = parse_tables(log_file)

base_tbl = None
lock_tbl = None
for key in tables:
    lower = key.lower()
    if "baseline" in lower:
        base_tbl = tables[key]
    elif "lockdep" in lower:
        lock_tbl = tables[key]

if base_tbl is None or lock_tbl is None:
    print(f"Error: could not find baseline/lockdep tables in {log_file}")
    print(f"Found tables: {list(tables.keys())}")
    sys.exit(1)

cs_ns = column(base_tbl, "cs_ns")
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

fig, axes = plt.subplots(1, 2, figsize=(10, 4.2))

# --- Left: Throughput vs CS length ---
ax = axes[0]
base_m = to_millions(baseline)
lock_m = to_millions(lockdep)
ax.plot(cs_ns, base_m, "o-", color=COLOR_BASELINE, label="baseline", markersize=4)
ax.plot(cs_ns, lock_m, "s-", color=COLOR_LOCKDEP, label="lockdep", markersize=4)
ax.set_xscale("symlog", linthresh=10)
ax.set_yscale("log")
ax.set_ylabel("ops/s (millions)")
ax.set_xlabel("critical section length (ns)")
ax.set_title("Throughput")
ax.legend(frameon=False)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Right: Overhead factor vs CS length ---
ax = axes[1]
ax.plot(cs_ns, overhead, "D-", color=COLOR_OVERHEAD, markersize=4)
annotate(ax, cs_ns, overhead, fmt="{:.1f}×", color=COLOR_OVERHEAD)
ax.set_xscale("symlog", linthresh=10)
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_ylabel("overhead (×)")
ax.set_xlabel("critical section length (ns)")
ax.set_title("Overhead factor")
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.08)

fig.suptitle("Overhead vs Critical Section Length (8 threads, 1 lock)",
             fontsize=13, fontweight="bold", y=0.98)
fig.tight_layout(rect=[0, 0, 1, 0.93])

os.makedirs("plots", exist_ok=True)
basename = os.path.splitext(os.path.basename(log_file))[0]
output = f"plots/{basename}.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
