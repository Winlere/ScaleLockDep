# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Compare nesting depth scaling across two commits, both 3-condition logs.

Shows baseline / global / rb wall-time means for shallow + deep, with
old (dashed) and new (solid) side-by-side.

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

CONDITIONS = ["baseline", "global", "rb"]


def has_rb(blocks):
    return all("rb" in blocks.get(v, {}) for v in ("shallow", "deep"))


OLD_HAS_RB = has_rb(old_blocks)
OLD_CONDS = CONDITIONS if OLD_HAS_RB else ["baseline", "global"]

def stats(samples):
    return statistics.mean(samples), (statistics.stdev(samples) if len(samples) > 1 else 0.0)


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

COLORS = {"baseline": "#2d2d2d", "global": "#5a8f3a", "rb": "#4a7ab5"}

fig, axes = plt.subplots(1, 2, figsize=(11, 4.8))

# --- Left: wall time bar chart, shallow vs deep, grouped by condition, side-by-side old/new ---
for ax, variant, pairs, title in [
    (axes[0], "shallow", SHALLOW_PAIRS, "Shallow (3-deep, 1.2M pairs)"),
    (axes[1], "deep", DEEP_PAIRS, "Deep (40-deep, 16M pairs)"),
]:
    width = 0.34
    x = list(range(len(CONDITIONS)))
    new_means_ms = [statistics.mean(new_blocks[variant][c]) / 1e6 for c in CONDITIONS]
    new_sds_ms = [(statistics.stdev(new_blocks[variant][c]) if len(new_blocks[variant][c]) > 1 else 0)/1e6 for c in CONDITIONS]

    # Old side: keep slot positions aligned with new conditions; leave
    # missing conditions (e.g. rb in March) as NaN so the bar collapses.
    old_means_ms = []
    old_sds_ms = []
    for c in CONDITIONS:
        samples = old_blocks[variant].get(c)
        if samples:
            old_means_ms.append(statistics.mean(samples) / 1e6)
            old_sds_ms.append((statistics.stdev(samples) if len(samples) > 1 else 0) / 1e6)
        else:
            old_means_ms.append(0.0)
            old_sds_ms.append(0.0)

    bars_old = ax.bar([xi - width/2 for xi in x], old_means_ms, width=width,
                      color=[COLORS[c] for c in CONDITIONS], yerr=old_sds_ms, capsize=3,
                      edgecolor="white", linewidth=0.5, alpha=0.55,
                      label=f"old ({old_commit})", hatch="//")
    bars_new = ax.bar([xi + width/2 for xi in x], new_means_ms, width=width,
                      color=[COLORS[c] for c in CONDITIONS], yerr=new_sds_ms, capsize=3,
                      edgecolor="white", linewidth=0.5,
                      label=f"new ({new_commit})")
    for b, v in zip(bars_old, old_means_ms):
        if v == 0.0:
            ax.text(b.get_x() + b.get_width()/2, 0, "n/a",
                    ha="center", va="bottom", fontsize=6.5, color="#888")
            continue
        ax.annotate(f"{v:.0f}", xy=(b.get_x() + b.get_width()/2, b.get_height()),
                    xytext=(0, 3), textcoords="offset points",
                    ha="center", va="bottom", fontsize=7, color="#444")
    for b, v in zip(bars_new, new_means_ms):
        ax.annotate(f"{v:.0f}", xy=(b.get_x() + b.get_width()/2, b.get_height()),
                    xytext=(0, 3), textcoords="offset points",
                    ha="center", va="bottom", fontsize=7)
    ax.set_xticks(x)
    ax.set_xticklabels(CONDITIONS)
    ax.set_ylabel("wall time (ms, mean)")
    ax.set_title(title)
    ax.legend(frameon=False, fontsize=8)

fig.suptitle(f"Nesting Depth: new ({new_commit}) vs old ({old_commit}), 40 threads",
             fontsize=13, fontweight="bold", y=0.99)
fig.tight_layout(rect=[0, 0, 1, 0.94])

os.makedirs("plots", exist_ok=True)
new_basename = os.path.splitext(os.path.basename(new_file))[0]
old_basename = os.path.splitext(os.path.basename(old_file))[0]
output = f"plots/{new_basename}_vs_{old_basename}_nesting.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
