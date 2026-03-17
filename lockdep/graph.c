#include "lockdep.h"

// Global dependency graph. g_adj[i][j] == 1 means there's an edge from lock i to lock j.
static uint8_t g_adj[LOCKDEP_MAX_LOCKS][LOCKDEP_MAX_LOCKS] = {{0}};

// Stats
static uint64_t g_edges_added = 0;
static uint64_t g_cycles_found = 0;

/**
 * DFS helper to check reachability in the lock dependency graph.
 * This helper assumes the caller already holds g_meta_lock.
 * @param cur The current lock ID.
 * @param target The target lock ID.
 * @param visited A boolean array to track visited nodes.
 * @return 1 if reachable, 0 otherwise.
 */
static int lockdep_reachable_locked(unsigned int cur,
                                    unsigned int target,
                                    uint8_t visited[LOCKDEP_MAX_LOCKS]) {
    if (cur == target) {
        return 1;
    }

    visited[cur] = 1;

    for (int next = 0; next < g_num_locks; next++) {
        if (g_adj[cur][next] && !visited[next]) {
            if (lockdep_reachable_locked((unsigned int)next, target, visited)) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Add edge from -> to.
 * If the new edge closes a cycle, report a potential deadlock.
 * @param from the lock on the held stack.
 * @param to the new lock being acquired.
 */
void lockdep_add_edge_and_check_cycle(unsigned int from, unsigned int to) {
    if (from == to) {
        return;
    }

    int found_cycle = 0;

    lockdep_meta_lock();

    if (!g_adj[from][to]) {
        uint8_t visited[LOCKDEP_MAX_LOCKS] = {0};

        /*
         * If 'to' can already reach 'from', then adding from -> to
         * closes a cycle.
         */
        if (lockdep_reachable_locked(to, from, visited)) {
            found_cycle = 1;
            g_cycles_found++;
        }

        g_adj[from][to] = 1;
        g_edges_added++;
    }

    lockdep_meta_unlock();

    if (found_cycle) {
        dprintf(2,
                "[LOCKDEP] potential deadlock: adding edge %u -> %u closes a cycle\n",
                from, to);
    }
}
