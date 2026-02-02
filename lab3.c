// lab3.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HISTORY_SIZE 5

int main(void) {
  char *history[5] = {NULL};
  int next = 0, count = 0;

  char *line = NULL;
  size_t cap = 0;

  while (1) {
    printf("Enter input: ");
    fflush(stdout);

    if (getline(&line, &cap, stdin) == -1)
      break;

    char *copy = strdup(line);

    if (history[next] != NULL)
      free(history[next]);
    history[next] = copy;

    next = (next + 1) % 5;
    if (count < 5)
      count++;

    if (strncmp(line, "print", 5) == 0 &&
        (line[5] == '\n' || line[5] == '\0')) {

      int start = (count < 5) ? 0 : next;

      for (int i = 0; i < count; i++) {
        int idx = (start + i) % 5;
        printf("%s", history[idx]);
      }
    }
  }

  for (int i = 0; i < 5; i++)
    free(history[i]);
  free(line);

  return 0;
}
