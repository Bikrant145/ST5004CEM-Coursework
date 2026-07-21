
/*
 * ST5004CEM - Task 2: Memory Management Simulation
 * task2_paging.c
 *
 * Simulates virtual memory paging with a configurable number of frames
 * (physical memory slots). Implements two page replacement algorithms:
 *   - FIFO (First-In-First-Out)
 *   - LRU  (Least Recently Used)
 *
 * Tracks page faults, hits, and computes hit/miss ratios for each
 * algorithm over the same reference string, so they can be compared.
 *
 * Compile:  gcc task2_paging.c -o task2_paging
 * Run:      ./task2_paging
 */
 
#include <stdio.h>
#include <stdlib.h>
 
#define MAX_FRAMES 10
#define MAX_REFERENCES 100
 
/* ------------------------------------------------------------------- */
/* FIFO page replacement                                                */
/* ------------------------------------------------------------------- */
 
void run_fifo(int reference_string[], int n, int num_frames) {
    int frames[MAX_FRAMES];
    for (int i = 0; i < num_frames; i++)
        frames[i] = -1; /* -1 means empty slot */
 
    int fifo_pointer = 0; /* points to the oldest frame, to be replaced next */
    int page_faults = 0;
    int page_hits = 0;
 
    printf("\n--- FIFO Page Replacement (frames = %d) ---\n", num_frames);
 
    for (int i = 0; i < n; i++) {
        int page = reference_string[i];
        int found = 0;
 
        for (int f = 0; f < num_frames; f++) {
            if (frames[f] == page) {
                found = 1;
                break;
            }
        }
 
        if (found) {
            page_hits++;
            printf("Ref %2d: page %2d -> HIT   | Frames: ", i, page);
        } else {
            page_faults++;
            frames[fifo_pointer] = page;
            fifo_pointer = (fifo_pointer + 1) % num_frames;
            printf("Ref %2d: page %2d -> FAULT | Frames: ", i, page);
        }
 
        for (int f = 0; f < num_frames; f++) {
            if (frames[f] == -1)
                printf("_ ");
            else
                printf("%d ", frames[f]);
        }
        printf("\n");
    }
 
    double hit_ratio = (double)page_hits / n * 100.0;
    double miss_ratio = (double)page_faults / n * 100.0;
    printf("FIFO Summary: faults=%d, hits=%d, hit_ratio=%.2f%%, miss_ratio=%.2f%%\n",
           page_faults, page_hits, hit_ratio, miss_ratio);
}
 
/* ------------------------------------------------------------------- */
/* LRU page replacement                                                 */
/* ------------------------------------------------------------------- */
 
void run_lru(int reference_string[], int n, int num_frames) {
    int frames[MAX_FRAMES];
    int last_used[MAX_FRAMES]; /* the reference-index each frame was last used at */
    for (int i = 0; i < num_frames; i++) {
        frames[i] = -1;
        last_used[i] = -1;
    }
 
    int page_faults = 0;
    int page_hits = 0;
 
    printf("\n--- LRU Page Replacement (frames = %d) ---\n", num_frames);
 
    for (int i = 0; i < n; i++) {
        int page = reference_string[i];
        int found = -1;
 
        for (int f = 0; f < num_frames; f++) {
            if (frames[f] == page) {
                found = f;
                break;
            }
        }
 
        if (found != -1) {
            page_hits++;
            last_used[found] = i; /* mark as most recently used */
            printf("Ref %2d: page %2d -> HIT   | Frames: ", i, page);
        } else {
            page_faults++;
 
            /* find an empty frame first */
            int target = -1;
            for (int f = 0; f < num_frames; f++) {
                if (frames[f] == -1) {
                    target = f;
                    break;
                }
            }
 
            /* no empty frame: evict the least recently used one */
            if (target == -1) {
                int oldest_time = last_used[0];
                target = 0;
                for (int f = 1; f < num_frames; f++) {
                    if (last_used[f] < oldest_time) {
                        oldest_time = last_used[f];
                        target = f;
                    }
                }
            }
 
            frames[target] = page;
            last_used[target] = i;
            printf("Ref %2d: page %2d -> FAULT | Frames: ", i, page);
        }
 
        for (int f = 0; f < num_frames; f++) {
            if (frames[f] == -1)
                printf("_ ");
            else
                printf("%d ", frames[f]);
        }
        printf("\n");
    }
 
    double hit_ratio = (double)page_hits / n * 100.0;
    double miss_ratio = (double)page_faults / n * 100.0;
    printf("LRU Summary: faults=%d, hits=%d, hit_ratio=%.2f%%, miss_ratio=%.2f%%\n",
           page_faults, page_hits, hit_ratio, miss_ratio);
}
 
/* ------------------------------------------------------------------- */
/* Main - run both algorithms on the same reference string(s) so they  */
/* can be directly compared for the analysis report                    */
/* ------------------------------------------------------------------- */
 
int main(void) {
    /* Test case 1: classic textbook reference string */
    int ref1[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    int n1 = sizeof(ref1) / sizeof(ref1[0]);
 
    printf("========================================================\n");
    printf("TEST CASE 1: reference string length = %d, frames = 3\n", n1);
    printf("========================================================\n");
    run_fifo(ref1, n1, 3);
    run_lru(ref1, n1, 3);
 
    /* Test case 2: same string, more frames available */
    printf("\n========================================================\n");
    printf("TEST CASE 2: reference string length = %d, frames = 4\n", n1);
    printf("========================================================\n");
    run_fifo(ref1, n1, 4);
    run_lru(ref1, n1, 4);
 
    /* Test case 3: a string with strong locality, favours LRU */
    int ref2[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    int n2 = sizeof(ref2) / sizeof(ref2[0]);
 
    printf("\n========================================================\n");
    printf("TEST CASE 3: reference string length = %d, frames = 3\n", n2);
    printf("========================================================\n");
    run_fifo(ref2, n2, 3);
    run_lru(ref2, n2, 3);
 
    return 0;
}
