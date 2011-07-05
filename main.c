/*
 *  ioscheme - an input event driven scheme interpreter
 *
 *  Copyright (C) 2011  A. Carl Douglas <carl.douglas@gmail.com>
 *
 */
/* vim: softtabstop=2 shiftwidth=2 expandtab  */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

/*#include "scheme.h"*/
#include "scheme-private.h"

#define BUFFER_SIZE 4096

static unsigned verbose = 0;

static const char help[] =
"ioscheme \n"
"  -e FUNCTION scheme entry point\n"
"  -p PORT     port number\n"
"  -u          udp mode, otherwise tcp\n"
"  -v          verbose\n"
"  -h          this help\n";

static void error() {
  syslog(LOG_ERR, "ERROR: %s", strerror(errno));
  exit(errno);
}

int main (int argc, char *argv[]) {

  enum { TCP, UDP } socket_type = TCP;

  int skfd = -1, err = 0;
  socklen_t addrlen = 0;
  struct sockaddr_in addr;
  unsigned short port = 8000;

  scheme *sc = NULL;

  int ch;
  int index;

  int optval = 0;

  unsigned char buffer[BUFFER_SIZE];
  char output[BUFFER_SIZE];

  const char *entry_point = "receive";

  while ((ch = getopt(argc, argv, "vuhp:e:")) != -1) {
    switch(ch) {
      case 'e':
        entry_point = optarg;
        break;
      case 'p':
        port = (unsigned short) atoi(optarg);
        break;
      case 'u':
        socket_type = UDP;
        break;
      case 'h':
        puts(help);
        return(0);
      case 'v':
        verbose = 1;
        break;
      case '?':
        if (optopt == 'p') {
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        } else if (isprint (optopt)) {
          /*fprintf (stderr, "Unknown option `-%c'.\n", optopt);*/
        }
        return(1);
    }
  }

  openlog("iodispatch", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);

  sc = scheme_init_new ();

  scheme_set_input_port_file  (sc, stdin);
  scheme_set_output_port_file (sc, stdout);

  for (index = optind; index < argc; index++) {
    FILE *file = NULL;
    file = fopen(argv[index], "ra");
    if (file == NULL) {
      error();
    }
    scheme_load_named_file (sc, file, argv[index]);
    fclose(file);
  }

  if (socket_type == TCP) {
    skfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  } else {
    skfd = socket(PF_INET, SOCK_DGRAM,  IPPROTO_UDP);
  }

  if (skfd < 0) {
    error();
  }

  optval = 1;
  if (setsockopt(skfd, SOL_SOCKET, SO_REUSEADDR, &optval, (socklen_t)sizeof optval) < 0) {
    error();
  }

  memset(&addr, 0, sizeof addr);
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  addrlen = sizeof(addr);

  if (bind(skfd, (struct sockaddr *)&addr, addrlen) < 0) {
    error();
  }

  if (verbose) {
    printf("bound to %s:%u\n", 
      inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
  }

  if (socket_type == TCP) {
    if (listen(skfd, 10) < 0) {
      error();
    }
  }

  scheme_set_output_port_string(sc, output, output+BUFFER_SIZE);
  sc->gc_verbose = (char)0;

  for (;;) {

    fd_set fds;
    struct sockaddr_in addr2;
    socklen_t addrlen2 = sizeof addr2;

    int skfd2 = -1;

    FD_ZERO(&fds);
    FD_SET(skfd, &fds);
  
    err = select(skfd + 1, &fds, NULL, NULL, NULL);
    if (err < 0) {
      error();
    }
    if (socket_type == TCP && FD_ISSET(skfd, &fds)) {
      err = accept(skfd, (struct sockaddr *)&addr2, &addrlen2);
      if (err < 0) {
        error();
      }
      skfd2 = err;
    }
    if (socket_type == TCP) {
      err = recvfrom(skfd2, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&addr2, &addrlen2);
    } else {
      err = recvfrom(skfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&addr2, &addrlen2);
    }
    if (err < 0) {
      error();
    }
    if (err > 0) {
      int i;
      pointer vector;

      vector = sc->vptr->mk_vector(sc, err);

      for(i = 0; (i < err) && (i < BUFFER_SIZE); i++) {
        sc->vptr->set_vector_elem(vector, i, mk_character(sc, buffer[i]));
      }

      scheme_apply1(sc, entry_point, _cons(sc, vector, sc->NIL, 0));

      if (socket_type == TCP && skfd2 > 0) {
        err = send(skfd2, output, strlen(output), 0);
      } else {
        err = sendto(skfd, output, strlen(output), 0, (struct sockaddr *)&addr2, addrlen2);
      }
      if (err < 0) {
        error();
      }
    }

    if (socket_type == TCP && skfd2 > 0) {
      close(skfd2);
    }
  }
  close(skfd);
  scheme_deinit(sc);
  closelog();

  return err;
}

