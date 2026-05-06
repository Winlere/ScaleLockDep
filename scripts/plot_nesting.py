# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Visualize nesting-depth scaling: shallow (3-deep) vs deep (40-deep) under
baseline / lockdep global / lockdep rb.

Reads logs/exp_06_may_nesting.txt (or any file in the same format: lines
of `condition run wall_ns` grouped under `=== nesting: shallow ...` and
`=== nesting: deep ...` sections, OR the rendered table from the log).

Usage:
    uv run scripts/plot_nesting.py [LOG_FILE]
"""

import os
import re
import statistics
import sys

import matplotlib.pyplot as plt


def parse_runs(path):
    """Parse the Raw Data section of the nesting log.

    The log lists 5 runs per (variant, condition) cell with `condition run wall_ns`
    rows, grouped by header lines containing 'Shallow' or 'Deep'.
    """
    with open(path) as f:
        text = f.read()

    # Find Shallow / Deep blocks under "--- Raw Data ---".
    blocks = {}
    pattern = re.compile(
        r"^(Shallow|Deep)\b[^\n]*\n"
        r"(?:condition\s+run\s+wall_ns\n)?"
        r"((?:(?:baseline|global|rb)\s+\d+\s+\d+\n)+)",
        re.M,
    )
    for m in pattern.finditer(text):
        variant = m.group(1).lower()  # "shallow" or "deep"
        body = m.group(2)
        cells = {}  # condition -> list of wall_ns
        for line in body.strip().splitlines():
            cond, _run, ns = line.split()
            cells.setdefault(cond, []).append(int(ns))
        blocks[variant] = cells
    return blocks


log_file = sys.argv[1] if len(sys.argv) > 1 else "logs/exp_06_may_nesting.txt"
blocks = parse_runs(log_file)

if "shallow" not in blocks or "deep" not in blocks:
    print(f"Error: missing shallow/deep raw data in {log_file}")
    print(f"Found: {list(blocks.keys())}")
    sys.exit(1)

# Pairs per run (workload constants)
SHALLOW_PAIRS = 1_200_000
DEEP_PAIRS = 16_000_000

conditions = ["baseline", "global", "rb"]


def stats(samples):
    return statistics.mean(samples), statistics.stdev(samples) if len(samples) > 1 else 0.0


shallow_means = {c: stats(blocks["shallow"][c]) for c in conditions}
deep_means = {c: stats(blocks["deep"][c]) for c in conditions}


def ns_per_pair(ns, pairs):
    return ns / pairs


def ops_per_sec(ns, pairs):
    return pairs / (ns / 1e9)


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

COLORS = {"baseline": "#2d2d2d", "global": "#b04040", "rb": "#4a7ab5"}
LABELS = {"baseline": "baseline", "global": "lockdep global", "rb": "lockdep rb"}

fig, axes = plt.subplots(1, 3, figsize=(13.5, 4.5))


# --- Left: wall time bars (shallow vs deep) ---
ax = axes[0]
x_positions = [0, 1]
width = 0.25
for i, cond in enumerate(conditions):
    s_mean, s_sd = shallow_means[cond]
    d_mean, d_sd = deep_means[cond]
    means_ms = [s_mean / 1e6, d_mean / 1e6]
    sds_ms = [s_sd / 1e6, d_sd / 1e6]
    offset = (i - 1) * width
    bars = ax.bar([x + offset for x in x_positions], means_ms,
                  width=width, color=COLORS[cond], label=LABELS[cond],
                  yerr=sds_ms, capsize=3, edgecolor="white", linewidth=0.5)
    for j, bar in enumerate(bars):
        ax.annotate(f"{means_ms[j]:.0f}", xy=(bar.get_x() + bar.get_width() / 2,
                                              bar.get_height()),
                    xytext=(0, 3), textcoords="offset points",
                    ha="center", va="bottom", fontsize=7)
ax.set_xticks(x_positions)
ax.set_xticklabels(["shallow (3-deep)", "deep (40-deep)"])
ax.set_ylabel("wall time (ms, 5-run mean)")
ax.set_title("Wall time per run")
ax.legend(frameon=False, fontsize=8)


# --- Middle: ns per pair ---
ax = axes[1]
for i, cond in enumerate(conditions):
    s_ns_pp = ns_per_pair(shallow_means[cond][0], SHALLOW_PAIRS)
    d_ns_pp = ns_per_pair(deep_means[cond][0], DEEP_PAIRS)
    means = [s_ns_pp, d_ns_pp]
    offset = (i - 1) * width
    bars = ax.bar([x + offset for x in x_positions], means,
                  width=width, color=COLORS[cond], label=LABELS[cond],
                  edgecolor="white", linewidth=0.5)
    for j, bar in enumerate(bars):
        ax.annotate(f"{means[j]:.1f}", xy=(bar.get_x() + bar.get_width() / 2,
                                           bar.get_height()),
                    xytext=(0, 3), textcoords="offset points",
                    ha="center", va="bottom", fontsize=7)
ax.set_xticks(x_positions)
ax.set_xticklabels(["shallow", "deep"])
ax.set_ylabel("ns per lock/unlock pair")
ax.set_title("Per-pair cost")
ax.legend(frameon=False, fontsize=8)


# --- Right: overhead vs baseline ---
ax = axes[2]
for i, cond in enumerate(["global", "rb"]):
    s_over = shallow_means[cond][0] / shallow_means["baseline"][0]
    d_over = deep_means[cond][0] / deep_means["baseline"][0]
    means = [s_over, d_over]
    offset = (i - 0.5) * width
    bars = ax.bar([x + offset for x in x_positions], means,
                  width=width, color=COLORS[cond], label=LABELS[cond],
                  edgecolor="white", linewidth=0.5)
    for j, bar in enumerate(bars):
        ax.annotate(f"{means[j]:.2f}x", xy=(bar.get_x() + bar.get_width() / 2,
                                            bar.get_height()),
                    xytext=(0, 3), textcoords="offset points",
                    ha="center", va="bottom", fontsize=7)
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xticks(x_positions)
ax.set_xticklabels(["shallow", "deep"])
ax.set_ylabel("overhead vs baseline (x)")
ax.set_title("Overhead factor")
ax.legend(frameon=False, fontsize=8)


fig.suptitle("Nesting Depth Scaling: shallow (3-deep) vs deep (40-deep), 40 threads",
             fontsize=13, fontweight="bold", y=0.99)
fig.tight_layout(rect=[0, 0, 1, 0.94])

os.makedirs("plots", exist_ok=True)
basename = os.path.splitext(os.path.basename(log_file))[0]
output = f"plots/{basename}.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
