# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Visualize overhead vs critical section length.

Handles both legacy 2-condition and new 3-condition (baseline / global / rb)
log formats.

Usage:
    uv run scripts/plot_overhead-cslen.py [LOG_FILE]
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


def column(table, name):
    return [row[name] for row in table]


# --- Load data ---

log_file = sys.argv[1] if len(sys.argv) > 1 else "logs/exp_06_may_cslen.txt"
tables = parse_tables(log_file)

base = find(tables, "baseline")
glob = find(tables, "global") or find(tables, "with lockdep")
rb = find(tables, "rb")

if base is None or glob is None:
    print(f"Error: missing baseline or lockdep table in {log_file}")
    print(f"Found titles: {[t for t, _ in tables]}")
    sys.exit(1)

cs_ns = column(base, "cs_ns")
b = column(base, "ops_per_sec")
g = column(glob, "ops_per_sec")
r = column(rb, "ops_per_sec") if rb else None

g_over = [bb / ll for bb, ll in zip(b, g)]
r_over = [bb / ll for bb, ll in zip(b, r)] if r else None


# --- Style ---
plt.rcParams.update({
    "font.family": "serif",
    "font.size": 10,
    "axes.linewidth": 0.8,
    "axes.grid": True,
    "grid.alpha": 0.3,
    "grid.linewidth": 0.5,
    "lines.linewidth": 1.8,
    "lines.markersize": 4,
    "figure.facecolor": "white",
})

COLOR_BASELINE = "#2d2d2d"
COLOR_GLOBAL = "#b04040"
COLOR_RB = "#4a7ab5"

fig, axes = plt.subplots(1, 2, figsize=(11, 4.4))

# --- Left: Throughput vs CS length ---
ax = axes[0]
ax.plot(cs_ns, [v / 1e6 for v in b], "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(cs_ns, [v / 1e6 for v in g], "s-", color=COLOR_GLOBAL, label="lockdep global")
if r:
    ax.plot(cs_ns, [v / 1e6 for v in r], "D-", color=COLOR_RB, label="lockdep rb")
ax.set_xscale("symlog", linthresh=10)
ax.set_yscale("log")
ax.set_ylabel("ops/s (millions)")
ax.set_xlabel("critical section length (ns)")
ax.set_title("Throughput")
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Right: Overhead factor ---
ax = axes[1]
ax.plot(cs_ns, g_over, "s-", color=COLOR_GLOBAL, label="global")
if r_over:
    ax.plot(cs_ns, r_over, "D-", color=COLOR_RB, label="rb")
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("symlog", linthresh=10)
ax.set_ylabel("overhead (x)")
ax.set_xlabel("critical section length (ns)")
ax.set_title("Overhead factor")
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.08)

fig.suptitle("Overhead vs Critical Section Length (8 threads, 1 lock)",
             fontsize=13, fontweight="bold", y=0.98)
fig.tight_layout(rect=[0, 0, 1, 0.93])

os.makedirs("plots", exist_ok=True)
basename = os.path.splitext(os.path.basename(log_file))[0]
output = f"plots/{basename}.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
