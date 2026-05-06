# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Compare per-operation latency: 3-condition new log (baseline + global + rb)
against an older 1-condition log.

Output: plots/<new_basename>_vs_<old_basename>_latency.pdf

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
                return line.split(":", 1)[1].strip()
    return os.path.basename(path)


def annotate(ax, xs, ys, fmt="{:.1f}x", color="black", offset=(0, 6)):
    for i, (x, y) in enumerate(zip(xs, ys)):
        ha = "right" if i == len(xs) - 1 else "center"
        dx = offset[0] - 4 if i == len(xs) - 1 else offset[0]
        ax.annotate(fmt.format(y), (x, y), textcoords="offset points",
                    xytext=(dx, offset[1]), ha=ha, va="bottom",
                    fontsize=6.5, color=color)


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
new_glob = find(new_tables, "global")
new_rb = find(new_tables, "rb")
old_lock = (find(old_tables, "with lockdep")
            or find(old_tables, "lockdep", exclude=("baseline",)))

if not all([new_base, new_glob, new_rb, old_lock]):
    print("Error: missing tables", file=sys.stderr)
    print(f"  new: {[t for t,_ in new_tables]}", file=sys.stderr)
    print(f"  old: {[t for t,_ in old_tables]}", file=sys.stderr)
    sys.exit(1)

threads = [int(t) for t in column(new_base, "threads")]

new_b_lock = column(new_base, "avg_lock_ns")
new_b_unlock = column(new_base, "avg_unlock_ns")
new_b_pair = column(new_base, "avg_pair_ns")
new_g_pair = column(new_glob, "avg_pair_ns")
new_r_pair = column(new_rb, "avg_pair_ns")
old_l_pair = column(old_lock, "avg_pair_ns")
new_g_lock = column(new_glob, "avg_lock_ns")
new_r_lock = column(new_rb, "avg_lock_ns")
old_l_lock = column(old_lock, "avg_lock_ns")
new_g_unlock = column(new_glob, "avg_unlock_ns")
new_r_unlock = column(new_rb, "avg_unlock_ns")
old_l_unlock = column(old_lock, "avg_unlock_ns")

old_pair_over = [l / b for b, l in zip(new_b_pair, old_l_pair)]
new_g_pair_over = [l / b for b, l in zip(new_b_pair, new_g_pair)]
new_r_pair_over = [l / b for b, l in zip(new_b_pair, new_r_pair)]

# --- Style ---
plt.rcParams.update({
    "font.family": "serif",
    "font.size": 10,
    "axes.linewidth": 0.8,
    "axes.grid": True,
    "grid.alpha": 0.3,
    "grid.linewidth": 0.5,
    "lines.linewidth": 1.7,
    "lines.markersize": 4.5,
    "figure.facecolor": "white",
})

COLOR_BASE = "#2d2d2d"
COLOR_OLD = "#b04040"
COLOR_GLOB = "#5a8f3a"
COLOR_RB = "#4a7ab5"

fig, axes = plt.subplots(2, 2, figsize=(10, 8))

# Top-left: lock latency
ax = axes[0, 0]
ax.plot(threads, new_b_lock, "o-", color=COLOR_BASE, label="baseline (new)")
ax.plot(threads, old_l_lock, "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, new_g_lock, "^-", color=COLOR_GLOB, label=f"global ({new_commit})")
ax.plot(threads, new_r_lock, "D-", color=COLOR_RB, label=f"rb ({new_commit})")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("latency (ns)")
ax.set_title("Lock latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

# Top-right: unlock latency
ax = axes[0, 1]
ax.plot(threads, new_b_unlock, "o-", color=COLOR_BASE, label="baseline (new)")
ax.plot(threads, old_l_unlock, "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, new_g_unlock, "^-", color=COLOR_GLOB, label=f"global ({new_commit})")
ax.plot(threads, new_r_unlock, "D-", color=COLOR_RB, label=f"rb ({new_commit})")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("latency (ns)")
ax.set_title("Unlock latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

# Bottom-left: pair latency
ax = axes[1, 0]
ax.plot(threads, new_b_pair, "o-", color=COLOR_BASE, label="baseline (new)")
ax.plot(threads, old_l_pair, "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, new_g_pair, "^-", color=COLOR_GLOB, label=f"global ({new_commit})")
ax.plot(threads, new_r_pair, "D-", color=COLOR_RB, label=f"rb ({new_commit})")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("latency (ns)")
ax.set_xlabel("threads")
ax.set_title("Lock+unlock pair latency")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

# Bottom-right: pair overhead factor
ax = axes[1, 1]
ax.plot(threads, old_pair_over, "s--", color=COLOR_OLD, label=f"old ({old_commit})")
ax.plot(threads, new_g_pair_over, "^-", color=COLOR_GLOB, label=f"global ({new_commit})")
ax.plot(threads, new_r_pair_over, "D-", color=COLOR_RB, label=f"rb ({new_commit})")
annotate(ax, threads, old_pair_over, color=COLOR_OLD, offset=(0, -14))
annotate(ax, threads, new_g_pair_over, color=COLOR_GLOB)
annotate(ax, threads, new_r_pair_over, color=COLOR_RB, offset=(0, -22))
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("log", base=2)
ax.set_ylabel("overhead (x)")
ax.set_xlabel("threads")
ax.set_title("Pair overhead factor")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

fig.suptitle(f"Latency: new ({new_commit}) vs old ({old_commit})",
             fontsize=13, fontweight="bold", y=0.995)
fig.tight_layout(rect=[0, 0, 1, 0.96])

os.makedirs("plots", exist_ok=True)
new_basename = os.path.splitext(os.path.basename(new_file))[0]
old_basename = os.path.splitext(os.path.basename(old_file))[0]
output = f"plots/{new_basename}_vs_{old_basename}_latency.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
