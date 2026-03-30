/*
questions to answer at top of server.c:

understanding the client:
1. how is the client sending data to the server? what protocol?
   the client uses tcp (SOCK_STREAM), connects to localhost and sends with
write().

2. what data is the client sending to the server?
   it sends 5 strings: Hello, Apple, Car, Green, Dog (each into a 1024 byte
buffer).

understanding the server:
1. explain the argument that the `run_acceptor` thread is passed as an argument.
   run_acceptor gets a pointer to struct with run flag, shared list, and mutex
for syncing.

2. how are received messages stored?
   each recieved message is stored in linked list nodes (malloc) and added to
shared list.

3. what does `main()` do with the received messages?
   main waits for enough messages, stops threads, prints and frees all data.

4. how are threads used in this sample code?
   one thread accepts clients, then one thread per client handles messages.

explain the use of non-blocking sockets in this lab.
how are sockets made non-blocking?
   using fcntl() and setting O_NONBLOCK flag.

what sockets are made non-blocking?
   both server socket and client sockets.

why are these sockets made non-blocking? what purpose does it serve?
   It is so the threads don’t block forever and can exit properly when stopping.
*/

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define PORT 8001
#define LISTEN_BACKLOG 32
#define MAX_CLIENTS 4
#define NUM_MSG_PER_CLIENT 5

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

struct list_node {
  struct list_node *next;
  void *data;
};

struct list_handle {
  struct list_node *last;
  volatile uint32_t count;
};

struct client_args {
  atomic_bool run;
  int cfd;
  struct list_handle *list_handle;
  pthread_mutex_t *list_lock;
};

struct acceptor_args {
  atomic_bool run;
  struct list_handle *list_handle;
  pthread_mutex_t *list_lock;
};

int init_server_socket() {
  struct sockaddr_in addr;
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
    handle_error("bind");
  }

  if (listen(sfd, LISTEN_BACKLOG) == -1) {
    handle_error("listen");
  }

  return sfd;
}

// makes fd nonblocking
void set_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL");
    exit(EXIT_FAILURE);
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl F_SETFL");
    exit(EXIT_FAILURE);
  }
}

void add_to_list(struct list_handle *list_handle, struct list_node *new_node) {
  struct list_node *last_node = list_handle->last;
  last_node->next = new_node;
  list_handle->last = last_node->next;
  list_handle->count++;
}

int collect_all(struct list_node head) {
  struct list_node *node = head.next;
  uint32_t total = 0;

  while (node != NULL) {
    printf("Collected: %s\n", (char *)node->data);
    total++;

    struct list_node *next = node->next;
    free(node->data);
    free(node);
    node = next;
  }

  return total;
}

static void *run_client(void *args) {
  struct client_args *cargs = (struct client_args *)args;
  int cfd = cargs->cfd;
  set_non_blocking(cfd);

  char msg_buf[BUF_SIZE];

  while (cargs->run) {
    ssize_t bytes_read = read(cfd, &msg_buf, BUF_SIZE);

    if (bytes_read == -1) {
      if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
        perror("Problem reading from socket!\n");
        break;
      }

      // dont spin too hard
      sleep(1000);
    } else if (bytes_read > 0) {
      struct list_node *new_node = malloc(sizeof(struct list_node));
      if (new_node == NULL) {
        perror("malloc new_node");
        break;
      }

      new_node->next = NULL;
      new_node->data = malloc(BUF_SIZE);
      if (new_node->data == NULL) {
        free(new_node);
        perror("malloc node data");
        break;
      }

      memcpy(new_node->data, msg_buf, BUF_SIZE);

      struct list_handle *list_handle = cargs->list_handle;

      // lock list before touching shared linked list
      pthread_mutex_lock(cargs->list_lock);
      add_to_list(list_handle, new_node);
      pthread_mutex_unlock(cargs->list_lock);
    } else {
      // client closed connection
      break;
    }
  }

  return NULL;
}

static void *run_acceptor(void *args) {
  int sfd = init_server_socket();
  set_non_blocking(sfd);

  struct acceptor_args *aargs = (struct acceptor_args *)args;
  pthread_t threads[MAX_CLIENTS];
  struct client_args client_args[MAX_CLIENTS];

  printf("Accepting clients...\n");

  uint16_t num_clients = 0;
  while (aargs->run) {
    if (num_clients < MAX_CLIENTS) {
      int cfd = accept(sfd, NULL, NULL);
      if (cfd == -1) {
        if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
          handle_error("accept");
        }

        sleep(1000);
      } else {
        printf("Client connected!\n");

        client_args[num_clients].cfd = cfd;
        client_args[num_clients].run = true;
        client_args[num_clients].list_handle = aargs->list_handle;
        client_args[num_clients].list_lock = aargs->list_lock;

        if (pthread_create(&threads[num_clients], NULL, run_client,
                           &client_args[num_clients]) != 0) {
          perror("pthread_create");
          close(cfd);
        } else {
          num_clients++;
        }
      }
    } else {
      // got enough clients already
      sleep(1000);
    }
  }

  printf("Not accepting any more clients!\n");

  for (int i = 0; i < num_clients; i++) {
    client_args[i].run = false;

    pthread_join(threads[i], NULL);

    if (close(client_args[i].cfd) == -1) {
      perror("client socket close");
    }
  }

  if (close(sfd) == -1) {
    perror("closing server socket");
  }

  return NULL;
}

int main() {
  pthread_mutex_t list_mutex;
  pthread_mutex_init(&list_mutex, NULL);

  // list head stays on stack
  struct list_node head = {NULL, NULL};

  struct list_handle list_handle = {
      .last = &head,
      .count = 0,
  };

  pthread_t acceptor_thread;
  struct acceptor_args aargs = {
      .run = true,
      .list_handle = &list_handle,
      .list_lock = &list_mutex,
  };

  pthread_create(&acceptor_thread, NULL, run_acceptor, &aargs);

  // wait until all expected messages are in
  while (1) {
    uint32_t current_count;

    pthread_mutex_lock(&list_mutex);
    current_count = list_handle.count;
    pthread_mutex_unlock(&list_mutex);

    if (current_count >= MAX_CLIENTS * NUM_MSG_PER_CLIENT) {
      break;
    }

    sleep(1000);
  }

  aargs.run = false;
  pthread_join(acceptor_thread, NULL);

  if (list_handle.count != MAX_CLIENTS * NUM_MSG_PER_CLIENT) {
    printf("Not enough messages were received!\n");
    pthread_mutex_destroy(&list_mutex);
    return 1;
  }

  int collected = collect_all(head);
  printf("Collected: %d\n", collected);

  if (collected != list_handle.count) {
    printf("Not all messages were collected!\n");
    pthread_mutex_destroy(&list_mutex);
    return 1;
  } else {
    printf("All messages were collected!\n");
  }

  pthread_mutex_destroy(&list_mutex);
  return 0;
}
//------------------------
// CLIENT.C
//------------------------

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8001
#define BUF_SIZE 1024
#define ADDR "127.0.0.1"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define NUM_MSG 5

static const char *messages[NUM_MSG] = {"Hello", "Apple", "Car", "Green",
                                        "Dog"};

int main() {
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);

  if (inet_pton(AF_INET, ADDR, &addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  if (connect(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    handle_error("connect");
  }

  char buf[BUF_SIZE];

  for (int i = 0; i < NUM_MSG; i++) {
    sleep(1);

    // put msg into full buffer
    strncpy(buf, messages[i], BUF_SIZE);

    if (write(sfd, buf, BUF_SIZE) == -1) {
      handle_error("write");
    } else {
      printf("Sent: %s\n", messages[i]);
    }
  }

  close(sfd);
  return 0;
}
