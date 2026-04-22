# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Compare per-operation latency across two commits: previous lockdep, current
lockdep, and current baseline on the same axes.

Usage:
    uv run scripts/plot_latency_compare.py <old_log> <new_log>
"""

import os
import sys

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


# --- Parsing ---

_DATA_COLUMNS = {
    "threads", "locks", "iters", "wall_ns", "total_ops", "ops_per_sec",
    "cs_ns", "avg_lock_ns", "avg_unlock_ns", "avg_pair_ns",
}


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
                rows.append({c: float(v) for c, v in zip(cols, vals)})
                i += 1
            tables[title] = rows
        i += 1
    return tables


def column(table, name):
    return [row[name] for row in table]


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
        if "baseline" in lower or lower.startswith("baseline"):
            base_tbl = tables[key]
        elif "lockdep" in lower or lower.startswith("with"):
            lock_tbl = tables[key]
    return base_tbl, lock_tbl


def annotate(ax, xs, ys, fmt="{:.0f}", color="black", offset=(0, 6)):
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

old_base, old_lock = find_tables(parse_tables(old_file))
new_base, new_lock = find_tables(parse_tables(new_file))

old_commit = commit_label(old_file)
new_commit = commit_label(new_file)

threads = [int(t) for t in column(new_base, "threads")]

# Pair latency
old_lock_pair = column(old_lock, "avg_pair_ns")
new_base_pair = column(new_base, "avg_pair_ns")
new_lock_pair = column(new_lock, "avg_pair_ns")

# Lock latency
old_lock_lock = column(old_lock, "avg_lock_ns")
new_base_lock = column(new_base, "avg_lock_ns")
new_lock_lock = column(new_lock, "avg_lock_ns")

# Unlock latency
old_lock_unlock = column(old_lock, "avg_unlock_ns")
new_base_unlock = column(new_base, "avg_unlock_ns")
new_lock_unlock = column(new_lock, "avg_unlock_ns")

# Overhead
old_overhead = [l / b for b, l in zip(new_base_pair, old_lock_pair)]
new_overhead = [l / b for b, l in zip(new_base_pair, new_lock_pair)]

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

# --- Top-left: Lock latency ---
ax = axes[0, 0]
ax.plot(threads, new_base_lock, "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, old_lock_lock, "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, new_lock_lock, "D-", color=COLOR_NEW, label=f"lockdep ({new_commit})")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("latency (ns)")
ax.set_title("Lock latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Top-right: Unlock latency ---
ax = axes[0, 1]
ax.plot(threads, new_base_unlock, "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, old_lock_unlock, "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, new_lock_unlock, "D-", color=COLOR_NEW, label=f"lockdep ({new_commit})")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("latency (ns)")
ax.set_title("Unlock latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Bottom-left: Pair latency ---
ax = axes[1, 0]
ax.plot(threads, new_base_pair, "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, old_lock_pair, "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, new_lock_pair, "D-", color=COLOR_NEW, label=f"lockdep ({new_commit})")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("latency (ns)")
ax.set_xlabel("threads")
ax.set_title("Lock+unlock pair latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Bottom-right: Pair overhead factor ---
ax = axes[1, 1]
ax.plot(threads, old_overhead, "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, new_overhead, "D-", color=COLOR_NEW, label=f"lockdep ({new_commit})")
annotate(ax, threads, old_overhead, fmt="{:.1f}x", color=COLOR_OLD, offset=(0, -14))
annotate(ax, threads, new_overhead, fmt="{:.1f}x", color=COLOR_NEW)
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("log", base=2)
ax.set_ylabel("overhead (x)")
ax.set_xlabel("threads")
ax.set_title("Lock+unlock pair overhead")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.15)

fig.suptitle("Per-Operation Latency Comparison (1 shared lock)",
             fontsize=13, fontweight="bold", y=0.98)
fig.tight_layout(rect=[0, 0, 1, 0.95])

os.makedirs("plots", exist_ok=True)
basename = os.path.splitext(os.path.basename(new_file))[0]
output = f"plots/{basename}_compare.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
