#include "lockdep.h"

/**
 * Print a panic message and exit the program.
 * @param msg The panic message to print.
 */
void lockdep_panic(const char *msg) {
    dprintf(2, "%s", msg);
    _exit(1);
}

/**
 * Print a log message for a lock operation.
 * @param op The operation being performed (e.g., "lock", "unlock", "lock-fail").
 * @param mutex The mutex involved in the operation.
 * @param rc The return code from the operation.
 */
void lockdep_log(const char *op, pthread_mutex_t *mutex, int rc) {
    dprintf(2, "[LOCKDEP] tid=%d op=%s mutex=%p rc=%d\n",
            gettid(), op, (void *)mutex, rc);
}

/**
 * Print a log message for the held locks in a thread.
 * @param new_id The ID of the new lock being acquired.
 */
void lockdep_log_held_context(unsigned int new_id) {
    dprintf(2, "[LOCKDEP] tid=%d acquire id=%u held=[",
            gettid(), new_id);

    for (unsigned int i = 0; i < tls_state.held_count; i++) {
        dprintf(2, "%u%s",
                tls_state.held[i],
                (i + 1 < tls_state.held_count) ? "," : "");
    }

    dprintf(2, "]\n");
}
