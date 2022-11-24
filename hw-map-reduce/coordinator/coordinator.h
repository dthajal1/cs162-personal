/**
 * The MapReduce coordinator.
 */

#ifndef H1_H__
#define H1_H__
#include "../rpc/rpc.h"
#include "../lib/lib.h"
#include "../app/app.h"
#include "job.h"
#include <glib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct {
  int next_job_id;          /* next unique jobID. Starts at 0 and counts up. */   
  GList* job_queue;  /* FIFO job queue. Each element is a jobId */
  GHashTable* job_map;       /* Map for job and its jobID */
} coordinator;

void coordinator_init(coordinator** coord_ptr);
#endif
