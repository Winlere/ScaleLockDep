#include "lockdep.h"

/**
 * Print a panic message and exit the program.
 */
void lockdep_panic(const char *msg) {
    dprintf(2, "%s", msg);
    _exit(1);
}

static void lockdep_print_lock_slot(int lock_slot) {
    pthread_mutex_t *addr = NULL;
    int lock_slot_count = atomic_load_explicit(&g_lock_slot_count, memory_order_acquire);

    if (lock_slot >= 0 && lock_slot < lock_slot_count) {
        addr = g_lock_slots[lock_slot].addr;
    }

    dprintf(2, "L%d(%p)", lock_slot, (void *)addr);
}

static void lockdep_print_thread_slot(int thread_slot) {
    pid_t tid = -1;
    int thread_slot_count = atomic_load_explicit(&g_thread_slot_count, memory_order_acquire);

    if (thread_slot >= 0 && thread_slot < thread_slot_count) {
        tid = g_thread_slots[thread_slot].tid;
    }

    dprintf(2, "T%d(tid=%d)", thread_slot, tid);
}

/**
 * Print a lock operation event.
 * This is debug output and can be disabled with LOCKDEP_DEBUG=0.
 */
void lockdep_log_lock_event(const char *op, pthread_mutex_t *mutex, int rc) {
    if (!g_debug_enabled) {
        return;
    }

    dprintf(2,
            "[LOCKDEP][debug] tid=%d op=%s mutex=%p rc=%d\n",
            gettid(),
            op,
            (void *)mutex,
            rc);
}

/**
 * Print the currently held lock slots before acquiring a new lock slot.
 * This is debug output and can be disabled with LOCKDEP_DEBUG=0.
 */
void lockdep_log_held_lock_slots(int new_lock_slot) {
    if (!g_debug_enabled) {
        return;
    }

    dprintf(2, "[LOCKDEP][debug] tid=%d acquire ", gettid());
    lockdep_print_lock_slot(new_lock_slot);
    dprintf(2, " held=[");

    for (int i = 0; i < tls_thread_state.held_lock_slot_count; i++) {
        lockdep_print_lock_slot(tls_thread_state.held_lock_slots[i]);
        if (i + 1 < tls_thread_state.held_lock_slot_count) {
            dprintf(2, ", ");
        }
    }

    dprintf(2, "]\n");
}

/**
 * Report a potential deadlock in a lockdep-style format.
 * existing_chain is the already-existing path Lto -> ... -> Lfrom.
 */
void lockdep_report_potential_deadlock(int from_lock_slot,
                                       int to_lock_slot,
                                       const int *existing_chain,
                                       int existing_chain_len) {
    dprintf(2, "[LOCKDEP] ========================================\n");
    dprintf(2, "[LOCKDEP] POTENTIAL DEADLOCK DETECTED\n");
    dprintf(2, "[LOCKDEP] new dependency: ");
    lockdep_print_lock_slot(from_lock_slot);
    dprintf(2, " -> ");
    lockdep_print_lock_slot(to_lock_slot);
    dprintf(2, "\n");

    if (existing_chain_len > 0) {
        dprintf(2, "[LOCKDEP] existing chain: ");
        for (int i = 0; i < existing_chain_len; i++) {
            lockdep_print_lock_slot(existing_chain[i]);
            if (i + 1 < existing_chain_len) {
                dprintf(2, " -> ");
            }
        }
        dprintf(2, "\n");

        dprintf(2, "[LOCKDEP] cycle: ");
        lockdep_print_lock_slot(from_lock_slot);
        dprintf(2, " -> ");
        for (int i = 0; i < existing_chain_len; i++) {
            lockdep_print_lock_slot(existing_chain[i]);
            if (i + 1 < existing_chain_len) {
                dprintf(2, " -> ");
            }
        }
        dprintf(2, "\n");
    }

    dprintf(2, "[LOCKDEP] ========================================\n");
}

/**
 * Report an actual deadlock with the full wait chain.
 * For each edge i:
 *   thread_chain[i] -> lock_chain[i] -> thread_chain[i + 1]
 */
void lockdep_report_actual_deadlock(const int *thread_chain,
                                    const int *lock_chain,
                                    int edge_count) {
    dprintf(2, "[LOCKDEP] ========================================\n");
    dprintf(2, "[LOCKDEP] ACTUAL DEADLOCK DETECTED\n");
    dprintf(2, "[LOCKDEP] wait chain: ");

    if (edge_count > 0) {
        lockdep_print_thread_slot(thread_chain[0]);
        for (int i = 0; i < edge_count; i++) {
            dprintf(2, " -> ");
            lockdep_print_lock_slot(lock_chain[i]);
            dprintf(2, " -> ");
            lockdep_print_thread_slot(thread_chain[i + 1]);
        }
        dprintf(2, "\n");
    }

    dprintf(2, "[LOCKDEP] ========================================\n");
}
