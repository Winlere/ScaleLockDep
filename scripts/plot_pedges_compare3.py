# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Compare potential-graph construction sweep: simplified 3-line view.

Lines per panel:
  1. baseline                  (no LD_PRELOAD, from new log)
  2. Naive Method              (synchronous global mode from the old log;
                                bench_potential_edges did not exist in March,
                                so the old log here is the closest available
                                run of the synchronous mode, e.g. May 5)
  3. ScaleLockDep              (new rb mode)

Usage:
    uv run scripts/plot_pedges_compare3.py <old_log> <new_log>
"""

import os
import statistics
import sys

import matplotlib.pyplot as plt


_DATA_COLUMNS = ["threads", "locks_per_thread", "iters", "wall_ns",
                 "total_ops", "ops_per_sec"]
_DATA_COLSET = set(_DATA_COLUMNS)


def _is_int_row(s):
    parts = s.split()
    if len(parts) != len(_DATA_COLUMNS):
        return False
    return all(p.lstrip("-").isdigit() for p in parts)


def parse_tables(path):
    with open(path) as f:
        lines = f.readlines()
    tables = []
    i = 0
    in_raw = False
    while i < len(lines):
        line = lines[i].strip()
        if line.startswith("--- Raw Data"):
            in_raw = True
            i += 1
            continue
        if not in_raw:
            i += 1
            continue
        if line.startswith("Columns:") or set(line.split()) == _DATA_COLSET:
            i += 1
            continue
        if line and not _is_int_row(line):
            title = line
            i += 1
            if i < len(lines) and set(lines[i].split()) == _DATA_COLSET:
                i += 1
            rows = []
            while i < len(lines):
                lstrip = lines[i].strip()
                if not lstrip:
                    break
                if not _is_int_row(lstrip):
                    break
                vals = lstrip.split()
                rows.append({c: int(v) for c, v in zip(_DATA_COLUMNS, vals)})
                i += 1
            if rows:
                tables.append((title, rows))
            continue
        i += 1
    return tables


def find(tables, *needles):
    for title, rows in tables:
        lower = title.lower()
        if all(n in lower for n in needles):
            return rows
    return None


def aggregate(rows):
    """Group rows by (threads, depth); return ordered list of (key, mean_ns_per_pair)."""
    groups = {}
    for r in rows:
        key = (r["threads"], r["locks_per_thread"])
        groups.setdefault(key, []).append(r)
    cells = []
    for key in groups:
        samples = groups[key]
        wall = statistics.mean(s["wall_ns"] for s in samples)
        ops = samples[0]["total_ops"]
        cells.append((key, wall / ops))
    return cells


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

old_tables = parse_tables(old_file)
new_tables = parse_tables(new_file)

old_commit = commit_label(old_file)
new_commit = commit_label(new_file)

new_base = find(new_tables, "baseline")
new_rb = find(new_tables, "rb")
old_naive = find(old_tables, "global") or find(old_tables, "with lockdep") \
            or find(old_tables, "lockdep")

if not all([new_base, new_rb, old_naive]):
    print("Error: missing tables", file=sys.stderr)
    print(f"  old: {[t for t,_ in old_tables]}", file=sys.stderr)
    print(f"  new: {[t for t,_ in new_tables]}", file=sys.stderr)
    sys.exit(1)


def lookup(cell_list, key):
    for k, v in cell_list:
        if k == key:
            return v
    return None


new_b_cells = aggregate(new_base)
new_s_cells = aggregate(new_rb)
old_n_cells = aggregate(old_naive)

keys = [k for k, _ in new_b_cells]
labels = [f"{t}:{d}" for t, d in keys]

new_b = [lookup(new_b_cells, k) for k in keys]
new_s = [lookup(new_s_cells, k) for k in keys]
old_n = [lookup(old_n_cells, k) for k in keys]

old_n_over = [a / b for a, b in zip(old_n, new_b)]
new_s_over = [a / b for a, b in zip(new_s, new_b)]

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

COLOR_BASE = "#2d2d2d"
COLOR_NAIVE = "#b04040"
COLOR_SCALE = "#4a7ab5"

LBL_BASE = "baseline"
LBL_NAIVE = f"Naive Method ({old_commit}*)"
LBL_SCALE = f"ScaleLockDep ({new_commit})"

x = list(range(len(keys)))

fig, axes = plt.subplots(1, 2, figsize=(11, 4.6))

ax = axes[0]
ax.plot(x, new_b, "o-", color=COLOR_BASE, label=LBL_BASE)
ax.plot(x, old_n, "s--", color=COLOR_NAIVE, label=LBL_NAIVE)
ax.plot(x, new_s, "D-", color=COLOR_SCALE, label=LBL_SCALE)
ax.set_xticks(x)
ax.set_xticklabels(labels)
ax.set_xlabel("threads : depth")
ax.set_ylabel("ns per lock/unlock pair")
ax.set_title("Per-pair cost")
ax.legend(frameon=False, fontsize=8)

ax = axes[1]
ax.plot(x, old_n_over, "s--", color=COLOR_NAIVE, label=LBL_NAIVE)
ax.plot(x, new_s_over, "D-", color=COLOR_SCALE, label=LBL_SCALE)
for xi, v in zip(x, old_n_over):
    ax.annotate(f"{v:.1f}x", xy=(xi, v), xytext=(0, -14),
                textcoords="offset points", ha="center", fontsize=7,
                color=COLOR_NAIVE)
for xi, v in zip(x, new_s_over):
    ax.annotate(f"{v:.1f}x", xy=(xi, v), xytext=(0, 6),
                textcoords="offset points", ha="center", fontsize=7,
                color=COLOR_SCALE)
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xticks(x)
ax.set_xticklabels(labels)
ax.set_xlabel("threads : depth")
ax.set_ylabel("overhead vs baseline (x)")
ax.set_title("Overhead factor")
ax.legend(frameon=False, fontsize=8)

fig.suptitle(f"Potential-Graph: ScaleLockDep ({new_commit}) vs Naive Method ({old_commit}*)\n"
             "* bench_potential_edges did not exist in March; old line is closest"
             " available synchronous-mode run",
             fontsize=11, fontweight="bold", y=1.0)
fig.tight_layout(rect=[0, 0, 1, 0.92])

os.makedirs("plots", exist_ok=True)
new_basename = os.path.splitext(os.path.basename(new_file))[0]
old_basename = os.path.splitext(os.path.basename(old_file))[0]
output = f"plots/{new_basename}_vs_{old_basename}_pedges.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
