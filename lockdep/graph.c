#include "lockdep.h"

/*
 * Global dependency graph.
 * g_dependency_graph[u][v] == 1 means an observed dependency edge Lu -> Lv.
 */
static uint8_t g_dependency_graph[LOCKDEP_MAX_LOCK_SLOTS][LOCKDEP_MAX_LOCK_SLOTS] = {{0}};

/* Optional small stats for debugging / future summary. */
static int g_dependency_edge_count = 0;
static int g_dependency_cycle_count = 0;

/**
 * DFS helper to find whether target_lock_slot is reachable from cur_lock_slot.
 * Also records parent pointers so we can reconstruct the existing dependency chain.
 * Caller must already hold g_meta_lock.
 */
static int lockdep_find_path_locked(int cur_lock_slot,
                                    int target_lock_slot,
                                    int visited[LOCKDEP_MAX_LOCK_SLOTS],
                                    int parent[LOCKDEP_MAX_LOCK_SLOTS]) {
    if (cur_lock_slot == target_lock_slot) {
        return 1;
    }

    visited[cur_lock_slot] = 1;

    for (int next_lock_slot = 0; next_lock_slot < g_lock_slot_count; next_lock_slot++) {
        if (g_dependency_graph[cur_lock_slot][next_lock_slot] && !visited[next_lock_slot]) {
            parent[next_lock_slot] = cur_lock_slot;
            if (lockdep_find_path_locked(next_lock_slot,
                                         target_lock_slot,
                                         visited,
                                         parent)) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Add a dependency edge from one held lock slot to a newly acquired lock slot.
 * If the new edge closes a cycle, report a potential deadlock.
 */
void lockdep_add_edge_and_check_cycle(int from_lock_slot, int to_lock_slot) {
    if (from_lock_slot == to_lock_slot) {
        return;
    }

    int found_cycle = 0;
    int existing_chain[LOCKDEP_MAX_LOCK_SLOTS];
    int existing_chain_len = 0;

    lockdep_meta_lock();

    if (!g_dependency_graph[from_lock_slot][to_lock_slot]) {
        int visited[LOCKDEP_MAX_LOCK_SLOTS] = {0};
        int parent[LOCKDEP_MAX_LOCK_SLOTS];
        for (int i = 0; i < LOCKDEP_MAX_LOCK_SLOTS; i++) {
            parent[i] = LOCKDEP_INVALID_SLOT;
        }

        /*
         * If Lto can already reach Lfrom, then adding Lfrom -> Lto closes a cycle.
         */
        if (lockdep_find_path_locked(to_lock_slot,
                                     from_lock_slot,
                                     visited,
                                     parent)) {
            int reverse_chain[LOCKDEP_MAX_LOCK_SLOTS];
            int reverse_chain_len = 0;
            int cur_lock_slot = from_lock_slot;

            found_cycle = 1;
            g_dependency_cycle_count++;

            while (cur_lock_slot != LOCKDEP_INVALID_SLOT &&
                   reverse_chain_len < LOCKDEP_MAX_LOCK_SLOTS) {
                reverse_chain[reverse_chain_len++] = cur_lock_slot;
                if (cur_lock_slot == to_lock_slot) {
                    break;
                }
                cur_lock_slot = parent[cur_lock_slot];
            }

            if (reverse_chain_len > 0 &&
                reverse_chain[reverse_chain_len - 1] == to_lock_slot) {
                for (int i = reverse_chain_len - 1; i >= 0; i--) {
                    existing_chain[existing_chain_len++] = reverse_chain[i];
                }
            }
        }

        g_dependency_graph[from_lock_slot][to_lock_slot] = 1;
        g_dependency_edge_count++;
    }

    lockdep_meta_unlock();

    if (found_cycle) {
        lockdep_report_potential_deadlock(from_lock_slot,
                                          to_lock_slot,
                                          existing_chain,
                                          existing_chain_len);
    }
}
