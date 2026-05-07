# Shared helpers for scripts/reproduce_*.sh.
# Source this from a reproduce script; it expects to be sourced after
# the script has cd'd to the project root.

set -euo pipefail

# Default output suffix is <DD>_<mon> for today's date, e.g. "06_may".
default_suffix() {
    date +%d_%b | tr '[:upper:]' '[:lower:]'
}

# Capture machine + software metadata into shell variables.
# Sets: SUFFIX, COMMIT, COMMIT_SHORT, DATE, OS, CPU, CORES, MEM
collect_metadata() {
    SUFFIX="${1:-$(default_suffix)}"
    COMMIT=$(git rev-parse HEAD)
    COMMIT_SHORT=$(git rev-parse --short HEAD)
    DATE=$(date +%Y-%m-%d)

    OS="$(uname -srm)"
    CPU="$(grep -m1 'model name' /proc/cpuinfo | sed 's/^[^:]*:[ \t]*//')"
    local sockets cores_per threads
    sockets=$(grep -c '^physical id' /proc/cpuinfo 2>/dev/null || echo 1)
    [ "$sockets" -eq 0 ] && sockets=1
    cores_per=$(grep -m1 'cpu cores' /proc/cpuinfo | awk -F: '{gsub(/ /,"",$2); print $2}')
    threads=$(grep -c '^processor' /proc/cpuinfo)
    if [ -n "$cores_per" ]; then
        CORES="$((sockets * cores_per)) cores / ${threads} threads"
    else
        CORES="${threads} threads"
    fi
    MEM=$(awk '/MemTotal/ {printf "%.0f GiB RAM", $2/1024/1024}' /proc/meminfo)
}

# Print the standard Settings + Software + Commit preamble.
# Args: title
write_preamble() {
    local title="$1"
    cat <<EOF
Experiment: ${title}
Date: ${DATE}
Commit ID: ${COMMIT_SHORT}

--- Settings ---

Machine
  OS       : ${OS}
  CPU      : ${CPU} (${CORES})
  Memory   : ${MEM}

Software
  Compiler : gcc (-Wall -Wextra -pthread; lockdep: -O3 -g -flto
             -fno-semantic-interposition)
  Loader   : LD_PRELOAD for lockdep injection
  Commit   : ${COMMIT}

EOF
}

# Print the standard "lockdep limits" footer.
write_lockdep_limits() {
    cat <<EOF
lockdep limits (lockdep.h)
  LOCKDEP_MAX_LOCK_SLOTS      = 256
  LOCKDEP_MAX_HELD_LOCK_SLOTS = 64
  LOCKDEP_MAX_THREAD_SLOTS    = 128

EOF
}

# Build liblockdep + benchmarks (idempotent — make handles up-to-date checks).
ensure_built() {
    make build >/dev/null
}

# Resolve LD_PRELOAD prefix for a condition name.
prefix_for() {
    case "$1" in
        baseline) echo "" ;;
        global)   echo "LOCKDEP_MODE=global LD_PRELOAD=./lockdep/liblockdep.so" ;;
        rb)       echo "LOCKDEP_MODE=rb     LD_PRELOAD=./lockdep/liblockdep.so" ;;
        *) echo "Unknown condition: $1" >&2; exit 1 ;;
    esac
}

# Human-readable label for a condition (used as table title).
label_for() {
    case "$1" in
        baseline) echo "baseline" ;;
        global)   echo "lockdep global" ;;
        rb)       echo "lockdep rb (ScaleLockDep)" ;;
    esac
}
