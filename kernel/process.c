#include "kernel.h"

extern u32 system_uptime;

#define MAX_PROCESSES 32
#define PROCESS_STACK_SIZE 4096
#define TIME_SLICE 10

typedef enum {
    PROC_UNUSED,
    PROC_READY,
    PROC_RUNNING,
    PROC_WAITING,
    PROC_ZOMBIE
} process_state_t;

typedef struct {
    u32 pid;
    u32 ppid;
    process_state_t state;
    char name[32];
    u32 esp;
    u32 ebp;
    u32 eip;
    u32 stack[PROCESS_STACK_SIZE / 4];
    u32 time_used;
    u32 priority;
    u32 uid;
    u32 gid;
    u8 is_user;
} process_t;

static process_t processes[MAX_PROCESSES];
static u32 current_pid = 0;
static u32 next_pid = 1;
static u32 tick_count = 0;

void proc_init(void) {
    memset(processes, 0, sizeof(processes));

    process_t* idle = &processes[0];
    idle->pid = 0;
    idle->state = PROC_READY;
    strcpy(idle->name, "idle");
    idle->is_user = 0;

    proc_create_user("ash", 0, 1);
}

int proc_create(const char* name, u64 entry) {
    return proc_create_user(name, (u32)entry, 1);
}

int proc_create_user(const char* name, u32 entry, u8 is_user) {
    if (next_pid >= MAX_PROCESSES) return -1;

    u32 pid = next_pid++;
    process_t* proc = &processes[pid];

    memset(proc, 0, sizeof(process_t));
    proc->pid = pid;
    proc->ppid = current_pid;
    proc->state = PROC_READY;
    proc->eip = entry;
    proc->is_user = is_user;
    proc->uid = is_user ? 1000 : 0;
    strcpy(proc->name, name);

    /* Setup stack - simple for now */
    u32* stack_top = proc->stack + (PROCESS_STACK_SIZE / 4) - 1;
    proc->esp = (u32)stack_top;
    proc->ebp = (u32)stack_top;

    return pid;
}

void proc_yield(void) {
    __asm__ volatile ("int $0x20");
}

void proc_exit(int code) {
    (void)code;
    process_t* proc = &processes[current_pid];
    proc->state = PROC_ZOMBIE;
    proc_yield();
}

static void schedule(void) {
    u32 next = (current_pid + 1) % MAX_PROCESSES;
    u32 start = next;

    while (next != start) {
        if (processes[next].state == PROC_READY ||
            processes[next].state == PROC_RUNNING) {
            processes[next].state = PROC_RUNNING;
            current_pid = next;
            return;
        }
        next = (next + 1) % MAX_PROCESSES;
    }

    current_pid = 0;
    processes[0].state = PROC_RUNNING;
}

void timer_handler(void) {
    tick_count++;
    system_uptime++;
    if (processes[current_pid].pid != 0) {
        processes[current_pid].time_used++;

        if (processes[current_pid].time_used >= TIME_SLICE) {
            processes[current_pid].state = PROC_READY;
            processes[current_pid].time_used = 0;
            schedule();
        }
    }
}

int proc_get_list(char* buffer, usize size) {
    usize pos = 0;

    pos += snprintf(buffer + pos, size - pos, "  PID PPID STATE     USER COMMAND\n");
    pos += snprintf(buffer + pos, size - pos, "--------------------------------\n");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_UNUSED) {
            const char* state_str;
            switch (processes[i].state) {
                case PROC_READY:   state_str = "READY   "; break;
                case PROC_RUNNING: state_str = "RUNNING "; break;
                case PROC_WAITING: state_str = "WAITING "; break;
                case PROC_ZOMBIE:  state_str = "ZOMBIE  "; break; /* funniest thing yet */
                default:           state_str = "UNKNOWN "; break;
            }

            pos += snprintf(buffer + pos, size - pos,
                "%5d %5d %s %5d %s\n",
                processes[i].pid,
                processes[i].ppid,
                state_str,
                processes[i].uid,
                processes[i].name);
        }
    }

    return pos;
}

/* gitttttt current PID */
u32 proc_get_pid(void) {
    return processes[current_pid].pid;
}
