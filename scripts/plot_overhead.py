# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Visualize overhead benchmark results from logs/exp_18_mar.txt.

Usage:
    uv run scripts/plot_overhead.py
"""

import os
import re

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


# --- Parsing ---

TABLE_HEADERS = [
    "High-contention (1 shared lock) — baseline",
    "High-contention (1 shared lock) — with lockdep",
    "Low-contention (num_locks = num_threads) — baseline",
    "Low-contention (num_locks = num_threads) — with lockdep",
]


def parse_tables(path):
    """Parse the four raw-data tables from the experiment log.

    Returns a dict mapping each table header to a list of row-dicts
    with int-typed values keyed by column name.
    """
    with open(path) as f:
        lines = f.readlines()

    tables = {}
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        if line in TABLE_HEADERS:
            header = line
            i += 1  # skip column-name row
            i += 1  # advance to first data row
            cols = lines[i - 1].split()
            rows = []
            while i < len(lines) and lines[i].strip():
                vals = lines[i].split()
                rows.append({c: int(v) for c, v in zip(cols, vals)})
                i += 1
            tables[header] = rows
        i += 1
    return tables


def column(table, name):
    return [row[name] for row in table]


# --- Load data ---

tables = parse_tables("logs/exp_18_mar.txt")

hi_base_tbl = tables[TABLE_HEADERS[0]]
hi_lock_tbl = tables[TABLE_HEADERS[1]]
lo_base_tbl = tables[TABLE_HEADERS[2]]
lo_lock_tbl = tables[TABLE_HEADERS[3]]

threads = column(hi_base_tbl, "threads")

hi_baseline = column(hi_base_tbl, "ops_per_sec")
hi_lockdep = column(hi_lock_tbl, "ops_per_sec")
lo_baseline = column(lo_base_tbl, "ops_per_sec")
lo_lockdep = column(lo_lock_tbl, "ops_per_sec")

hi_overhead = [b / l for b, l in zip(hi_baseline, hi_lockdep)]
lo_overhead = [b / l for b, l in zip(lo_baseline, lo_lockdep)]


# --- Helpers ---

def to_millions(vals):
    return [v / 1e6 for v in vals]


def annotate(ax, xs, ys, fmt="{:.1f}", color="black", offset=(0, 6)):
    """Place a text label above each data point."""
    for i, (x, y) in enumerate(zip(xs, ys)):
        # Shift the last point's label left so it stays inside the plot
        ha = "right" if i == len(xs) - 1 else "center"
        dx = offset[0] - 4 if i == len(xs) - 1 else offset[0]
        ax.annotate(fmt.format(y), (x, y), textcoords="offset points",
                    xytext=(dx, offset[1]), ha=ha, va="bottom",
                    fontsize=7, color=color)


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

fig, axes = plt.subplots(2, 2, figsize=(8.5, 7))

# --- Top-left: High contention throughput ---
ax = axes[0, 0]
hi_base_m = to_millions(hi_baseline)
hi_lock_m = to_millions(hi_lockdep)
ax.plot(threads, hi_base_m, "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, hi_lock_m, "s-", color=COLOR_LOCKDEP, label="lockdep")
annotate(ax, threads, hi_base_m, color=COLOR_BASELINE)
annotate(ax, threads, hi_lock_m, color=COLOR_LOCKDEP)
ax.set_xscale("log", base=2)
ax.set_ylabel("ops/s (millions)")
ax.set_title("High contention (1 shared lock)")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.08)

# --- Top-right: Low contention throughput ---
ax = axes[0, 1]
lo_base_m = to_millions(lo_baseline)
lo_lock_m = to_millions(lo_lockdep)
ax.plot(threads, lo_base_m, "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, lo_lock_m, "s-", color=COLOR_LOCKDEP, label="lockdep")
annotate(ax, threads, lo_base_m, color=COLOR_BASELINE)
annotate(ax, threads, lo_lock_m, color=COLOR_LOCKDEP)
ax.set_xscale("log", base=2)
ax.set_ylabel("ops/s (millions)")
ax.set_title("Low contention (per-thread locks)")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.08)

# --- Bottom-left: High contention overhead ---
ax = axes[1, 0]
ax.plot(threads, hi_overhead, "D-", color=COLOR_OVERHEAD)
annotate(ax, threads, hi_overhead, fmt="{:.1f}×", color=COLOR_OVERHEAD)
ax.set_xscale("log", base=2)
ax.set_ylabel("overhead (×)")
ax.set_xlabel("threads")
ax.set_title("High contention — overhead factor")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.08)

# --- Bottom-right: Low contention overhead ---
ax = axes[1, 1]
ax.plot(threads, lo_overhead, "D-", color=COLOR_OVERHEAD)
annotate(ax, threads, lo_overhead, fmt="{:.0f}×", color=COLOR_OVERHEAD)
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("overhead (×, log scale)")
ax.set_xlabel("threads")
ax.set_title("Low contention — overhead factor")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)  # log scale: multiply for headroom

fig.suptitle("Overhead Microbenchmarks", fontsize=13, fontweight="bold", y=0.98)
fig.tight_layout(rect=[0, 0, 1, 0.95])

output = "plots/overhead_benchmarks.pdf"
os.makedirs("plots", exist_ok=True)
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
