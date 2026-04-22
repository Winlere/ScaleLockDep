# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Compare overhead benchmarks across two commits: previous lockdep, current
lockdep, and current baseline on the same axes.

Usage:
    uv run scripts/plot_overhead_compare.py <old_log> <new_log>

Example:
    uv run scripts/plot_overhead_compare.py logs/exp_18_mar.txt logs/exp_20_apr_overhead.txt
"""

import os
import sys

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
    with open(path) as f:
        lines = f.readlines()

    tables = {}
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        if line in TABLE_HEADERS:
            header = line
            i += 1
            cols = lines[i].split()
            i += 1
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


def to_millions(vals):
    return [v / 1e6 for v in vals]


def commit_label(path):
    """Extract commit ID from the log file header."""
    with open(path) as f:
        for line in f:
            if line.startswith("Commit ID:"):
                return line.split(":", 1)[1].strip()
    return os.path.basename(path)


def annotate(ax, xs, ys, fmt="{:.1f}", color="black", offset=(0, 6)):
    for i, (x, y) in enumerate(zip(xs, ys)):
        ha = "right" if i == len(xs) - 1 else "center"
        dx = offset[0] - 4 if i == len(xs) - 1 else offset[0]
        ax.annotate(fmt.format(y), (x, y), textcoords="offset points",
                    xytext=(dx, offset[1]), ha=ha, va="bottom",
                    fontsize=6.5, color=color)


# --- Load data ---

if len(sys.argv) < 3:
    print(f"Usage: {sys.argv[0]} <old_log> <new_log>")
    sys.exit(1)

old_file = sys.argv[1]
new_file = sys.argv[2]

old_tables = parse_tables(old_file)
new_tables = parse_tables(new_file)

old_commit = commit_label(old_file)
new_commit = commit_label(new_file)

threads = column(new_tables[TABLE_HEADERS[0]], "threads")

# High contention
hi_old_lockdep = column(old_tables[TABLE_HEADERS[1]], "ops_per_sec")
hi_new_baseline = column(new_tables[TABLE_HEADERS[0]], "ops_per_sec")
hi_new_lockdep = column(new_tables[TABLE_HEADERS[1]], "ops_per_sec")

hi_old_overhead = [b / l for b, l in zip(hi_new_baseline, hi_old_lockdep)]
hi_new_overhead = [b / l for b, l in zip(hi_new_baseline, hi_new_lockdep)]

# Low contention
lo_old_lockdep = column(old_tables[TABLE_HEADERS[3]], "ops_per_sec")
lo_new_baseline = column(new_tables[TABLE_HEADERS[2]], "ops_per_sec")
lo_new_lockdep = column(new_tables[TABLE_HEADERS[3]], "ops_per_sec")

lo_old_overhead = [b / l for b, l in zip(lo_new_baseline, lo_old_lockdep)]
lo_new_overhead = [b / l for b, l in zip(lo_new_baseline, lo_new_lockdep)]

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

fig, axes = plt.subplots(2, 2, figsize=(9, 7.5))

# --- Top-left: High contention throughput ---
ax = axes[0, 0]
ax.plot(threads, to_millions(hi_new_baseline), "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, to_millions(hi_old_lockdep), "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, to_millions(hi_new_lockdep), "D-", color=COLOR_NEW, label=f"lockdep ({new_commit})")
ax.set_xscale("log", base=2)
ax.set_ylabel("ops/s (millions)")
ax.set_title("High contention (1 shared lock)")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.08)

# --- Top-right: Low contention throughput ---
ax = axes[0, 1]
ax.plot(threads, to_millions(lo_new_baseline), "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, to_millions(lo_old_lockdep), "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, to_millions(lo_new_lockdep), "D-", color=COLOR_NEW, label=f"lockdep ({new_commit})")
ax.set_xscale("log", base=2)
ax.set_ylabel("ops/s (millions)")
ax.set_title("Low contention (per-thread locks)")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.08)

# --- Bottom-left: High contention overhead ---
ax = axes[1, 0]
ax.plot(threads, hi_old_overhead, "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, hi_new_overhead, "D-", color=COLOR_NEW, label=f"lockdep ({new_commit})")
annotate(ax, threads, hi_old_overhead, fmt="{:.1f}x", color=COLOR_OLD, offset=(0, -14))
annotate(ax, threads, hi_new_overhead, fmt="{:.1f}x", color=COLOR_NEW)
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("log", base=2)
ax.set_ylabel("overhead (x)")
ax.set_xlabel("threads")
ax.set_title("High contention — overhead factor")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.15)

# --- Bottom-right: Low contention overhead ---
ax = axes[1, 1]
ax.plot(threads, lo_old_overhead, "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, lo_new_overhead, "D-", color=COLOR_NEW, label=f"lockdep ({new_commit})")
annotate(ax, threads, lo_old_overhead, fmt="{:.0f}x", color=COLOR_OLD, offset=(0, -14))
annotate(ax, threads, lo_new_overhead, fmt="{:.1f}x", color=COLOR_NEW)
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("overhead (x, log scale)")
ax.set_xlabel("threads")
ax.set_title("Low contention — overhead factor")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

fig.suptitle("Overhead Comparison: Per-Thread RB vs Global Lock",
             fontsize=13, fontweight="bold", y=0.98)
fig.tight_layout(rect=[0, 0, 1, 0.95])

os.makedirs("plots", exist_ok=True)
basename = os.path.splitext(os.path.basename(new_file))[0]
output = f"plots/{basename}_compare.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
