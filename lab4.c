// lab4.c
#define _DEFAULT_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 256

struct header {
  uint64_t size;
  struct header *next;
};

static void handle_error(const char *msg) {
  (void)msg;
  _exit(1);
}

void print_out(char *format, void *data, size_t data_size) {
  char buf[BUF_SIZE];
  ssize_t len = snprintf(buf, BUF_SIZE, format,
                         data_size == sizeof(uint64_t) ? *(uint64_t *)data
                                                       : *(void **)data);

  if (len < 0)
    handle_error("snprintf");
  if (write(STDOUT_FILENO, buf, (size_t)len) < 0)
    handle_error("write");
}

int main(void) {
  void *new_heap = sbrk(256);
  if (new_heap == (void *)-1) {
    handle_error("sbrk");
  }

  struct header *block1 = (struct header *)new_heap;

  struct header *block2 = (struct header *)((char *)new_heap + 128);

  block1->size = 128;
  block1->next = NULL;

  block2->size = 128;
  block2->next = block1;

  size_t header_bytes = sizeof(struct header);
  size_t data_bytes = 128 - header_bytes;

  uint8_t *block1_data = (uint8_t *)block1 + header_bytes;
  uint8_t *block2_data = (uint8_t *)block2 + header_bytes;

  memset(block1_data, 0, data_bytes);
  memset(block2_data, 1, data_bytes);

  print_out("first block:       %p\n", &block1, sizeof(block1));
  print_out("second block:      %p\n", &block2, sizeof(block2));

  print_out("first block size:  %lu\n", &block1->size, sizeof(block1->size));
  print_out("first block next:  %p\n", &block1->next, sizeof(block1->next));
  print_out("second block size: %lu\n", &block2->size, sizeof(block2->size));
  print_out("second block next: %p\n", &block2->next, sizeof(block2->next));

  for (size_t i = 0; i < data_bytes; i++) {
    uint64_t v = block1_data[i];
    print_out("%lu\n", &v, sizeof(v));
  }
  for (size_t i = 0; i < data_bytes; i++) {
    uint64_t v = block2_data[i];
    print_out("%lu\n", &v, sizeof(v));
  }

  return 0;
}
