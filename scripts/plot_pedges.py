# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Visualize potential-graph construction cost across the (threads, depth)
sweep at fixed total lock budget.

Reads logs/exp_06_may_pedges.txt (or compatible). Reports mean over the
3 runs per cell.

Usage:
    uv run scripts/plot_pedges.py [LOG_FILE]
"""

import os
import statistics
import sys

import matplotlib.pyplot as plt


_DATA_COLUMNS = {"threads", "locks_per_thread", "iters", "wall_ns",
                 "total_ops", "ops_per_sec"}


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
            cols = tokens
            i += 1
            rows = []
            while i < len(lines) and lines[i].strip():
                vals = lines[i].split()
                rows.append({c: int(v) for c, v in zip(cols, vals)})
                i += 1
            tables.append((title, rows))
        i += 1
    return tables


def find(tables, *needles):
    for title, rows in tables:
        lower = title.lower()
        if all(n in lower for n in needles):
            return rows
    return None


def aggregate(rows):
    """Group rows by (threads, depth) and return mean wall_ns, total_ops."""
    groups = {}
    for r in rows:
        key = (r["threads"], r["locks_per_thread"])
        groups.setdefault(key, []).append(r)
    cells = {}
    for key, samples in groups.items():
        wall = statistics.mean(s["wall_ns"] for s in samples)
        ops = samples[0]["total_ops"]
        cells[key] = (wall, ops)
    return cells


log_file = sys.argv[1] if len(sys.argv) > 1 else "logs/exp_06_may_pedges.txt"
tables = parse_tables(log_file)

base = find(tables, "baseline")
glob = find(tables, "global")
rb = find(tables, "rb")

if base is None or glob is None or rb is None:
    print(f"Error: missing baseline / global / rb tables in {log_file}")
    print(f"Found titles: {[t for t, _ in tables]}")
    sys.exit(1)

base_cells = aggregate(base)
glob_cells = aggregate(glob)
rb_cells = aggregate(rb)

# Order the cells by depth (descending), which is the headline sweep axis.
cells = sorted(base_cells.keys(), key=lambda k: (-k[1], k[0]))

labels = [f"{t}T x d={d}" for (t, d) in cells]
depths = [d for (_, d) in cells]


def ns_pp(cells_dict, key):
    wall, ops = cells_dict[key]
    return wall / ops


b_pp = [ns_pp(base_cells, k) for k in cells]
g_pp = [ns_pp(glob_cells, k) for k in cells]
r_pp = [ns_pp(rb_cells, k) for k in cells]

g_marg = [g - b for b, g in zip(b_pp, g_pp)]
r_marg = [r - b for b, r in zip(b_pp, r_pp)]


# --- Style ---
plt.rcParams.update({
    "font.family": "serif",
    "font.size": 10,
    "axes.linewidth": 0.8,
    "axes.grid": True,
    "grid.alpha": 0.3,
    "grid.linewidth": 0.5,
    "lines.linewidth": 1.8,
    "lines.markersize": 6,
    "figure.facecolor": "white",
})

COLOR_BASELINE = "#2d2d2d"
COLOR_GLOBAL = "#b04040"
COLOR_RB = "#4a7ab5"

fig, axes = plt.subplots(1, 2, figsize=(12, 4.8))


# --- Left: ns/pair grouped bars across cells ---
ax = axes[0]
x = list(range(len(cells)))
width = 0.27
ax.bar([xi - width for xi in x], b_pp, width=width,
       color=COLOR_BASELINE, label="baseline", edgecolor="white", linewidth=0.5)
ax.bar(x, g_pp, width=width,
       color=COLOR_GLOBAL, label="lockdep global", edgecolor="white", linewidth=0.5)
ax.bar([xi + width for xi in x], r_pp, width=width,
       color=COLOR_RB, label="lockdep rb", edgecolor="white", linewidth=0.5)
for xi, val in zip(x, b_pp):
    ax.annotate(f"{val:.1f}", xy=(xi - width, val), xytext=(0, 3),
                textcoords="offset points", ha="center", va="bottom", fontsize=6.5)
for xi, val in zip(x, g_pp):
    ax.annotate(f"{val:.1f}", xy=(xi, val), xytext=(0, 3),
                textcoords="offset points", ha="center", va="bottom", fontsize=6.5)
for xi, val in zip(x, r_pp):
    ax.annotate(f"{val:.1f}", xy=(xi + width, val), xytext=(0, 3),
                textcoords="offset points", ha="center", va="bottom", fontsize=6.5)
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=20, ha="right")
ax.set_ylabel("ns per lock/unlock pair")
ax.set_title("Per-pair cost across (threads, depth)")
ax.legend(frameon=False, fontsize=8)


# --- Right: marginal cost vs depth (line plot) ---
ax = axes[1]
ax.plot(depths, g_marg, "s-", color=COLOR_GLOBAL, label="global")
ax.plot(depths, r_marg, "D-", color=COLOR_RB, label="rb")
for d, val in zip(depths, g_marg):
    ax.annotate(f"{val:.1f}", xy=(d, val), xytext=(0, 6),
                textcoords="offset points", ha="center", va="bottom",
                fontsize=7, color=COLOR_GLOBAL)
for d, val in zip(depths, r_marg):
    ax.annotate(f"{val:.1f}", xy=(d, val), xytext=(0, -14),
                textcoords="offset points", ha="center", va="bottom",
                fontsize=7, color=COLOR_RB)
ax.set_xscale("log", base=2)
ax.set_xticks(sorted(set(depths)))
ax.get_xaxis().set_major_formatter(plt.matplotlib.ticker.ScalarFormatter())
ax.set_xlabel("nesting depth (locks_per_thread)")
ax.set_ylabel("marginal ns/pair (lockdep - baseline)")
ax.set_title("Marginal cost vs nesting depth")
ax.legend(frameon=False, fontsize=8)


fig.suptitle("Potential-Graph Construction Cost (disjoint locks, 100k iters/thread)",
             fontsize=13, fontweight="bold", y=0.99)
fig.tight_layout(rect=[0, 0, 1, 0.94])

os.makedirs("plots", exist_ok=True)
basename = os.path.splitext(os.path.basename(log_file))[0]
output = f"plots/{basename}.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
