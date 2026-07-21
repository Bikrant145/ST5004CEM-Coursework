/*
 * ST5004CEM - Task 1: Process Management and Threading
 * task1_scheduler.c
 *
 * Simulates round-robin CPU scheduling over a set of processes with
 * arrival times and burst times. Tracks waiting time and turnaround
 * time per process, and prints a Gantt-chart style execution order.
 *
 * Compile:  gcc task1_scheduler.c -o task1_scheduler
 * Run:      ./task1_scheduler
 */

#include <stdio.h>
#include <stdlib.h>

#define MAX_PROCESSES 10

typedef struct {
    int pid;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int completion_time;
    int waiting_time;
    int turnaround_time;
} Process;

typedef struct {
    int pid;
    int start;
    int end;
} TimelineEntry;

typedef struct {
    int items[MAX_PROCESSES * 50];
    int front, rear, count;
} Queue;

void queue_init(Queue *q) { q->front = 0; q->rear = 0; q->count = 0; }
void queue_push(Queue *q, int val) {
    q->items[q->rear] = val;
    q->rear = (q->rear + 1) % (MAX_PROCESSES * 50);
    q->count++;
}
int queue_pop(Queue *q) {
    int val = q->items[q->front];
    q->front = (q->front + 1) % (MAX_PROCESSES * 50);
    q->count--;
    return val;
}
int queue_empty(Queue *q) { return q->count == 0; }

void round_robin(Process procs[], int n, int quantum,
                  TimelineEntry timeline[], int *timeline_len) {
    int time_elapsed = 0;
    int completed = 0;
    int admitted[MAX_PROCESSES] = {0};
    Queue q;
    queue_init(&q);
    *timeline_len = 0;

    for (int i = 0; i < n; i++) {
        if (!admitted[i] && procs[i].arrival_time <= time_elapsed) {
            queue_push(&q, i);
            admitted[i] = 1;
        }
    }

    while (completed < n) {
        if (queue_empty(&q)) {
            int next_arrival = -1;
            for (int i = 0; i < n; i++) {
                if (!admitted[i] &&
                    (next_arrival == -1 || procs[i].arrival_time < next_arrival))
                    next_arrival = procs[i].arrival_time;
            }
            time_elapsed = next_arrival;
            for (int i = 0; i < n; i++) {
                if (!admitted[i] && procs[i].arrival_time <= time_elapsed) {
                    queue_push(&q, i);
                    admitted[i] = 1;
                }
            }
            continue;
        }

        int idx = queue_pop(&q);
        int run_time = (procs[idx].remaining_time < quantum)
                            ? procs[idx].remaining_time
                            : quantum;
        int start = time_elapsed;
        time_elapsed += run_time;
        procs[idx].remaining_time -= run_time;

        timeline[*timeline_len].pid = procs[idx].pid;
        timeline[*timeline_len].start = start;
        timeline[*timeline_len].end = time_elapsed;
        (*timeline_len)++;

        for (int i = 0; i < n; i++) {
            if (!admitted[i] && procs[i].arrival_time <= time_elapsed) {
                queue_push(&q, i);
                admitted[i] = 1;
            }
        }

        if (procs[idx].remaining_time > 0) {
            queue_push(&q, idx);
        } else {
            procs[idx].completion_time = time_elapsed;
            procs[idx].turnaround_time =
                procs[idx].completion_time - procs[idx].arrival_time;
            procs[idx].waiting_time =
                procs[idx].turnaround_time - procs[idx].burst_time;
            completed++;
        }
    }
}

void print_gantt_chart(TimelineEntry timeline[], int len) {
    for (int i = 0; i < len; i++)
        printf("| P%-2d ", timeline[i].pid);
    printf("|\n0");
    for (int i = 0; i < len; i++)
        printf("%6d", timeline[i].end);
    printf("\n");
}

void print_stats(Process procs[], int n) {
    printf("%-5s%-10s%-8s%-12s%-12s%-8s\n",
           "PID", "Arrival", "Burst", "Completion", "Turnaround", "Waiting");
    int total_wait = 0, total_turnaround = 0;
    for (int i = 0; i < n; i++) {
        printf("%-5d%-10d%-8d%-12d%-12d%-8d\n",
               procs[i].pid, procs[i].arrival_time, procs[i].burst_time,
               procs[i].completion_time, procs[i].turnaround_time,
               procs[i].waiting_time);
        total_wait += procs[i].waiting_time;
        total_turnaround += procs[i].turnaround_time;
    }
    printf("\nAverage Waiting Time:    %.2f\n", (double)total_wait / n);
    printf("Average Turnaround Time: %.2f\n", (double)total_turnaround / n);
}

int main(void) {
    int quantum = 4;
    int n = 4;

    Process procs[MAX_PROCESSES] = {
        {1, 0, 10, 10, 0, 0, 0},
        {2, 1, 5, 5, 0, 0, 0},
        {3, 2, 8, 8, 0, 0, 0},
        {4, 4, 3, 3, 0, 0, 0},
    };

    TimelineEntry timeline[MAX_PROCESSES * 50];
    int timeline_len = 0;

    printf("Round Robin Scheduling Simulation (quantum = %d)\n\n", quantum);
    round_robin(procs, n, quantum, timeline, &timeline_len);

    printf("Execution order (Gantt chart):\n");
    print_gantt_chart(timeline, timeline_len);

    printf("\nPer-process statistics:\n");
    print_stats(procs, n);

    return 0;
}
