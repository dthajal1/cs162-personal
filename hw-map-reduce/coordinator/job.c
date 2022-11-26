/**
 * Logic for job and task management.
 *
 * You are not required to modify this file.
 */

#include "job.h"

task_info* accept_task(int task_id, path *input_files, int is_reduce) {
    task_info* new_task = malloc(sizeof(task_info));

    new_task->task_id = task_id;

    if (is_reduce) {
        new_task->is_reduce = 1;
    } else {
        new_task->file = strdup(input_files[task_id]); // input_files is not null terminated. Might cause error?
        new_task->is_reduce = 0;
    }

    // new_task->status = TASK_READY;
    return new_task;
}

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

    // new_job->status = JOB_READY;
    return new_job;
}

int *get_job_status(GHashTable* job_map, int job_id) {
    static int result[3] = {0, 0, 0}; /* {done, failed, invalid_job_id} */

    job_info *existing_job = g_hash_table_lookup(job_map, GINT_TO_POINTER(job_id));
    if (existing_job == NULL) {
        result[2] = 1;
    } else {
        enum job_status status = existing_job->status;
        if (status == JOB_DONE) {
            result[0] = 1;
        } else if (status == JOB_FAILED) {
            result[0] = 1;
            result[1] = 1;
        }
    }

    return result;
}

job_info* get_highest_prio_job(GList* job_queue, GHashTable* job_map) {
    for (GList* elem = job_queue; elem; elem = elem->next) {
        int job_id = GPOINTER_TO_INT(elem->data); /* Cast data back to an integer. */
        job_info *existing_job = g_hash_table_lookup(job_map, GINT_TO_POINTER(job_id));
        // if failed => mark the corresponding job as failed and stop assigning tasks for that job
        if (existing_job && (existing_job->status != JOB_FAILED 
            || existing_job->status != JOB_DONE)) {
            return existing_job;
        }
    }
    return NULL;
}

task_info* get_highest_prio_task(GList* task_queue, GHashTable* task_map) {
    for (GList* elem = task_queue; elem; elem = elem->next) {
        int task_id = GPOINTER_TO_INT(elem->data); /* Cast data back to an integer. */
        task_info *existing_task = g_hash_table_lookup(task_map, GINT_TO_POINTER(task_id));
        if (existing_task && existing_task->status != TASK_DONE) {
            // TODO: need more coditions? or if it has failed?
            return existing_task;
        }
    }
    return NULL;
}

void set_next_task(GList* job_queue, GHashTable* job_map, get_task_reply* result) {
    /* find highest priority job that is ready to be processed. */
    job_info *next_job = get_highest_prio_job(job_queue, job_map);    

    if (next_job != NULL) {
        result->job_id = next_job->job_id;
        result->app = next_job->app;
        result->n_reduce = next_job->n_reduce;
        result->n_map = next_job->n_map;
        result->output_dir = next_job->output_dir;
        result->args.args_len = next_job->args.args_len;
        result->args.args_val = malloc(sizeof(char) * result->args.args_len);
        memcpy(result->args.args_val, next_job->args.args_val, next_job->args.args_len);
        result->wait = 0;

        /* from the highest priority job, find highest priority map task */
        task_info *next_mtask = get_highest_prio_task(next_job->mtask_queue, next_job->mtask_map);
        if (next_mtask != NULL) {
            result->file = next_mtask->file;
            result->task = next_mtask->task_id;
            
            result->reduce = 0;
        } else {
            // no more map task left to be scheduled
            // reduce tasks can now be scheduled
            task_info *next_rtask = get_highest_prio_task(next_job->rtask_queue, next_job->rtask_map);
            if (next_rtask != NULL) {
                result->task = next_rtask->task_id;
                result->reduce = 1;
            } else {
                // should never end up here?
                result->wait = 1;
                // next_mtask->status = TASK_DONE;
                // next_rtask->status = TASK_DONE;
                // next_job->status = JOB_DONE;
                // printf("This statement shouldn't be reached if implemented correctly.\n");
            }
        }
    } else {
        // no more job left to be scheduled
        result->wait = 1;
    }
}


int num_task_finished(GList* task_queue, GHashTable* task_map) {
    int counter = 0;
    for (GList* elem = task_queue; elem; elem = elem->next) {
        int task_id = GPOINTER_TO_INT(elem->data); /* Cast data back to an integer. */
        task_info *existing_task = g_hash_table_lookup(task_map, GINT_TO_POINTER(task_id));
        if (existing_task && existing_task->status == TASK_DONE) {
            counter++;
        }
    }
    return counter;
}

void complete_task(GHashTable* job_map, finish_task_request* task_req) {
    job_info *existing_job = g_hash_table_lookup(job_map, GINT_TO_POINTER(task_req->job_id));
    if (existing_job) {
        // update task status
        task_info *existing_task = NULL;
        if (task_req->reduce) {
            existing_task = g_hash_table_lookup(existing_job->rtask_map, GINT_TO_POINTER(task_req->task));
        } else {
            existing_task = g_hash_table_lookup(existing_job->mtask_map, GINT_TO_POINTER(task_req->task));
        }

        if (existing_task) {
            if (task_req->success) {
                existing_task->status = TASK_DONE;
            } else {
                existing_task->status = TASK_FAILED;
                existing_job->status = JOB_FAILED;
                return;
            }
        }
    }

    // update job status
    int mtotal = g_list_length(existing_job->mtask_queue);
    int num_mtask_finished = num_task_finished(existing_job->mtask_queue, existing_job->mtask_map);
    if (num_mtask_finished == mtotal) {
        existing_job->status = JOB_MAP_DONE;
    }

    int rtotal = g_list_length(existing_job->rtask_queue);
    int num_rtask_finished = num_task_finished(existing_job->rtask_queue, existing_job->rtask_map);
    if (num_rtask_finished == rtotal) {
        existing_job->status = JOB_DONE;
    }
}
