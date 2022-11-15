/**
 * Server binary.
 */

#include "kv_store.h"
#include <glib.h>
#include <memory.h>
#include <netinet/in.h>
#include <rpc/pmap_clnt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#ifndef SIG_PF
#define SIG_PF void (*)(int)
#endif

/* Global state. Database to store (key, value) pair. */
GHashTable *kv_store;

extern void kvstore_1(struct svc_req *, struct SVCXPRT *);

/* Set up and run RPC server. */
int main(int argc, char **argv) {
  register SVCXPRT *transp;

  pmap_unset(KVSTORE, KVSTORE_V1);

  transp = svcudp_create(RPC_ANYSOCK);
  if (transp == NULL) {
    fprintf(stderr, "%s", "cannot create udp service.");
    exit(1);
  }
  if (!svc_register(transp, KVSTORE, KVSTORE_V1, kvstore_1, IPPROTO_UDP)) {
    fprintf(stderr, "%s", "unable to register (KVSTORE, KVSTORE_V1, udp).");
    exit(1);
  }

  transp = svctcp_create(RPC_ANYSOCK, 0, 0);
  if (transp == NULL) {
    fprintf(stderr, "%s", "cannot create tcp service.");
    exit(1);
  }
  if (!svc_register(transp, KVSTORE, KVSTORE_V1, kvstore_1, IPPROTO_TCP)) {
    fprintf(stderr, "%s", "unable to register (KVSTORE, KVSTORE_V1, tcp).");
    exit(1);
  }

  /* Initialize state (key, value) database. */
  kv_store = g_hash_table_new(g_bytes_hash, g_bytes_equal);

  svc_run();
  fprintf(stderr, "%s", "svc_run returned");
  exit(1);
  /* NOTREACHED */
}

/* Example server-side RPC stub. */
int *example_1_svc(int *argp, struct svc_req *rqstp) {
  static int result;

  result = *argp + 1;

  return &result;
}

/* TODO: Add additional RPC stubs. */

/* Echo server-side RPC stub. */
char **echo_1_svc(char **argp, struct svc_req *rqstp) {
  static char *result;

  result = *argp;

  return &result;
}

/* PUT server-side RPC stub. */
void *put_1_svc(k_v_pair *argp, struct svc_req *rqstp) {
  static void *result;

  // add to global key value pair storage
  buf k = argp->k;
  buf v = argp->v;
  GBytes *key = g_bytes_new(k.buf_val, k.buf_len); 
  GBytes *value = g_bytes_new(v.buf_val, v.buf_len);
  g_hash_table_insert(kv_store, key, value);

  return (void *) &result;
}

/* GET server-side RPC stub. */
buf *get_1_svc(buf *argp, struct svc_req *rqstp) {
  static buf result;

  // lookup in global kv_store using key *BUF
  GBytes *key = g_bytes_new(argp->buf_val, argp->buf_len);
  GBytes *value = g_hash_table_lookup(kv_store, key);

  g_bytes_unref(key);

  if (value != NULL) {
    long unsigned int len;
    const char *data = g_bytes_get_data(value, &len); /* Sets len = strlen(value). */
    result.buf_len = len;
    result.buf_val = data;
  } else {
    result.buf_len = 0;
  }

  return &result;
}

