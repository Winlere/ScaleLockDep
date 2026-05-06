# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Compare cslen sweep: simplified 3-line view.

Lines per panel:
  1. baseline                  (no LD_PRELOAD, from new log)
  2. Naive Method              (March's synchronous lockdep, from old log)
  3. ScaleLockDep              (new rb mode)

Usage:
    uv run scripts/plot_cslen_compare3.py <old_log> <new_log>
"""

import os
import sys

import matplotlib.pyplot as plt


_DATA_COLUMNS = {"threads", "iters", "cs_ns", "wall_ns", "total_ops", "ops_per_sec"}


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
                rows.append({c: int(v) for c, v in zip(tokens, vals)})
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


def to_millions(vals):
    return [v / 1e6 for v in vals]


def commit_label(path):
    with open(path) as f:
        for line in f:
            if line.startswith("Commit ID:"):
                return line.split(":", 1)[1].strip().split()[0]
    return os.path.basename(path)


def annotate(ax, xs, ys, fmt="{:.1f}x", color="black", offset=(0, 6), fontsize=7):
    for i, (x, y) in enumerate(zip(xs, ys)):
        ha = "right" if i == len(xs) - 1 else "center"
        dx = offset[0] - 4 if i == len(xs) - 1 else offset[0]
        ax.annotate(fmt.format(y), (x, y), textcoords="offset points",
                    xytext=(dx, offset[1]), ha=ha, va="bottom",
                    fontsize=fontsize, color=color)


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

cs_ns = column(new_base, "cs_ns")
new_b = column(new_base, "ops_per_sec")
new_s = column(new_rb, "ops_per_sec")
old_n = column(old_naive, "ops_per_sec")

old_n_over = [b / l for b, l in zip(new_b, old_n)]
new_s_over = [b / l for b, l in zip(new_b, new_s)]

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

fig, axes = plt.subplots(1, 2, figsize=(11, 4.4))

ax = axes[0]
ax.plot(cs_ns, to_millions(new_b), "o-", color=COLOR_BASE, label=LBL_BASE)
ax.plot(cs_ns, to_millions(old_n), "s--", color=COLOR_NAIVE, label=LBL_NAIVE)
ax.plot(cs_ns, to_millions(new_s), "D-", color=COLOR_SCALE, label=LBL_SCALE)
ax.set_xscale("symlog", linthresh=10)
ax.set_yscale("log")
ax.set_ylabel("ops/s (millions)")
ax.set_xlabel("critical section length (ns)")
ax.set_title("Throughput")
ax.legend(frameon=False, fontsize=8)

ax = axes[1]
cs_plot = [max(x, 1) for x in cs_ns]
ax.plot(cs_plot, old_n_over, "s--", color=COLOR_NAIVE, label=LBL_NAIVE)
ax.plot(cs_plot, new_s_over, "D-", color=COLOR_SCALE, label=LBL_SCALE)
annotate(ax, cs_plot, old_n_over, color=COLOR_NAIVE, offset=(0, -14))
annotate(ax, cs_plot, new_s_over, color=COLOR_SCALE)
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("log")
ax.set_ylabel("overhead vs baseline (x)")
ax.set_xlabel("critical section length (ns)")
ax.set_title("Overhead factor")
ax.set_xticks(cs_plot)
ax.set_xticklabels([str(v) for v in cs_ns], fontsize=7, rotation=45, ha="right")
ax.legend(frameon=False, fontsize=8)

fig.suptitle(f"CS Length: ScaleLockDep ({new_commit}) vs Naive Method ({old_commit})",
             fontsize=13, fontweight="bold", y=0.995)
fig.tight_layout(rect=[0, 0, 1, 0.93])

os.makedirs("plots", exist_ok=True)
new_basename = os.path.splitext(os.path.basename(new_file))[0]
old_basename = os.path.splitext(os.path.basename(old_file))[0]
output = f"plots/{new_basename}_vs_{old_basename}_cslen.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
