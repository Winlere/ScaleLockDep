# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Compare nesting depth scaling: simplified 3-line view.

Bars per variant (shallow / deep):
  1. baseline                  (no LD_PRELOAD, from new log)
  2. Naive Method              (March's synchronous lockdep, from old log)
  3. ScaleLockDep              (new rb mode)

Usage:
    uv run scripts/plot_nesting_compare3.py <old_log> <new_log>
"""

import os
import re
import statistics
import sys

import matplotlib.pyplot as plt


def parse_runs(path):
    with open(path) as f:
        text = f.read()
    blocks = {}
    pattern = re.compile(
        r"^(Shallow|Deep)\b[^\n]*\n"
        r"(?:condition\s+run\s+wall_ns\n)?"
        r"((?:(?:baseline|global|rb)\s+\d+\s+\d+\n)+)",
        re.M,
    )
    for m in pattern.finditer(text):
        variant = m.group(1).lower()
        body = m.group(2)
        cells = {}
        for line in body.strip().splitlines():
            cond, _run, ns = line.split()
            cells.setdefault(cond, []).append(int(ns))
        blocks[variant] = cells
    return blocks


def commit_label(path):
    with open(path) as f:
        for line in f:
            if line.startswith("Commit ID:"):
                return line.split(":", 1)[1].strip().split()[0]
    return os.path.basename(path)


if len(sys.argv) < 3:
    print(f"Usage: {sys.argv[0]} <old_log> <new_log>")
    sys.exit(1)

old_file = sys.argv[1]
new_file = sys.argv[2]

old_blocks = parse_runs(old_file)
new_blocks = parse_runs(new_file)

old_commit = commit_label(old_file)
new_commit = commit_label(new_file)

SHALLOW_PAIRS = 1_200_000
DEEP_PAIRS = 16_000_000

plt.rcParams.update({
    "font.family": "serif",
    "font.size": 10,
    "axes.linewidth": 0.8,
    "axes.grid": True,
    "grid.alpha": 0.3,
    "grid.linewidth": 0.5,
    "figure.facecolor": "white",
})

COLOR_BASE = "#2d2d2d"
COLOR_NAIVE = "#b04040"
COLOR_SCALE = "#4a7ab5"

LBL_BASE = "baseline"
LBL_NAIVE = f"Naive Method ({old_commit})"
LBL_SCALE = f"ScaleLockDep ({new_commit})"


def mean_ms(samples):
    if not samples:
        return None
    return statistics.mean(samples) / 1e6


def sd_ms(samples):
    if not samples or len(samples) < 2:
        return 0.0
    return statistics.stdev(samples) / 1e6


fig, axes = plt.subplots(1, 2, figsize=(11, 4.6))

for ax, variant, pairs, title in [
    (axes[0], "shallow", SHALLOW_PAIRS, "Shallow (3-deep, 1.2M pairs)"),
    (axes[1], "deep", DEEP_PAIRS, "Deep (40-deep, 16M pairs)"),
]:
    series = [
        (LBL_BASE, COLOR_BASE, mean_ms(new_blocks[variant].get("baseline")),
         sd_ms(new_blocks[variant].get("baseline"))),
        (LBL_NAIVE, COLOR_NAIVE, mean_ms(old_blocks[variant].get("global")),
         sd_ms(old_blocks[variant].get("global"))),
        (LBL_SCALE, COLOR_SCALE, mean_ms(new_blocks[variant].get("rb")),
         sd_ms(new_blocks[variant].get("rb"))),
    ]

    x = list(range(len(series)))
    means = [v if v is not None else 0.0 for _, _, v, _ in series]
    sds = [s for _, _, _, s in series]
    colors = [c for _, c, _, _ in series]
    labels = [l for l, _, _, _ in series]

    bars = ax.bar(x, means, color=colors, yerr=sds, capsize=4,
                  edgecolor="white", linewidth=0.6)
    for bar, val in zip(bars, means):
        ax.annotate(f"{val:.0f}",
                    xy=(bar.get_x() + bar.get_width() / 2, bar.get_height()),
                    xytext=(0, 3), textcoords="offset points",
                    ha="center", va="bottom", fontsize=8)

    # Add overhead annotation relative to baseline
    base_val = means[0]
    if base_val:
        for i in (1, 2):
            v = means[i]
            ratio = v / base_val
            ax.annotate(f"{ratio:.2f}x",
                        xy=(x[i], v / 2),
                        ha="center", va="center", fontsize=9,
                        color="white", fontweight="bold")

    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=8, rotation=15, ha="right")
    ax.set_ylabel("wall time (ms, mean)")
    ax.set_title(title)

fig.suptitle(f"Nesting Depth: ScaleLockDep ({new_commit}) vs Naive Method ({old_commit}), 40 threads",
             fontsize=13, fontweight="bold", y=0.99)
fig.tight_layout(rect=[0, 0, 1, 0.94])

os.makedirs("plots", exist_ok=True)
new_basename = os.path.splitext(os.path.basename(new_file))[0]
old_basename = os.path.splitext(os.path.basename(old_file))[0]
output = f"plots/{new_basename}_vs_{old_basename}_nesting.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
