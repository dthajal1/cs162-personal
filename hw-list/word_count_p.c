/*
 * Implementation of the word_count interface using Pintos lists and pthreads.
 *
 * You may modify this file, and are expected to modify it.
 */

/*
 * Copyright © 2021 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_lp.c"
#endif

#ifndef PTHREADS
#error "PTHREADS must be #define'd when compiling word_count_lp.c"
#endif

#include "word_count.h"


void init_words(word_count_list_t* wclist) { 
  list_init(&(wclist->lst));
  pthread_mutex_init(&(wclist->lock), NULL); // TODO: also need to destroy it?
}

size_t len_words(word_count_list_t* wclist) {
  size_t result = 0;

  struct list_elem *e;
  for (e = list_begin(&(wclist->lst)); e != list_end(&(wclist->lst)); e = list_next(e)) {
    // pthread_mutex_lock(&(wclist->lock));
    result += 1;
    // pthread_mutex_unlock(&(wclist->lock));
  }

  return result;
}

word_count_t* find_word(word_count_list_t* wclist, char* word) {
  struct list_elem *e;
  for (e = list_begin(&(wclist->lst)); e != list_end(&(wclist->lst)); e = list_next(e)) {
    word_count_t *wc = list_entry(e, struct word_count, elem);
    if (strcmp(wc->word, word) == 0) {
      return wc;
    }
  }

  return NULL;
}

word_count_t* add_word(word_count_list_t* wclist, char* word) {
  pthread_mutex_lock(&(wclist->lock));

  word_count_t *existing_wc = find_word(wclist, word);
  if (existing_wc != NULL) {
    // pthread_mutex_lock(&(wclist->lock));
    existing_wc->count += 1;
    // pthread_mutex_unlock(&(wclist->lock));
    return existing_wc;
  }

  word_count_t *new_wc = malloc(sizeof(struct word_count));
  if (new_wc == NULL) {
    perror("malloc failed when allocating word_count struct");
    return NULL;
  }

  // pthread_mutex_lock(&(wclist->lock));

  new_wc->count = 1;
  new_wc->word = word;

  list_push_back(&(wclist->lst), &(new_wc->elem));

  pthread_mutex_unlock(&(wclist->lock));

  return new_wc;
}

void fprint_words(word_count_list_t* wclist, FILE* outfile) { /* TODO */
  struct list_elem *e;
  for (e = list_begin(&(wclist->lst)); e != list_end(&(wclist->lst)); e = list_next(e)) {
    word_count_t *wc = list_entry(e, struct word_count, elem);
    fprintf(outfile, "\t%d\t%s\n", wc->count, wc->word);
  }
}

static bool less_list(const struct list_elem* ewc1, const struct list_elem* ewc2, void* aux) {
  // cast a function pointer
  bool (*comparator_func)(const word_count_t*, const word_count_t*) = aux;
  word_count_t *wc1 = list_entry(ewc1, struct word_count, elem);
  word_count_t *wc2 = list_entry(ewc2, struct word_count, elem);
  return comparator_func(wc1, wc2);
}

void wordcount_sort(word_count_list_t* wclist,
                    bool less(const word_count_t*, const word_count_t*)) {
  list_sort(&(wclist->lst), less_list, less);
}
