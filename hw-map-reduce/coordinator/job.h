/**
 * Logic for job and task management.
 *
 * You are not required to modify this file.
 */

#ifndef JOB_H__
#define JOB_H__

/* You may add definitions here */

/* states in job's life cycle. */
enum job_status {
    JOB_READY,      /* Job ready to be scheduled. */
    JOB_RUNNING,    /* Job is currently running. */
    JOB_CRASHED,    /* Job crashed. */
    JOB_FINISHED,   /* Job finished. */
};

typedef struct {
    u_int args_len;
    char *args_val;
} args;

typedef struct {
    enum job_status status;     /* current stauts of this job */
    args aux;                   /* aux argument for map and reduce function */
} job_info;

#endif
