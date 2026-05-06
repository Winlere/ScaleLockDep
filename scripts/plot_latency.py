# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Visualize per-operation latency.

Handles both legacy 2-condition and new 3-condition (baseline / global / rb)
log formats.

Usage:
    uv run scripts/plot_latency.py [LOG_FILE]
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
            cols = tokens
            i += 1
            rows = []
            while i < len(lines) and lines[i].strip():
                vals = lines[i].split()
                rows.append({c: float(v) for c, v in zip(cols, vals)})
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


def annotate(ax, xs, ys, fmt="{:.1f}", color="black", offset=(0, 6), fontsize=6.5):
    for i, (x, y) in enumerate(zip(xs, ys)):
        ha = "right" if i == len(xs) - 1 else "center"
        dx = offset[0] - 4 if i == len(xs) - 1 else offset[0]
        ax.annotate(fmt.format(y), (x, y), textcoords="offset points",
                    xytext=(dx, offset[1]), ha=ha, va="bottom",
                    fontsize=fontsize, color=color)


# --- Load data ---

log_file = sys.argv[1] if len(sys.argv) > 1 else "logs/exp_06_may_latency.txt"
tables = parse_tables(log_file)

base = find(tables, "baseline")
glob = find(tables, "global") or find(tables, "with lockdep")
rb = find(tables, "rb")

if base is None or glob is None:
    print(f"Error: missing baseline or lockdep table in {log_file}")
    print(f"Found titles: {[t for t, _ in tables]}")
    sys.exit(1)

threads = [int(t) for t in column(base, "threads")]
b_pair = column(base, "avg_pair_ns")
g_pair = column(glob, "avg_pair_ns")
r_pair = column(rb, "avg_pair_ns") if rb else None

b_lock = column(base, "avg_lock_ns")
g_lock = column(glob, "avg_lock_ns")
r_lock = column(rb, "avg_lock_ns") if rb else None

b_unlock = column(base, "avg_unlock_ns")
g_unlock = column(glob, "avg_unlock_ns")
r_unlock = column(rb, "avg_unlock_ns") if rb else None

g_added_pair = [l - bb for bb, l in zip(b_pair, g_pair)]
r_added_pair = [l - bb for bb, l in zip(b_pair, r_pair)] if r_pair else None

g_pair_over = [l / bb for bb, l in zip(b_pair, g_pair)]
r_pair_over = [l / bb for bb, l in zip(b_pair, r_pair)] if r_pair else None


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

# --- Top-left: Lock latency ---
ax = axes[0, 0]
ax.plot(threads, b_lock, "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, g_lock, "s-", color=COLOR_GLOBAL, label="global")
if r_lock:
    ax.plot(threads, r_lock, "D-", color=COLOR_RB, label="rb")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("avg lock latency (ns)")
ax.set_title("Lock latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Top-right: Unlock latency ---
ax = axes[0, 1]
ax.plot(threads, b_unlock, "o-", color=COLOR_BASELINE, label="baseline")
ax.plot(threads, g_unlock, "s-", color=COLOR_GLOBAL, label="global")
if r_unlock:
    ax.plot(threads, r_unlock, "D-", color=COLOR_RB, label="rb")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("avg unlock latency (ns)")
ax.set_title("Unlock latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Bottom-left: Added pair delay ---
ax = axes[1, 0]
ax.plot(threads, g_added_pair, "s-", color=COLOR_GLOBAL, label="global")
if r_added_pair:
    ax.plot(threads, r_added_pair, "D-", color=COLOR_RB, label="rb")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("added pair delay (ns)")
ax.set_xlabel("threads")
ax.set_title("Lockdep added pair delay (lockdep - baseline)")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)
ylo, yhi = ax.get_ylim()
ax.set_ylim(ylo, yhi * 1.5)

# --- Bottom-right: Pair overhead factor ---
ax = axes[1, 1]
ax.plot(threads, g_pair_over, "s-", color=COLOR_GLOBAL, label="global")
if r_pair_over:
    ax.plot(threads, r_pair_over, "D-", color=COLOR_RB, label="rb")
annotate(ax, threads, g_pair_over, fmt="{:.2f}x", color=COLOR_GLOBAL)
if r_pair_over:
    annotate(ax, threads, r_pair_over, fmt="{:.2f}x", color=COLOR_RB, offset=(0, -14))
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

fig.suptitle("Per-Operation Latency (1 shared lock, baseline vs global vs rb)",
             fontsize=13, fontweight="bold", y=0.98)
fig.tight_layout(rect=[0, 0, 1, 0.95])

os.makedirs("plots", exist_ok=True)
basename = os.path.splitext(os.path.basename(log_file))[0]
output = f"plots/{basename}.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
