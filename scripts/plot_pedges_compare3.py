# /// script
# requires-python = ">=3.10"
# dependencies = ["matplotlib"]
# ///
"""
Compare potential-graph construction sweep across two commits, both 3-condition.

For each (threads:depth) cell, plots wall time per pair (ns) under
baseline / global / rb, with old (dashed) and new (solid) lines.

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
    """Parse 3-condition pedges log. Handles both formats:
    - per-section column header (new logs)
    - single "Columns:" prefix at top, no per-section headers (May 5 logs)
    """
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
        # Skip global "Columns:" preamble or repeated header line
        if line.startswith("Columns:") or set(line.split()) == _DATA_COLSET:
            i += 1
            continue
        # Detect a title line: non-empty, not a data row, looks like a heading.
        if line and not _is_int_row(line):
            title = line
            i += 1
            # Optionally skip a per-section column header line.
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


def cells_for(tables):
    base = find(tables, "baseline")
    glob = find(tables, "global")
    rb = find(tables, "rb")
    if not all([base, glob, rb]):
        return None
    return {"baseline": aggregate(base),
            "global": aggregate(glob),
            "rb": aggregate(rb)}


old_cells = cells_for(old_tables)
new_cells = cells_for(new_tables)

if not old_cells or not new_cells:
    print("Error: missing tables", file=sys.stderr)
    sys.exit(1)

# Use the new run's cell order for x-axis. Match old by key.
keys = [k for k, _ in new_cells["baseline"]]
labels = [f"{t}:{d}" for t, d in keys]


def lookup(cells_list, key):
    for k, v in cells_list:
        if k == key:
            return v
    return None


def series(cells, cond):
    return [lookup(cells[cond], k) for k in keys]


plt.rcParams.update({
    "font.family": "serif",
    "font.size": 10,
    "axes.linewidth": 0.8,
    "axes.grid": True,
    "grid.alpha": 0.3,
    "grid.linewidth": 0.5,
    "lines.linewidth": 1.7,
    "lines.markersize": 5,
    "figure.facecolor": "white",
})

COLORS = {"baseline": "#2d2d2d", "global": "#5a8f3a", "rb": "#4a7ab5"}

fig, axes = plt.subplots(1, 2, figsize=(11, 4.6))

# --- Left: ns-per-pair lines ---
ax = axes[0]
x = list(range(len(keys)))
for cond in ("baseline", "global", "rb"):
    old_y = series(old_cells, cond)
    new_y = series(new_cells, cond)
    ax.plot(x, old_y, "s--", color=COLORS[cond], alpha=0.55,
            label=f"{cond} ({old_commit})")
    ax.plot(x, new_y, "o-", color=COLORS[cond],
            label=f"{cond} ({new_commit})")
ax.set_xticks(x)
ax.set_xticklabels(labels)
ax.set_xlabel("threads : depth")
ax.set_ylabel("ns per lock/unlock pair")
ax.set_title("Per-pair cost")
ax.legend(frameon=False, fontsize=7, ncol=2)

# --- Right: overhead vs new baseline ---
ax = axes[1]
new_b = series(new_cells, "baseline")
for cond in ("global", "rb"):
    old_y = series(old_cells, cond)
    new_y = series(new_cells, cond)
    old_over = [a / b for a, b in zip(old_y, new_b)]
    new_over = [a / b for a, b in zip(new_y, new_b)]
    ax.plot(x, old_over, "s--", color=COLORS[cond], alpha=0.55,
            label=f"{cond} ({old_commit})")
    ax.plot(x, new_over, "o-", color=COLORS[cond],
            label=f"{cond} ({new_commit})")
    for xi, v in zip(x, new_over):
        ax.annotate(f"{v:.1f}x", xy=(xi, v), xytext=(0, 6),
                    textcoords="offset points", ha="center",
                    fontsize=6.5, color=COLORS[cond])
ax.axhline(y=1.0, color="#888888", linewidth=0.8, linestyle="--", zorder=0)
ax.set_xticks(x)
ax.set_xticklabels(labels)
ax.set_xlabel("threads : depth")
ax.set_ylabel("overhead vs new baseline (x)")
ax.set_title("Overhead factor")
ax.legend(frameon=False, fontsize=7, ncol=2)

fig.suptitle(f"Potential-Graph: new ({new_commit}) vs old ({old_commit})",
             fontsize=13, fontweight="bold", y=0.99)
fig.tight_layout(rect=[0, 0, 1, 0.94])

os.makedirs("plots", exist_ok=True)
new_basename = os.path.splitext(os.path.basename(new_file))[0]
old_basename = os.path.splitext(os.path.basename(old_file))[0]
output = f"plots/{new_basename}_vs_{old_basename}_pedges.pdf"
fig.savefig(output, bbox_inches="tight")
print(f"Saved to {output}")
