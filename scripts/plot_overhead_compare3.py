# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Compare overhead benchmarks: 3-condition new log (baseline + global + rb)
against an older 1-condition log (baseline + with lockdep).

Plots four lines per panel: baseline (new), old lockdep, new global,
new rb. Output PDF: plots/<new_basename>_vs_<old_basename>_overhead.pdf.

Usage:
    uv run scripts/plot_overhead_compare3.py <old_log> <new_log>
"""

import os
import sys

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


_DATA_COLUMNS = {"threads", "locks", "iters", "wall_ns", "total_ops", "ops_per_sec"}


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


def find(tables, *needles, scenario=None, exclude=()):
    for title, rows in tables:
        lower = title.lower()
        if scenario and scenario not in lower:
            continue
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

# New side: 3 conditions per scenario.
hi_base = find(new_tables, "baseline", scenario="high")
hi_glob = find(new_tables, "global", scenario="high")
hi_rb = find(new_tables, "rb", scenario="high")
lo_base = find(new_tables, "baseline", scenario="low")
lo_glob = find(new_tables, "global", scenario="low")
lo_rb = find(new_tables, "rb", scenario="low")

# Old side: legacy "with lockdep" (March/April format).
hi_old = (find(old_tables, "with lockdep", scenario="high")
          or find(old_tables, "lockdep", scenario="high", exclude=("baseline",)))
lo_old = (find(old_tables, "with lockdep", scenario="low")
          or find(old_tables, "lockdep", scenario="low", exclude=("baseline",)))

if not all([hi_base, hi_glob, hi_rb, lo_base, lo_glob, lo_rb, hi_old, lo_old]):
    print("Error: missing tables", file=sys.stderr)
    print(f"  new titles: {[t for t,_ in new_tables]}", file=sys.stderr)
    print(f"  old titles: {[t for t,_ in old_tables]}", file=sys.stderr)
    sys.exit(1)

threads = column(hi_base, "threads")

hi_b = column(hi_base, "ops_per_sec")
hi_g = column(hi_glob, "ops_per_sec")
hi_r = column(hi_rb, "ops_per_sec")
hi_o = column(hi_old, "ops_per_sec")

lo_b = column(lo_base, "ops_per_sec")
lo_g = column(lo_glob, "ops_per_sec")
lo_r = column(lo_rb, "ops_per_sec")
lo_o = column(lo_old, "ops_per_sec")

hi_o_over = [b / l for b, l in zip(hi_b, hi_o)]
hi_g_over = [b / l for b, l in zip(hi_b, hi_g)]
hi_r_over = [b / l for b, l in zip(hi_b, hi_r)]
lo_o_over = [b / l for b, l in zip(lo_b, lo_o)]
lo_g_over = [b / l for b, l in zip(lo_b, lo_g)]
lo_r_over = [b / l for b, l in zip(lo_b, lo_r)]

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

# Top-left: high-contention throughput
ax = axes[0, 0]
ax.plot(threads, to_millions(hi_b), "o-", color=COLOR_BASE, label="baseline (new)")
ax.plot(threads, to_millions(hi_o), "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, to_millions(hi_g), "^-", color=COLOR_GLOB, label=f"global ({new_commit})")
ax.plot(threads, to_millions(hi_r), "D-", color=COLOR_RB, label=f"rb ({new_commit})")
ax.set_xscale("log", base=2)
ax.set_ylabel("ops/s (millions)")
ax.set_title("High contention (1 shared lock)")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

# Top-right: low-contention throughput
ax = axes[0, 1]
ax.plot(threads, to_millions(lo_b), "o-", color=COLOR_BASE, label="baseline (new)")
ax.plot(threads, to_millions(lo_o), "s--", color=COLOR_OLD, label=f"lockdep ({old_commit})")
ax.plot(threads, to_millions(lo_g), "^-", color=COLOR_GLOB, label=f"global ({new_commit})")
ax.plot(threads, to_millions(lo_r), "D-", color=COLOR_RB, label=f"rb ({new_commit})")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("ops/s (millions, log)")
ax.set_title("Low contention (per-thread locks)")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

# Bottom-left: high-contention overhead
ax = axes[1, 0]
ax.plot(threads, hi_o_over, "s--", color=COLOR_OLD, label=f"old ({old_commit})")
ax.plot(threads, hi_g_over, "^-", color=COLOR_GLOB, label=f"global ({new_commit})")
ax.plot(threads, hi_r_over, "D-", color=COLOR_RB, label=f"rb ({new_commit})")
annotate(ax, threads, hi_o_over, color=COLOR_OLD, offset=(0, -14))
annotate(ax, threads, hi_g_over, color=COLOR_GLOB)
annotate(ax, threads, hi_r_over, color=COLOR_RB, offset=(0, -22))
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("log", base=2)
ax.set_ylabel("overhead (x)")
ax.set_xlabel("threads")
ax.set_title("High contention - overhead factor")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

# Bottom-right: low-contention overhead (log y)
ax = axes[1, 1]
ax.plot(threads, lo_o_over, "s--", color=COLOR_OLD, label=f"old ({old_commit})")
ax.plot(threads, lo_g_over, "^-", color=COLOR_GLOB, label=f"global ({new_commit})")
ax.plot(threads, lo_r_over, "D-", color=COLOR_RB, label=f"rb ({new_commit})")
annotate(ax, threads, lo_o_over, fmt="{:.0f}x", color=COLOR_OLD, offset=(0, -14))
annotate(ax, threads, lo_g_over, color=COLOR_GLOB)
annotate(ax, threads, lo_r_over, color=COLOR_RB, offset=(0, -22))
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_ylabel("overhead (x, log)")
ax.set_xlabel("threads")
ax.set_title("Low contention - overhead factor")
ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
ax.set_xticks(threads)
ax.legend(frameon=False, fontsize=8)

fig.suptitle(f"Overhead: new ({new_commit}) vs old ({old_commit})",
             fontsize=13, fontweight="bold", y=0.995)
fig.tight_layout(rect=[0, 0, 1, 0.96])

os.makedirs("plots", exist_ok=True)
new_base = os.path.splitext(os.path.basename(new_file))[0]
old_base = os.path.splitext(os.path.basename(old_file))[0]
output = f"plots/{new_base}_vs_{old_base}_overhead.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
