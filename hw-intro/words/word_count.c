/*

Copyright © 2019 University of California, Berkeley

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

word_count provides lists of words and associated count

Functional methods take the head of a list as first arg.
Mutators take a reference to a list as first arg.
*/

#include "word_count.h"

/* Basic utilities */

char *new_string(char *str) {
  char *new_str = (char *) malloc(strlen(str) + 1);
  if (new_str == NULL) {
    return NULL;
  }
  return strcpy(new_str, str);
}

int init_words(WordCount **wclist) {
  /* Initialize word count.
     Returns 0 if no errors are encountered
     in the body of this function; 1 otherwise.
  */
   *wclist = NULL;
  return 0;
}

ssize_t len_words(WordCount *wchead) {
  /* Return -1 if any errors are
     encountered in the body of
     this function.
  */
    if (wchead == NULL) {
      return -1;
    }

    size_t len = 0;
    WordCount *temp_pointer = wchead;
    while (temp_pointer->word != NULL) {
      len += 1;
      temp_pointer = temp_pointer->next;
    }
    return len;
}

WordCount *find_word(WordCount *wchead, char *word) {
  /* Return count for word, if it exists */

  WordCount *temp_pointer = wchead;
  while (temp_pointer != NULL && temp_pointer->word != NULL) {
    if (strcmp(temp_pointer->word, word) == 0) {
      return temp_pointer;
    }
    temp_pointer = temp_pointer->next;
  }
  
  return NULL;
}

int add_word(WordCount **wclist, char *word) {
  /* If word is present in word_counts list, increment the count.
     Otherwise insert with count 1.
     Returns 0 if no errors are encountered in the body of this function; 1 otherwise.
  */

  // printf("adding %s to list\n", word);
  WordCount *existing_word_count = find_word(*wclist, word);
  if (existing_word_count != NULL) {
    // printf("existing word exists!!!!!!!!!!!!!!!!!\n");
    existing_word_count->count += 1;
  } else {
    // printf("NOT EXISTS\n");
    // create a new word_count node with count set to 1
    WordCount *new_wc = malloc(sizeof(WordCount));
    if (new_wc == NULL) {
      return 1;
    } 
    // init_words(&new_wc);
    char *malloced_string = new_string(word);
    if (malloced_string == NULL) {
      return 1;
    }
    new_wc->word = malloced_string;
    new_wc->count = 1;

    // append it to front of linked list
    WordCount *temp = *wclist;
    *wclist = new_wc;
    new_wc->next = temp;
  }
  return 0;
}

void fprint_words(WordCount *wchead, FILE *ofile) {
  /* print word counts to a file */
  WordCount *wc;
  for (wc = wchead; wc; wc = wc->next) {
    fprintf(ofile, "%i\t%s\n", wc->count, wc->word);
  }
}
