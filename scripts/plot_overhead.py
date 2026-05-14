# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Visualize overhead benchmark results from an experiment log.

Handles both the legacy 2-condition format (baseline + with lockdep) and
the 3-condition format (baseline + lockdep global + lockdep rb).

Usage:
    uv run scripts/plot_overhead.py [LOG_FILE]
"""

import os
import sys

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


_DATA_COLUMNS = {"threads", "locks", "iters", "wall_ns", "total_ops", "ops_per_sec"}


def parse_tables(path):
    """Parse raw-data tables. Title is the nearest preceding non-empty line."""
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


def find_table(tables, *needles, scenario=None):
    for title, rows in tables:
        lower = title.lower()
        if scenario and scenario not in lower:
            continue
        if all(n in lower for n in needles):
            return rows
    return None


def column(table, name):
    return [row[name] for row in table]


def to_millions(vals):
    return [v / 1e6 for v in vals]


def annotate(ax, xs, ys, fmt="{:.1f}", color="black", offset=(0, 6), fontsize=6.5):
    for i, (x, y) in enumerate(zip(xs, ys)):
        ha = "right" if i == len(xs) - 1 else "center"
        dx = offset[0] - 4 if i == len(xs) - 1 else offset[0]
        ax.annotate(fmt.format(y), (x, y), textcoords="offset points",
                    xytext=(dx, offset[1]), ha=ha, va="bottom",
                    fontsize=fontsize, color=color)


# --- Load data ---

log_file = sys.argv[1] if len(sys.argv) > 1 else "logs/exp_06_may_overhead.txt"
tables = parse_tables(log_file)

hi_base = find_table(tables, "baseline", scenario="high")
hi_global = (find_table(tables, "global", scenario="high")
             or find_table(tables, "with lockdep", scenario="high"))
hi_rb = find_table(tables, "rb", scenario="high")

lo_base = find_table(tables, "baseline", scenario="low")
lo_global = (find_table(tables, "global", scenario="low")
             or find_table(tables, "with lockdep", scenario="low"))
lo_rb = find_table(tables, "rb", scenario="low")

if hi_base is None or hi_global is None or lo_base is None or lo_global is None:
    print(f"Error: could not find required tables in {log_file}")
    print(f"Found titles: {[t for t, _ in tables]}")
    sys.exit(1)

threads = column(hi_base, "threads")

hi_b = column(hi_base, "ops_per_sec")
hi_g = column(hi_global, "ops_per_sec")
hi_r = column(hi_rb, "ops_per_sec") if hi_rb else None

lo_b = column(lo_base, "ops_per_sec")
lo_g = column(lo_global, "ops_per_sec")
lo_r = column(lo_rb, "ops_per_sec") if lo_rb else None

hi_g_over = [b / l for b, l in zip(hi_b, hi_g)]
hi_r_over = [b / l for b, l in zip(hi_b, hi_r)] if hi_r else None
lo_g_over = [b / l for b, l in zip(lo_b, lo_g)]
lo_r_over = [b / l for b, l in zip(lo_b, lo_r)] if lo_r else None


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
COLOR_GLOBAL = "#b04040"
COLOR_RB = "#4a7ab5"

fig, axes = plt.subplots(2, 2, figsize=(9.5, 7.5))

# --- Top-left: High contention throughput ---
ax = axes[0, 0]
ax.plot(threads, to_millions(hi_b), "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, to_millions(hi_g), "s-", color=COLOR_GLOBAL, label="lockdep global")
if hi_r:
    ax.plot(threads, to_millions(hi_r), "D-", color=COLOR_RB, label="lockdep rb")
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
ax.plot(threads, to_millions(lo_b), "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, to_millions(lo_g), "s-", color=COLOR_GLOBAL, label="lockdep global")
if lo_r:
    ax.plot(threads, to_millions(lo_r), "D-", color=COLOR_RB, label="lockdep rb")
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
ax.plot(threads, hi_g_over, "s-", color=COLOR_GLOBAL, label="global")
if hi_r_over:
    ax.plot(threads, hi_r_over, "D-", color=COLOR_RB, label="rb")
annotate(ax, threads, hi_g_over, fmt="{:.1f}x", color=COLOR_GLOBAL)
if hi_r_over:
    annotate(ax, threads, hi_r_over, fmt="{:.1f}x", color=COLOR_RB, offset=(0, -14))
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("log", base=2)
ax.set_ylabel("overhead (x)")
ax.set_xlabel("threads")
ax.set_title("High contention - overhead factor")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.15)

# --- Bottom-right: Low contention overhead ---
ax = axes[1, 1]
ax.plot(threads, lo_g_over, "s-", color=COLOR_GLOBAL, label="global")
if lo_r_over:
    ax.plot(threads, lo_r_over, "D-", color=COLOR_RB, label="rb")
annotate(ax, threads, lo_g_over, fmt="{:.1f}x", color=COLOR_GLOBAL)
if lo_r_over:
    annotate(ax, threads, lo_r_over, fmt="{:.1f}x", color=COLOR_RB, offset=(0, -14))
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("log", base=2)
ax.set_ylabel("overhead (x)")
ax.set_xlabel("threads")
ax.set_title("Low contention - overhead factor")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.15)

fig.suptitle("Overhead Microbenchmarks (baseline vs global vs rb)",
             fontsize=13, fontweight="bold", y=0.98)
fig.tight_layout(rect=[0, 0, 1, 0.95])

os.makedirs("plots", exist_ok=True)
basename = os.path.splitext(os.path.basename(log_file))[0]
output = f"plots/{basename}.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
