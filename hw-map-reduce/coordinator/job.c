/**
 * Logic for job and task management.
 *
 * You are not required to modify this file.
 */

#include "job.h"

/* Helper function. */
task_info* accept_task(int task_id, path *input_files, int is_reduce) {
    task_info* new_task = malloc(sizeof(task_info));

    new_task->task_id = task_id;
    new_task->is_reduce = is_reduce;

    if (is_reduce == 0) {
        new_task->file = strdup(input_files[task_id]);
    } 
    new_task->status = TASK_READY;
    return new_task;
}

/* Accept job and store it to process later. */
job_info* accept_job(submit_job_request* job_request) {
    job_info* new_job = malloc(sizeof(job_info)); /* Store on heap since hash table only stores pointer. */

    new_job->n_map = job_request->files.files_len;
    // initialize and populate mtask queues and maps    
    new_job->mtask_queue = NULL;
    new_job->mtask_map = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
    for (int i = 0; i < new_job->n_map; i++) {
        new_job->mtask_queue = g_list_append(new_job->mtask_queue, GINT_TO_POINTER(i));
        task_info* mtask = accept_task(i, job_request->files.files_val, 0);
        g_hash_table_insert(new_job->mtask_map, GINT_TO_POINTER(i), mtask);
    }

    new_job->n_reduce = job_request->n_reduce;
    // initialize and populate rtask queues and maps
    new_job->rtask_queue = NULL;
    new_job->rtask_map = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
    for (int i = 0; i < new_job->n_reduce; i++) {
        new_job->rtask_queue = g_list_append(new_job->rtask_queue, GINT_TO_POINTER(i));
        task_info* rtask = accept_task(i, NULL, 1);
        g_hash_table_insert(new_job->rtask_map, GINT_TO_POINTER(i), rtask);
    }

    // copy output_dir
    new_job->output_dir = strdup(job_request->output_dir);

    // copy app
    new_job->app = strdup(job_request->app);

    // copy args
    args *args_cpy = malloc(sizeof(args));
    args_cpy->args_len = job_request->args.args_len;
    args_cpy->args_val = malloc(sizeof(char) * args_cpy->args_len);
    memcpy(args_cpy->args_val, job_request->args.args_val, job_request->args.args_len);
    new_job->args = *args_cpy;

    new_job->status = JOB_READY;
    return new_job;
}

/* Get job status. */
int *get_job_status(GHashTable* job_map, int job_id) {
    static int result[3] = {0, 0, 0}; /* {done, failed, invalid_job_id} */

    job_info *existing_job = g_hash_table_lookup(job_map, GINT_TO_POINTER(job_id));
    if (existing_job == NULL) {
        // printf("existing job is invalid!\n");
        result[0] = 0;
        result[1] = 0;
        result[2] = 1;
    } else {
        enum job_status status = existing_job->status;
        // printf("existing job is valid!\n");
        if (status == JOB_DONE) {
            // printf("job done!\n");
            result[0] = 1;
            result[1] = 0;
            result[2] = 0;
        } else if (status == JOB_FAILED) {
            // printf("job failed!\n");
            result[0] = 1;
            result[1] = 1;
            result[2] = 0;
        } else {
            // printf("job in progress!\n");
            result[0] = 0;
            result[1] = 0;
            result[2] = 0;
        }
    }

    return result;
}

/* Get highest priority job that is yet to finished. */
job_info* get_highest_prio_job(GList* job_queue, GHashTable* job_map) {
    for (GList* elem = job_queue; elem; elem = elem->next) {
        int job_id = GPOINTER_TO_INT(elem->data); /* Cast data back to an integer. */
        job_info *existing_job = g_hash_table_lookup(job_map, GINT_TO_POINTER(job_id));
        if (existing_job->status == JOB_READY || existing_job->status == JOB_MAP_IP ||
            existing_job->status == JOB_MAP_DONE || existing_job->status == JOB_REDUCE_IP) {
            return existing_job;
        }
    }
    return NULL;
}

/* Get highest priority task that is yet to be processed. */
task_info* get_highest_prio_task(GList* task_queue, GHashTable* task_map) {
    for (GList* elem = task_queue; elem; elem = elem->next) {
        int task_id = GPOINTER_TO_INT(elem->data); /* Cast data back to an integer. */
        task_info *existing_task = g_hash_table_lookup(task_map, GINT_TO_POINTER(task_id));
        if (existing_task->status == TASK_IN_PROGRESS) {
            // handle worker failures by immediately assiging the task to another worker
            time_t secs_since_assigned = time(NULL) - existing_task->assigned_time;
            // printf("Seconds since start of program = %ld\n", secs_since_assigned);
            if (secs_since_assigned > TASK_TIMEOUT_SECS) {
                // printf("##############################################3\n");
                // printf("Reassigning this task to another worker\n");
                // printf("##############################################3\n");
                // existing_task->status = TASK_SHOULD_REASSIGN;
                return existing_task;
            }
        }
        if (existing_task->status == TASK_READY) {
            return existing_task;
        }
    }
    return NULL;
}

/* Helper function: returns the number of finsihed tasks. */
int num_task_finished(GList* task_queue, GHashTable* task_map) {
    int counter = 0;
    for (GList* elem = task_queue; elem; elem = elem->next) {
        int task_id = GPOINTER_TO_INT(elem->data); /* Cast data back to an integer. */
        task_info *existing_task = g_hash_table_lookup(task_map, GINT_TO_POINTER(task_id));
        if (existing_task->status == TASK_DONE) {
            counter++;
        }
    }
    return counter;
}

/* Sets the next task to be assigned to result. */
void set_next_task(GList* job_queue, GHashTable* job_map, get_task_reply* result) {
    result->file = "";
    result->output_dir = "";
    result->app = "";
    result->wait = true;
    result->args.args_len = 0;

    /* find highest priority job that is ready to be processed. */
    job_info *next_job = get_highest_prio_job(job_queue, job_map);    
    if (next_job != NULL) {
        // printf("next job to be assigned: %d\n", next_job->job_id);
        result->job_id = next_job->job_id;
        result->app = next_job->app;
        result->n_reduce = next_job->n_reduce;
        result->n_map = next_job->n_map;
        result->output_dir = next_job->output_dir;
        result->args.args_len = next_job->args.args_len;
        result->args.args_val = malloc(sizeof(char) * result->args.args_len);
        memcpy(result->args.args_val, next_job->args.args_val, next_job->args.args_len);
        result->wait = false;

        /* from the highest priority job, find highest priority ready map task */
        task_info *next_mtask = get_highest_prio_task(next_job->mtask_queue, next_job->mtask_map);
        if (next_mtask != NULL) {
            // printf("next map task to be assigned: %d\n", next_mtask->task_id);
            result->file = next_mtask->file;
            result->task = next_mtask->task_id;
            result->reduce = false;

            next_mtask->assigned_time = time(NULL);
            // printf("Map Assigned time = %ld\n", next_mtask->assigned_time);
            next_mtask->status = TASK_IN_PROGRESS;
            next_job->status = JOB_MAP_IP;
        } else {
            // no more map task left to be scheduled
            // check if all map tasks are finished
            if (next_job->status == JOB_MAP_DONE || next_job->status == JOB_REDUCE_IP) {
                // reduce tasks can now be scheduled
                task_info *next_rtask = get_highest_prio_task(next_job->rtask_queue, next_job->rtask_map);
                if (next_rtask != NULL) {
                    // printf("next reduce task to be assigned: %d\n", next_rtask->task_id);
                    result->task = next_rtask->task_id;
                    result->reduce = true;

                    next_rtask->assigned_time = time(NULL);
                    // printf("Reduce Assigned time = %ld\n", next_rtask->assigned_time);
                    next_rtask->status = TASK_IN_PROGRESS;
                    next_job->status = JOB_REDUCE_IP;
                } else {
                    // all reduce tasks are now assigned
                    // have to wait until all reduce tasks are finished
                    // in the meantime, can assign tasks for another job
                    // printf("All reduce tasks are now assigned for job %d! In the meantime can assign tasks for another job.\n", next_job->job_id);
                    next_job->status = JOB_ALL_REDUCE_TASKS_ASSIGNED;
                    set_next_task(job_queue, job_map, result);
                }
            } else {
                // have to wait until all map tasks have finished
                // in the meantime can assign tasks for another job
                // printf("All map tasks are now assigned for job %d! In the meantime assign tasks for another job.\n", next_job->job_id);
                next_job->status = JOB_ALL_MAP_TASKS_ASSIGNED;
                set_next_task(job_queue, job_map, result);
            }
        }
    } else {
        // no more job left to be scheduled
        // printf("No more job left to be scheduled!\n");
        result->wait = true;
    }
}

/* Marks the task TASK_REQ as finished and updates its job accordingly. */
void complete_task(GHashTable* job_map, finish_task_request* task_req) {
    // printf("*******************************************************\n");
    job_info *existing_job = g_hash_table_lookup(job_map, GINT_TO_POINTER(task_req->job_id));
    if (existing_job) {
        // printf("completing job...\n");
        // update task status
        task_info *existing_task = NULL;
        if (task_req->reduce) {
            // printf("reduce task\n");
            existing_task = g_hash_table_lookup(existing_job->rtask_map, GINT_TO_POINTER(task_req->task));
        } else {
            // printf("map task\n");
            existing_task = g_hash_table_lookup(existing_job->mtask_map, GINT_TO_POINTER(task_req->task));
        }

        if (existing_task) {
            if (task_req->success) {
                // printf("marking task %d for job %d as finished\n", existing_task->task_id, existing_job->job_id);
                existing_task->status = TASK_DONE;
            } else {
                // printf("marking task %d for job %d as failed\n", existing_task->task_id, existing_job->job_id);
                existing_task->status = TASK_FAILED;
                existing_job->status = JOB_FAILED;
                return;
            }

            // update job status
            if (!task_req->reduce) {
                int num_mtask_finished = num_task_finished(existing_job->mtask_queue, existing_job->mtask_map);
                if (num_mtask_finished == existing_job->n_map) {
                    existing_job->status = JOB_MAP_DONE;
                    // printf("all map tasks for job %d are now finished!\n", existing_job->job_id);
                }
            } else {
                int num_rtask_finished = num_task_finished(existing_job->rtask_queue, existing_job->rtask_map);
                if (num_rtask_finished == existing_job->n_reduce) {
                    existing_job->status = JOB_DONE;
                    // printf("job %d is now finished!\n", existing_job->job_id);
                }
            }
            
        } else {
            // printf("unable to complete task because no such task id exists\n");
        }
    } else {
        // printf("unable to complete job because job id invalid...\n");
    }

    // printf("*******************************************************\n");
}
