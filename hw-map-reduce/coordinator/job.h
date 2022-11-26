/**
 * Logic for job and task management.
 *
 * You are not required to modify this file.
 */
#include "../rpc/rpc.h"
#include <glib.h>

#ifndef JOB_H__
#define JOB_H__

enum job_status {
    // JOB_ALL_MAP_TASKS_ASSIGNED,
    JOB_MAP_DONE,    /* Job is done running map tasks. */
    JOB_FAILED,      /* Job crashed. */
    JOB_DONE,        /* Job finished. */
};

enum task_status {
    // TASK_ASSIGNED,  /* Task assigned and is running. */
    TASK_FAILED,    /* Task crashed. */
    TASK_DONE,      /* Task finished. */
};

typedef char *path;

struct {
    int files_len;
    path *files_val;
} files;

typedef struct {
    int args_len;
    char *args_val;
} args;

/* Map/Reduce task */
typedef struct {
    enum task_status status;     /* current status of this task */
    int task_id;                 /* Map/Reduce task number. */
    path file;                   /* Input file to this task. NULL if is_reduce. */
    int is_reduce;               /* 1 if this task is reduce. 0 otherwise. */
} task_info;

typedef struct {
    GList* mtask_queue;          /* FIFO map task queue. Each element is a taskId */
    GHashTable* mtask_map;       /* Map for mapTask and its taskID */
    GList* rtask_queue;          /* FIFO reduce task queue. Each element is a taskId */
    GHashTable* rtask_map;       /* Map for reduceTask and its taskID */

    int job_id;
    enum job_status status;     /* current status of this job */
    char *app;                  /* MapReduce application */
    int n_reduce;               /* Desired number of reduce tasks */
    int n_map;                  /* Number of map tasks. */
    path output_dir;            /* output directory */
    args args;                  /* Auxiliary arguments for map and reduce function */
} job_info;

job_info* accept_job(submit_job_request* job_request);

int *get_job_status(GHashTable* job_map, int job_id);

void set_next_task(GList* job_queue, GHashTable* job_map, get_task_reply* next_task);

void complete_task(GHashTable* job_map, finish_task_request* task_req);

#endif
