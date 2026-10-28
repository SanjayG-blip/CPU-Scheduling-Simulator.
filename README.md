# CPU Scheduling Simulator (DSA Project in C)

A menu-driven CPU scheduling simulator written in C. It runs five scheduling algorithms on a set of processes, draws a text Gantt chart, and compares their average waiting, turnaround and response times.

A browser version of the same simulator is included in `index.html` (open it directly, or host it with GitHub Pages).

## Algorithms

| Algorithm | Type | Ready-queue data structure |
|---|---|---|
| FCFS (First Come First Served) | Non-preemptive | Circular queue |
| SJF (Shortest Job First) | Non-preemptive | Min-heap (key: burst time) |
| SRTF (Shortest Remaining Time First) | Preemptive | Min-heap (key: remaining time) |
| Priority Scheduling | Non-preemptive | Min-heap (key: priority) |
| Round Robin | Preemptive | Circular queue + time quantum |

Other data structures used: structs and arrays for the process table and Gantt chart, and insertion sort to order processes by arrival time.

## Metrics

- Completion time (CT)
- Turnaround time (TAT) = CT - arrival time
- Waiting time (WT) = TAT - burst time
- Response time (RT) = first CPU time - arrival time
- CPU utilization, plus idle gaps shown in the Gantt chart

## Build and run

```bash
gcc -Wall -o scheduler scheduler.c
./scheduler
```

On Windows (MinGW): `gcc -o scheduler.exe scheduler.c` then `scheduler.exe`.

Choose option 2 to load sample data, then run any algorithm, or option 9 to compare them all.

## Sample output

Sample data (arrival, burst, priority): P1 (0, 8, 3), P2 (1, 4, 1), P3 (2, 9, 4), P4 (3, 5, 2), P5 (6, 2, 5)

```
=== SRTF (Preemptive SJF) ===

 Gantt Chart:
 |P1   |P2   |P4   |P5   |P4   |P1   |P3   |
 0     1     5     6     8     12    19    28

 PID   AT    BT    CT    TAT   WT    RT
 ------------------------------------------
 P1    0     8     19    19    11    0
 P2    1     4     5     4     0     0
 P3    2     9     28    26    17    17
 P4    3     5     12    9     4     2
 P5    6     2     8     2     0     0

 Average Waiting Time    : 6.40
 Average Turnaround Time : 12.00
 Average Response Time   : 3.80
 CPU Utilization         : 100.00%
```

Comparison (Round Robin quantum = 2):

| Algorithm | Avg WT | Avg TAT | Avg RT |
|---|---|---|---|
| FCFS | 11.00 | 16.60 | 11.00 |
| SJF (Non-preemptive) | 7.80 | 13.40 | 7.80 |
| SRTF (Preemptive SJF) | 6.40 | 12.00 | 3.80 |
| Priority (Non-preemptive) | 10.20 | 15.80 | 10.20 |
| Round Robin | 12.60 | 18.20 | 2.80 |

SRTF gives the lowest average waiting time, and Round Robin gives the best response time.

## Project structure

```
.
├── scheduler.c   # C implementation (main project)
├── index.html    # browser demo of the same algorithms
└── README.md
```

## Limits

- Up to 20 processes
- Arrival and burst times up to 1000
- Priority: lower number means higher priority
- When two processes tie, the one that arrived first runs first, then the lower PID

## Publish on GitHub

```bash
git init
git add scheduler.c index.html README.md
git commit -m "CPU scheduling simulator in C"
git branch -M main
git remote add origin https://github.com/<your-username>/cpu-scheduling-simulator.git
git push -u origin main
```

To get a live demo link for `index.html`: on GitHub open **Settings > Pages**, choose **Deploy from a branch**, select `main` and `/ (root)`, then save. The page will be available at `https://<your-username>.github.io/cpu-scheduling-simulator/`.
