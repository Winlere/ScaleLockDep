# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Compare overhead vs critical section length across two commits: previous
lockdep, current lockdep, and current baseline on the same axes.

Usage:
    uv run scripts/plot_cslen_compare.py <old_log> <new_log>
"""

import os
import sys

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


# --- Parsing ---

_DATA_COLUMNS = {"threads", "locks", "iters", "wall_ns", "total_ops", "ops_per_sec", "cs_ns"}


def parse_tables(path):
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


def to_millions(vals):
    return [v / 1e6 for v in vals]


def commit_label(path):
    with open(path) as f:
        for line in f:
            if line.startswith("Commit ID:"):
                return line.split(":", 1)[1].strip()
    return os.path.basename(path)


def find_tables(tables):
    base_tbl = lock_tbl = None
    for key in tables:
        lower = key.lower()
        if "baseline" in lower:
            base_tbl = tables[key]
        elif "lockdep" in lower:
            lock_tbl = tables[key]
    return base_tbl, lock_tbl


def annotate(ax, xs, ys, fmt="{:.1f}", color="black", offset=(0, 6), fontsize=6.5):
    for i, (x, y) in enumerate(zip(xs, ys)):
        ha = "right" if i == len(xs) - 1 else "center"
        dx = offset[0] - 4 if i == len(xs) - 1 else offset[0]
        ax.annotate(fmt.format(y), (x, y), textcoords="offset points",
                    xytext=(dx, offset[1]), ha=ha, va="bottom",
                    fontsize=fontsize, color=color)


# --- Load data ---

if len(sys.argv) < 3:
    print(f"Usage: {sys.argv[0]} <old_log> <new_log>")
    sys.exit(1)

old_file = sys.argv[1]
new_file = sys.argv[2]

old_base, old_lock = find_tables(parse_tables(old_file))
new_base, new_lock = find_tables(parse_tables(new_file))

old_commit = commit_label(old_file)
new_commit = commit_label(new_file)

cs_ns = column(new_base, "cs_ns")

old_lockdep = column(old_lock, "ops_per_sec")
new_baseline = column(new_base, "ops_per_sec")
new_lockdep = column(new_lock, "ops_per_sec")

old_overhead = [b / l for b, l in zip(new_baseline, old_lockdep)]
new_overhead = [b / l for b, l in zip(new_baseline, new_lockdep)]

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
COLOR_OLD = "#b04040"
COLOR_NEW = "#4a7ab5"

fig, axes = plt.subplots(1, 2, figsize=(10, 4.2))

# --- Left: Throughput vs CS length ---
ax = axes[0]
ax.plot(cs_ns, to_millions(new_baseline), "o-", color=COLOR_BASELINE, label="baseline", markersize=4)
ax.plot(cs_ns, to_millions(old_lockdep), "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})", markersize=4)
ax.plot(cs_ns, to_millions(new_lockdep), "D-", color=COLOR_NEW, label=f"lockdep ({new_commit})", markersize=4)
ax.set_xscale("symlog", linthresh=10)
ax.set_yscale("log")
ax.set_ylabel("ops/s (millions)")
ax.set_xlabel("critical section length (ns)")
ax.set_title("Throughput")
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Right: Overhead factor vs CS length ---
ax = axes[1]
# Shift cs_ns=0 to 1 for log scale; use string labels to show true values
cs_plot = [max(x, 1) for x in cs_ns]
ax.plot(cs_plot, old_overhead, "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})", markersize=4)
ax.plot(cs_plot, new_overhead, "D-", color=COLOR_NEW, label=f"lockdep ({new_commit})", markersize=4)
annotate(ax, cs_plot, old_overhead, fmt="{:.1f}x", color=COLOR_OLD, offset=(0, -14))
annotate(ax, cs_plot, new_overhead, fmt="{:.1f}x", color=COLOR_NEW)
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("log")
ax.set_ylabel("overhead (x)")
ax.set_xlabel("critical section length (ns)")
ax.set_title("Overhead factor")
# Relabel the first tick as "0"
ax.set_xticks(cs_plot)
ax.set_xticklabels([str(v) for v in cs_ns], fontsize=7, rotation=45, ha="right")
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.15)

fig.suptitle("CS Length Comparison (8 threads, 1 lock)",
             fontsize=13, fontweight="bold", y=0.98)
fig.tight_layout(rect=[0, 0, 1, 0.93])

os.makedirs("plots", exist_ok=True)
basename = os.path.splitext(os.path.basename(new_file))[0]
output = f"plots/{basename}_compare.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
