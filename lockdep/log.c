#include "lockdep.h"

#include <dlfcn.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

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

static void lockdep_trim_newline(char *buf) {
    size_t len = strlen(buf);

    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }
}

static int lockdep_shell_quote(const char *src, char *dst, size_t dst_size) {
    size_t pos = 0;

    if (dst_size < 3) {
        return 0;
    }

    dst[pos++] = '\'';
    for (const char *p = src; *p != '\0'; p++) {
        if (*p == '\'') {
            if (pos + 4 >= dst_size) {
                return 0;
            }
            dst[pos++] = '\'';
            dst[pos++] = '\\';
            dst[pos++] = '\'';
            dst[pos++] = '\'';
        } else {
            if (pos + 1 >= dst_size) {
                return 0;
            }
            dst[pos++] = *p;
        }
    }

    if (pos + 1 >= dst_size) {
        return 0;
    }
    dst[pos++] = '\'';
    dst[pos] = '\0';
    return 1;
}

static int lockdep_addr2line_output_is_known(const char *line) {
    if (line[0] == '\0') {
        return 0;
    }

    if (strncmp(line, "??", 2) == 0 ||
        strstr(line, " at ??:") ||
        strstr(line, "??:0")) {
        return 0;
    }

    return 1;
}

static int lockdep_addr2line_lookup(const char *object_path,
                                    uintptr_t address,
                                    char *out,
                                    size_t out_size) {
    char quoted_path[PATH_MAX * 4];
    char command[PATH_MAX * 4 + 128];
    FILE *pipe;

    if (!object_path || object_path[0] == '\0') {
        return 0;
    }
    if (!lockdep_shell_quote(object_path, quoted_path, sizeof(quoted_path))) {
        return 0;
    }
    if (snprintf(command,
                 sizeof(command),
                 "addr2line -f -p -e %s 0x%lx 2>/dev/null",
                 quoted_path,
                 (unsigned long)address) >= (int)sizeof(command)) {
        return 0;
    }

    pipe = popen(command, "r");
    if (!pipe) {
        return 0;
    }

    if (!fgets(out, (int)out_size, pipe)) {
        pclose(pipe);
        return 0;
    }

    pclose(pipe);
    lockdep_trim_newline(out);
    return lockdep_addr2line_output_is_known(out);
}

static void lockdep_format_callsite(uintptr_t pc, char *out, size_t out_size) {
    Dl_info info;

    if (pc == (uintptr_t)0) {
        snprintf(out, out_size, "unknown");
        return;
    }

    if (dladdr((void *)pc, &info) && info.dli_fname) {
        uintptr_t relative_pc = pc;

        if (info.dli_fbase) {
            relative_pc = pc - (uintptr_t)info.dli_fbase;
            if (lockdep_addr2line_lookup(info.dli_fname, relative_pc, out, out_size)) {
                return;
            }
        }

        if (lockdep_addr2line_lookup(info.dli_fname, pc, out, out_size)) {
            return;
        }

        if (info.dli_sname) {
            uintptr_t symbol_offset = pc - (uintptr_t)info.dli_saddr;
            snprintf(out,
                     out_size,
                     "%s+0x%lx (%s)",
                     info.dli_sname,
                     (unsigned long)symbol_offset,
                     info.dli_fname);
            return;
        }

        snprintf(out, out_size, "%p (%s)", (void *)pc, info.dli_fname);
        return;
    }

    snprintf(out, out_size, "%p", (void *)pc);
}

static void lockdep_print_thread_callsite(const char *prefix,
                                          int thread_slot,
                                          uintptr_t pc) {
    char callsite[512];

    dprintf(2, "%s", prefix);
    lockdep_print_thread_slot(thread_slot);
    lockdep_format_callsite(pc, callsite, sizeof(callsite));
    dprintf(2, " at %s\n", callsite);
}

static void lockdep_print_edge_info(const char *prefix,
                                    const lockdep_edge_info_t *edge_info) {
    if (!edge_info || edge_info->acquire_pc == (uintptr_t)0) {
        dprintf(2, "%sunknown acquire site\n", prefix);
        return;
    }

    lockdep_print_thread_callsite(prefix,
                                  edge_info->thread_slot,
                                  edge_info->acquire_pc);
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
                                       int existing_chain_len,
                                       const lockdep_edge_info_t *new_edge_info,
                                       const lockdep_edge_info_t *existing_edge_infos) {
    dprintf(2, "[LOCKDEP] ========================================\n");
    dprintf(2, "[LOCKDEP] POTENTIAL DEADLOCK DETECTED\n");
    dprintf(2, "[LOCKDEP] new dependency: ");
    lockdep_print_lock_slot(from_lock_slot);
    dprintf(2, " -> ");
    lockdep_print_lock_slot(to_lock_slot);
    dprintf(2, "\n");
    if (LOCKDEP_UNLIKELY(g_report_sites_enabled)) {
        lockdep_print_edge_info("[LOCKDEP]   observed by ", new_edge_info);
    }

    if (existing_chain_len > 0) {
        dprintf(2, "[LOCKDEP] existing chain: ");
        for (int i = 0; i < existing_chain_len; i++) {
            lockdep_print_lock_slot(existing_chain[i]);
            if (i + 1 < existing_chain_len) {
                dprintf(2, " -> ");
            }
        }
        dprintf(2, "\n");

        if (LOCKDEP_UNLIKELY(g_report_sites_enabled)) {
            for (int i = 0; i + 1 < existing_chain_len; i++) {
                dprintf(2, "[LOCKDEP]   edge ");
                lockdep_print_lock_slot(existing_chain[i]);
                dprintf(2, " -> ");
                lockdep_print_lock_slot(existing_chain[i + 1]);
                dprintf(2, "\n");
                lockdep_print_edge_info("[LOCKDEP]     observed by ",
                                        &existing_edge_infos[i]);
            }
        }

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
    dprintf(2, "[LOCKDEP] wait chain:");

    if (edge_count > 0) {
        dprintf(2, " ");
        lockdep_print_thread_slot(thread_chain[0]);
        if (!LOCKDEP_UNLIKELY(g_report_sites_enabled)) {
            dprintf(2, "[LOCKDEP] ========================================\n");
            return;
        }

        for (int i = 0; i < edge_count; i++) {
            dprintf(2, " -> ");
            lockdep_print_lock_slot(lock_chain[i]);
            dprintf(2, " -> ");
            lockdep_print_thread_slot(thread_chain[i + 1]);
        }
        dprintf(2, "\n");

        for (int i = 0; i < edge_count; i++) {
            uintptr_t waiting_pc = (uintptr_t)0;
            uintptr_t owner_pc = (uintptr_t)0;

            if (thread_chain[i] >= 0 &&
                thread_chain[i] < atomic_load_explicit(&g_thread_slot_count,
                                                       memory_order_acquire)) {
                waiting_pc = atomic_load_explicit(
                    &g_thread_slots[thread_chain[i]].waiting_on_pc,
                    memory_order_acquire);
            }
            if (lock_chain[i] >= 0 &&
                lock_chain[i] < atomic_load_explicit(&g_lock_slot_count,
                                                     memory_order_acquire)) {
                owner_pc = atomic_load_explicit(
                    &g_lock_slots[lock_chain[i]].owner_acquire_pc,
                    memory_order_acquire);
            }

            dprintf(2, "[LOCKDEP]   ");
            lockdep_print_thread_slot(thread_chain[i]);
            dprintf(2, " waits for ");
            lockdep_print_lock_slot(lock_chain[i]);
            dprintf(2, " held by ");
            lockdep_print_thread_slot(thread_chain[i + 1]);
            dprintf(2, "\n");
            lockdep_print_thread_callsite("[LOCKDEP]     waiting site: ",
                                          thread_chain[i],
                                          waiting_pc);
            lockdep_print_thread_callsite("[LOCKDEP]     owner acquire: ",
                                          thread_chain[i + 1],
                                          owner_pc);
        }
    }

    dprintf(2, "[LOCKDEP] ========================================\n");
}
