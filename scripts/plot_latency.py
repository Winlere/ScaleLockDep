# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Visualize per-operation latency from logs/exp_24_mar_latency.txt.

Usage:
    uv run scripts/plot_latency.py [LOG_FILE]

Defaults to logs/exp_24_mar_latency.txt if no argument is given.
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
    """Parse raw-data tables from an experiment log."""
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


# --- Helpers ---

def annotate(ax, xs, ys, fmt="{:.0f}", color="black", offset=(0, 6)):
    """Place a text label above each data point."""
    for i, (x, y) in enumerate(zip(xs, ys)):
        ha = "right" if i == len(xs) - 1 else "center"
        dx = offset[0] - 4 if i == len(xs) - 1 else offset[0]
        ax.annotate(fmt.format(y), (x, y), textcoords="offset points",
                    xytext=(dx, offset[1]), ha=ha, va="bottom",
                    fontsize=6.5, color=color)


# --- Load data ---

log_file = sys.argv[1] if len(sys.argv) > 1 else "logs/exp_24_mar_latency.txt"
tables = parse_tables(log_file)

base_tbl = None
lock_tbl = None
for key in tables:
    lower = key.lower()
    if "baseline" in lower or lower.startswith("baseline"):
        base_tbl = tables[key]
    elif "lockdep" in lower or lower.startswith("with"):
        lock_tbl = tables[key]

if base_tbl is None or lock_tbl is None:
    print(f"Error: could not find baseline/lockdep tables in {log_file}")
    print(f"Found tables: {list(tables.keys())}")
    sys.exit(1)

threads = [int(t) for t in column(base_tbl, "threads")]

base_lock = column(base_tbl, "avg_lock_ns")
base_unlock = column(base_tbl, "avg_unlock_ns")
base_pair = column(base_tbl, "avg_pair_ns")

lock_lock = column(lock_tbl, "avg_lock_ns")
lock_unlock = column(lock_tbl, "avg_unlock_ns")
lock_pair = column(lock_tbl, "avg_pair_ns")

added_lock = [l - b for b, l in zip(base_lock, lock_lock)]
added_unlock = [l - b for b, l in zip(base_unlock, lock_unlock)]
added_pair = [l - b for b, l in zip(base_pair, lock_pair)]
pair_overhead = [l / b for b, l in zip(base_pair, lock_pair)]

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
COLOR_LOCKDEP = "#b04040"
COLOR_LOCK = "#4a7ab5"
COLOR_UNLOCK = "#2d8e5e"
COLOR_PAIR = "#7b5ea7"

fig, axes = plt.subplots(2, 2, figsize=(9, 7))

# --- Top-left: Lock latency ---
ax = axes[0, 0]
ax.plot(threads, base_lock, "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, lock_lock, "s-", color=COLOR_LOCKDEP, label="lockdep")
annotate(ax, threads, base_lock, color=COLOR_BASELINE)
annotate(ax, threads, lock_lock, color=COLOR_LOCKDEP)
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("latency (ns)")
ax.set_title("Lock latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Top-right: Unlock latency ---
ax = axes[0, 1]
ax.plot(threads, base_unlock, "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, lock_unlock, "s-", color=COLOR_LOCKDEP, label="lockdep")
annotate(ax, threads, base_unlock, color=COLOR_BASELINE)
annotate(ax, threads, lock_unlock, color=COLOR_LOCKDEP)
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("latency (ns)")
ax.set_title("Unlock latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Bottom-left: Added delay (lock vs unlock) ---
ax = axes[1, 0]
ax.plot(threads, added_lock, "D-", color=COLOR_LOCK, label="lock")
ax.plot(threads, added_unlock, "^-", color=COLOR_UNLOCK, label="unlock")
annotate(ax, threads, added_lock, color=COLOR_LOCK)
annotate(ax, threads, added_unlock, color=COLOR_UNLOCK)
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("added delay (ns)")
ax.set_xlabel("threads")
ax.set_title("Lockdep added delay")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Bottom-right: Pair overhead factor ---
ax = axes[1, 1]
ax.plot(threads, pair_overhead, "D-", color=COLOR_PAIR)
annotate(ax, threads, pair_overhead, fmt="{:.1f}×", color=COLOR_PAIR)
ax.set_xscale("log", base=2)
ax.set_ylabel("overhead (×)")
ax.set_xlabel("threads")
ax.set_title("Lock+unlock pair overhead")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi + (yhi - ylo) * 0.08)

fig.suptitle("Per-Operation Latency (1 shared lock)",
             fontsize=13, fontweight="bold", y=0.98)
fig.tight_layout(rect=[0, 0, 1, 0.95])

os.makedirs("plots", exist_ok=True)
basename = os.path.splitext(os.path.basename(log_file))[0]
output = f"plots/{basename}.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
