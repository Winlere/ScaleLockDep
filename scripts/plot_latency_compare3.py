# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Compare per-operation latency: simplified 3-line view.

Lines per panel:
  1. baseline                  (no LD_PRELOAD, from new log)
  2. Naive Method              (March's synchronous lockdep, from old log)
  3. ScaleLockDep              (new rb mode)

Usage:
    uv run scripts/plot_latency_compare3.py <old_log> <new_log>
"""

import os
import sys

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


_DATA_COLUMNS = {"threads", "iters", "avg_lock_ns", "avg_unlock_ns", "avg_pair_ns"}


def parse_tables(path):
    with open(path) as f:
        lines = f.readlines()
    tables = []
    i = 0
    while i < len(lines):
        tokens = lines[i].split()
        if tokens and all(t in _DATA_COLUMNS for t in tokens):
            title = ""
            for j in range(i - 1, -1, -1):
                if lines[j].strip():
                    title = lines[j].strip()
                    break
            i += 1
            rows = []
            while i < len(lines) and lines[i].strip():
                vals = lines[i].split()
                rows.append({c: float(v) for c, v in zip(tokens, vals)})
                i += 1
            tables.append((title, rows))
        i += 1
    return tables


def find(tables, *needles, exclude=()):
    for title, rows in tables:
        lower = title.lower()
        if any(e in lower for e in exclude):
            continue
        if all(n in lower for n in needles):
            return rows
    return None


def column(table, name):
    return [row[name] for row in table]


def commit_label(path):
    with open(path) as f:
        for line in f:
            if line.startswith("Commit ID:"):
                return line.split(":", 1)[1].strip().split()[0]
    return os.path.basename(path)


def annotate(ax, xs, ys, fmt="{:.1f}x", color="black", offset=(0, 6)):
    for i, (x, y) in enumerate(zip(xs, ys)):
        ha = "right" if i == len(xs) - 1 else "center"
        dx = offset[0] - 4 if i == len(xs) - 1 else offset[0]
        ax.annotate(fmt.format(y), (x, y), textcoords="offset points",
                    xytext=(dx, offset[1]), ha=ha, va="bottom",
                    fontsize=7, color=color)


if len(sys.argv) < 3:
    print(f"Usage: {sys.argv[0]} <old_log> <new_log>")
    sys.exit(1)

old_file = sys.argv[1]
new_file = sys.argv[2]

old_tables = parse_tables(old_file)
new_tables = parse_tables(new_file)

old_commit = commit_label(old_file)
new_commit = commit_label(new_file)

new_base = find(new_tables, "baseline")
new_rb = find(new_tables, "rb")
old_naive = (find(old_tables, "with lockdep")
             or find(old_tables, "lockdep", exclude=("baseline",)))

if not all([new_base, new_rb, old_naive]):
    print("Error: missing tables", file=sys.stderr)
    sys.exit(1)

threads = [int(t) for t in column(new_base, "threads")]

base_lock = column(new_base, "avg_lock_ns")
base_unlock = column(new_base, "avg_unlock_ns")
base_pair = column(new_base, "avg_pair_ns")
naive_lock = column(old_naive, "avg_lock_ns")
naive_unlock = column(old_naive, "avg_unlock_ns")
naive_pair = column(old_naive, "avg_pair_ns")
scale_lock = column(new_rb, "avg_lock_ns")
scale_unlock = column(new_rb, "avg_unlock_ns")
scale_pair = column(new_rb, "avg_pair_ns")

naive_pair_over = [l / b for b, l in zip(base_pair, naive_pair)]
scale_pair_over = [l / b for b, l in zip(base_pair, scale_pair)]

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

COLOR_BASE = "#2d2d2d"
COLOR_NAIVE = "#b04040"
COLOR_SCALE = "#4a7ab5"

LBL_BASE = "baseline"
LBL_NAIVE = f"Naive Method ({old_commit})"
LBL_SCALE = f"ScaleLockDep ({new_commit})"

fig, axes = plt.subplots(2, 2, figsize=(10, 8))

ax = axes[0, 0]
ax.plot(threads, base_lock, "o-", color=COLOR_BASE, label=LBL_BASE)
ax.plot(threads, naive_lock, "s--", color=COLOR_NAIVE, label=LBL_NAIVE)
ax.plot(threads, scale_lock, "D-", color=COLOR_SCALE, label=LBL_SCALE)
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("latency (ns)")
ax.set_title("Lock latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

ax = axes[0, 1]
ax.plot(threads, base_unlock, "o-", color=COLOR_BASE, label=LBL_BASE)
ax.plot(threads, naive_unlock, "s--", color=COLOR_NAIVE, label=LBL_NAIVE)
ax.plot(threads, scale_unlock, "D-", color=COLOR_SCALE, label=LBL_SCALE)
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("latency (ns)")
ax.set_title("Unlock latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

ax = axes[1, 0]
ax.plot(threads, base_pair, "o-", color=COLOR_BASE, label=LBL_BASE)
ax.plot(threads, naive_pair, "s--", color=COLOR_NAIVE, label=LBL_NAIVE)
ax.plot(threads, scale_pair, "D-", color=COLOR_SCALE, label=LBL_SCALE)
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("latency (ns)")
ax.set_xlabel("threads")
ax.set_title("Lock+unlock pair latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

ax = axes[1, 1]
ax.plot(threads, naive_pair_over, "s--", color=COLOR_NAIVE, label=LBL_NAIVE)
ax.plot(threads, scale_pair_over, "D-", color=COLOR_SCALE, label=LBL_SCALE)
annotate(ax, threads, naive_pair_over, color=COLOR_NAIVE, offset=(0, -14))
annotate(ax, threads, scale_pair_over, color=COLOR_SCALE)
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("log", base=2)
ax.set_ylabel("overhead vs baseline (x)")
ax.set_xlabel("threads")
ax.set_title("Pair overhead factor")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

fig.suptitle(f"Latency: ScaleLockDep ({new_commit}) vs Naive Method ({old_commit})",
             fontsize=13, fontweight="bold", y=0.995)
fig.tight_layout(rect=[0, 0, 1, 0.96])

os.makedirs("plots", exist_ok=True)
new_basename = os.path.splitext(os.path.basename(new_file))[0]
old_basename = os.path.splitext(os.path.basename(old_file))[0]
output = f"plots/{new_basename}_vs_{old_basename}_latency.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
