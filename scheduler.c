/*
 * CPU Scheduling Simulator  -  DSA Project in C
 *
 * Algorithms : FCFS, SJF (non-preemptive), SRTF (preemptive SJF),
 *              Priority (non-preemptive), Round Robin
 *
 * Data structures used
 *   - Circular Queue : ready queue for FCFS and Round Robin
 *   - Min-Heap       : ready queue for SJF, SRTF and Priority
 *   - Arrays/structs : process table and Gantt chart segments
 *   - Insertion sort : ordering processes by arrival time
 *
 * Build : gcc -Wall -o scheduler scheduler.c
 * Run   : ./scheduler
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_P     20      /* max processes            */
#define MAX_SEG   1000    /* max Gantt chart segments */
#define IDLE_PID  0       /* pid used for CPU idle    */

/* ------------------------------------------------------------------ */
/*  Basic types                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    int pid;        /* process id (1..n)                 */
    int at;         /* arrival time                      */
    int bt;         /* burst time                        */
    int pr;         /* priority (lower number = higher)  */
    int rt;         /* remaining time (working copy)     */
    int firstRun;   /* time first scheduled, -1 if never */
    int ct;         /* completion time                   */
    int tat;        /* turnaround time = ct - at         */
    int wt;         /* waiting time    = tat - bt        */
    int resp;       /* response time   = firstRun - at   */
} Process;

typedef struct {
    int pid, start, end;
} Segment;

typedef struct {
    Segment seg[MAX_SEG];
    int count;
} Gantt;

typedef struct {
    double avgWT, avgTAT, avgRT, util;
} Stats;

typedef enum {
    ALGO_FCFS, ALGO_SJF, ALGO_SRTF, ALGO_PRIORITY, ALGO_RR, ALGO_COUNT
} Algo;

static const char *ALGO_NAME[ALGO_COUNT] = {
    "FCFS", "SJF (Non-preemptive)", "SRTF (Preemptive SJF)",
    "Priority (Non-preemptive)", "Round Robin"
};

/* ------------------------------------------------------------------ */
/*  Circular Queue                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    int data[MAX_P];
    int front, rear, count;
} Queue;

static void qInit(Queue *q)              { q->front = q->rear = q->count = 0; }
static int  qEmpty(const Queue *q)       { return q->count == 0; }

static void qPush(Queue *q, int v) {
    q->data[q->rear] = v;
    q->rear = (q->rear + 1) % MAX_P;
    q->count++;
}

static int qPop(Queue *q) {
    int v = q->data[q->front];
    q->front = (q->front + 1) % MAX_P;
    q->count--;
    return v;
}

/* ------------------------------------------------------------------ */
/*  Min-Heap of process indices                                        */
/*  The heap key can be burst time, priority or remaining time.        */
/*  Ties are broken by arrival time, then by pid.                      */
/* ------------------------------------------------------------------ */

typedef enum { KEY_BURST, KEY_PRIORITY, KEY_REMAINING } KeyType;

typedef struct {
    int      data[MAX_P];
    int      size;
    KeyType  key;
    Process *procs;
} MinHeap;

static void heapInit(MinHeap *h, Process *procs, KeyType key) {
    h->size = 0;
    h->key = key;
    h->procs = procs;
}

static int heapEmpty(const MinHeap *h) { return h->size == 0; }

static int keyOf(const MinHeap *h, int i) {
    const Process *p = &h->procs[i];
    switch (h->key) {
        case KEY_BURST:    return p->bt;
        case KEY_PRIORITY: return p->pr;
        default:           return p->rt;
    }
}

/* returns 1 if process a should be served before process b */
static int before(const MinHeap *h, int a, int b) {
    int ka = keyOf(h, a), kb = keyOf(h, b);
    if (ka != kb) return ka < kb;
    if (h->procs[a].at != h->procs[b].at) return h->procs[a].at < h->procs[b].at;
    return h->procs[a].pid < h->procs[b].pid;
}

static void swapInt(int *a, int *b) { int t = *a; *a = *b; *b = t; }

static void heapPush(MinHeap *h, int idx) {
    int i = h->size++;
    h->data[i] = idx;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (!before(h, h->data[i], h->data[parent])) break;
        swapInt(&h->data[i], &h->data[parent]);
        i = parent;
    }
}

static int heapPop(MinHeap *h) {
    int top = h->data[0];
    h->data[0] = h->data[--h->size];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = 2 * i + 2, best = i;
        if (l < h->size && before(h, h->data[l], h->data[best])) best = l;
        if (r < h->size && before(h, h->data[r], h->data[best])) best = r;
        if (best == i) break;
        swapInt(&h->data[i], &h->data[best]);
        i = best;
    }
    return top;
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/* insertion sort of indices by (arrival time, pid) */
static void sortByArrival(const Process *p, int n, int *order) {
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 && (p[order[j]].at > p[key].at ||
              (p[order[j]].at == p[key].at && p[order[j]].pid > p[key].pid))) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }
}

/* add a block to the Gantt chart, merging with the previous one if possible */
static void ganttAdd(Gantt *g, int pid, int start, int end) {
    if (end <= start) return;
    if (g->count > 0) {
        Segment *last = &g->seg[g->count - 1];
        if (last->pid == pid && last->end == start) { last->end = end; return; }
    }
    if (g->count < MAX_SEG) {
        g->seg[g->count].pid = pid;
        g->seg[g->count].start = start;
        g->seg[g->count].end = end;
        g->count++;
    }
}

static void complete(Process *p, int t) {
    p->ct   = t;
    p->tat  = p->ct - p->at;
    p->wt   = p->tat - p->bt;
    p->resp = p->firstRun - p->at;
}

/* ------------------------------------------------------------------ */
/*  Scheduling algorithms                                              */
/* ------------------------------------------------------------------ */

static void runFCFS(Process *p, int n, Gantt *g) {
    int order[MAX_P], t = 0;
    Queue q;
    qInit(&q);
    sortByArrival(p, n, order);
    for (int i = 0; i < n; i++) qPush(&q, order[i]);

    while (!qEmpty(&q)) {
        int i = qPop(&q);
        if (t < p[i].at) { ganttAdd(g, IDLE_PID, t, p[i].at); t = p[i].at; }
        p[i].firstRun = t;
        ganttAdd(g, p[i].pid, t, t + p[i].bt);
        t += p[i].bt;
        p[i].rt = 0;
        complete(&p[i], t);
    }
}

/* SJF and Priority share the same loop; only the heap key differs */
static void runNonPreemptive(Process *p, int n, Gantt *g, KeyType key) {
    int order[MAX_P], next = 0, done = 0, t = 0;
    MinHeap h;
    heapInit(&h, p, key);
    sortByArrival(p, n, order);

    while (done < n) {
        while (next < n && p[order[next]].at <= t) heapPush(&h, order[next++]);
        if (heapEmpty(&h)) {                       /* CPU idle until next arrival */
            int na = p[order[next]].at;
            ganttAdd(g, IDLE_PID, t, na);
            t = na;
            continue;
        }
        int i = heapPop(&h);
        p[i].firstRun = t;
        ganttAdd(g, p[i].pid, t, t + p[i].bt);
        t += p[i].bt;
        p[i].rt = 0;
        complete(&p[i], t);
        done++;
    }
}

static void runSRTF(Process *p, int n, Gantt *g) {
    int order[MAX_P], next = 0, done = 0, t = 0;
    MinHeap h;
    heapInit(&h, p, KEY_REMAINING);
    sortByArrival(p, n, order);

    while (done < n) {
        while (next < n && p[order[next]].at <= t) heapPush(&h, order[next++]);
        if (heapEmpty(&h)) {
            int na = p[order[next]].at;
            ganttAdd(g, IDLE_PID, t, na);
            t = na;
            continue;
        }
        int i = heapPop(&h);                       /* shortest remaining time */
        if (p[i].firstRun < 0) p[i].firstRun = t;
        ganttAdd(g, p[i].pid, t, t + 1);           /* run for one time unit   */
        t++;
        p[i].rt--;
        if (p[i].rt == 0) { complete(&p[i], t); done++; }
        else heapPush(&h, i);
    }
}

static void runRR(Process *p, int n, Gantt *g, int quantum) {
    int order[MAX_P], next = 0, done = 0, t = 0;
    Queue q;
    qInit(&q);
    sortByArrival(p, n, order);

    while (done < n) {
        while (next < n && p[order[next]].at <= t) qPush(&q, order[next++]);
        if (qEmpty(&q)) {
            int na = p[order[next]].at;
            ganttAdd(g, IDLE_PID, t, na);
            t = na;
            continue;
        }
        int i = qPop(&q);
        if (p[i].firstRun < 0) p[i].firstRun = t;
        int run = p[i].rt < quantum ? p[i].rt : quantum;
        ganttAdd(g, p[i].pid, t, t + run);
        t += run;
        p[i].rt -= run;
        /* processes that arrived during this slice go ahead of the preempted one */
        while (next < n && p[order[next]].at <= t) qPush(&q, order[next++]);
        if (p[i].rt == 0) { complete(&p[i], t); done++; }
        else qPush(&q, i);
    }
}

/* ------------------------------------------------------------------ */
/*  Simulation driver, statistics and printing                         */
/* ------------------------------------------------------------------ */

static Stats simulate(Algo a, const Process *input, int n, int quantum,
                      Process *out, Gantt *g) {
    memcpy(out, input, sizeof(Process) * n);
    for (int i = 0; i < n; i++) { out[i].rt = out[i].bt; out[i].firstRun = -1; }
    g->count = 0;

    switch (a) {
        case ALGO_FCFS:     runFCFS(out, n, g);                         break;
        case ALGO_SJF:      runNonPreemptive(out, n, g, KEY_BURST);     break;
        case ALGO_SRTF:     runSRTF(out, n, g);                         break;
        case ALGO_PRIORITY: runNonPreemptive(out, n, g, KEY_PRIORITY);  break;
        case ALGO_RR:       runRR(out, n, g, quantum);                  break;
        default: break;
    }

    Stats s = {0, 0, 0, 0};
    int busy = 0;
    for (int i = 0; i < n; i++) {
        s.avgWT  += out[i].wt;
        s.avgTAT += out[i].tat;
        s.avgRT  += out[i].resp;
        busy     += out[i].bt;
    }
    s.avgWT /= n; s.avgTAT /= n; s.avgRT /= n;
    int total = g->count ? g->seg[g->count - 1].end : 0;
    s.util = total ? 100.0 * busy / total : 0;
    return s;
}

static void printProcesses(const Process *p, int n) {
    printf("\n PID   Arrival   Burst   Priority\n");
    printf(" ---------------------------------\n");
    for (int i = 0; i < n; i++)
        printf(" P%-4d %-9d %-7d %d\n", p[i].pid, p[i].at, p[i].bt, p[i].pr);
}

static void printGantt(const Gantt *g) {
    printf("\n Gantt Chart:\n ");
    for (int i = 0; i < g->count; i++) {
        char label[16];
        if (g->seg[i].pid == IDLE_PID) strcpy(label, "IDLE");
        else sprintf(label, "P%d", g->seg[i].pid);
        printf("|%-5s", label);
    }
    printf("|\n ");
    for (int i = 0; i < g->count; i++) printf("%-6d", g->seg[i].start);
    if (g->count) printf("%d", g->seg[g->count - 1].end);
    printf("\n");
}

static void printResult(Algo a, const Process *p, int n, const Gantt *g, const Stats *s) {
    printf("\n=== %s ===\n", ALGO_NAME[a]);
    printGantt(g);
    printf("\n PID   AT    BT    CT    TAT   WT    RT\n");
    printf(" ------------------------------------------\n");
    for (int i = 0; i < n; i++)
        printf(" P%-4d %-5d %-5d %-5d %-5d %-5d %d\n",
               p[i].pid, p[i].at, p[i].bt, p[i].ct, p[i].tat, p[i].wt, p[i].resp);
    printf("\n Average Waiting Time    : %.2f", s->avgWT);
    printf("\n Average Turnaround Time : %.2f", s->avgTAT);
    printf("\n Average Response Time   : %.2f", s->avgRT);
    printf("\n CPU Utilization         : %.2f%%\n", s->util);
}

static void compareAll(const Process *input, int n, int quantum) {
    Process work[MAX_P];
    Gantt g;
    Stats st[ALGO_COUNT];
    int best = 0;

    for (int a = 0; a < ALGO_COUNT; a++) {
        st[a] = simulate((Algo)a, input, n, quantum, work, &g);
        if (st[a].avgWT < st[best].avgWT) best = a;
    }
    printf("\n=== Comparison (Round Robin quantum = %d) ===\n", quantum);
    printf("\n %-27s %-9s %-9s %-9s\n", "Algorithm", "Avg WT", "Avg TAT", "Avg RT");
    printf(" ----------------------------------------------------------\n");
    for (int a = 0; a < ALGO_COUNT; a++)
        printf(" %-27s %-9.2f %-9.2f %-9.2f\n",
               ALGO_NAME[a], st[a].avgWT, st[a].avgTAT, st[a].avgRT);
    printf("\n Lowest average waiting time: %s\n", ALGO_NAME[best]);
}

/* ------------------------------------------------------------------ */
/*  Input handling                                                     */
/* ------------------------------------------------------------------ */

static int readInt(const char *prompt, int lo, int hi) {
    char buf[64], extra;
    int v;
    for (;;) {
        printf("%s", prompt);
        if (!fgets(buf, sizeof buf, stdin)) exit(0);
        if (sscanf(buf, "%d %c", &v, &extra) == 1 && v >= lo && v <= hi) return v;
        printf("  Please enter a whole number between %d and %d.\n", lo, hi);
    }
}

static int enterProcesses(Process *p) {
    int n = readInt("Number of processes (1-20): ", 1, MAX_P);
    for (int i = 0; i < n; i++) {
        printf("Process P%d\n", i + 1);
        p[i].pid = i + 1;
        p[i].at  = readInt("  Arrival time (0-1000): ", 0, 1000);
        p[i].bt  = readInt("  Burst time   (1-1000): ", 1, 1000);
        p[i].pr  = readInt("  Priority     (1-100, lower = higher): ", 1, 100);
    }
    return n;
}

static int loadSample(Process *p) {
    /* AT  BT  PR */
    int sample[5][3] = { {0, 8, 3}, {1, 4, 1}, {2, 9, 4}, {3, 5, 2}, {6, 2, 5} };
    for (int i = 0; i < 5; i++) {
        p[i].pid = i + 1;
        p[i].at  = sample[i][0];
        p[i].bt  = sample[i][1];
        p[i].pr  = sample[i][2];
    }
    return 5;
}

static void printMenu(void) {
    printf("\n========== CPU SCHEDULING SIMULATOR ==========\n");
    printf(" 1. Enter processes\n");
    printf(" 2. Load sample data\n");
    printf(" 3. Show processes\n");
    printf(" 4. FCFS\n");
    printf(" 5. SJF (Non-preemptive)\n");
    printf(" 6. SRTF (Preemptive SJF)\n");
    printf(" 7. Priority (Non-preemptive)\n");
    printf(" 8. Round Robin\n");
    printf(" 9. Compare all algorithms\n");
    printf(" 0. Exit\n");
}

int main(void) {
    Process input[MAX_P], work[MAX_P];
    Gantt   g;
    int     n = 0;

    for (;;) {
        printMenu();
        int choice = readInt("Choice: ", 0, 9);

        if (choice == 0) { printf("Goodbye!\n"); break; }
        if (choice == 1) { n = enterProcesses(input); continue; }
        if (choice == 2) { n = loadSample(input); printProcesses(input, n); continue; }
        if (n == 0)      { printf("\nNo processes yet. Use option 1 or 2 first.\n"); continue; }
        if (choice == 3) { printProcesses(input, n); continue; }

        int quantum = 0;
        if (choice == 8 || choice == 9)
            quantum = readInt("Time quantum (1-100): ", 1, 100);

        if (choice == 9) { compareAll(input, n, quantum); continue; }

        Algo a = (Algo)(choice - 4);   /* menu 4..8 maps to algorithms 0..4 */
        Stats s = simulate(a, input, n, quantum, work, &g);
        printResult(a, work, n, &g, &s);
    }
    return 0;
}
