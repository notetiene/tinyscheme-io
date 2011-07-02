/*
 *  Copyright (C) 2011  A. Carl Douglas <carl.douglas@gmail.com>
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

unsigned verbose = 0;

const char help[] =
"ioscheme \n"
"  -p PORT    port number\n"
"  -u         udp mode\n"
"  -h         this help\n";

void error() {
  syslog(LOG_ERR, "ERROR: %s", strerror(errno));
  exit(errno);
}

int main (int argc, char *argv[], char *arge[]) {

  int skfd = -1, err = 0;
  socklen_t addrlen = 0;
  struct sockaddr_in addr;
  unsigned short port = 8000;

  scheme *sc;

  int ch;
  int index;

  unsigned udp = 0;
  int optval = 0;

  unsigned char buffer[BUFFER_SIZE];
  char output[BUFFER_SIZE];

  openlog("iodispatch", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);

  sc = scheme_init_new ();

  while ((ch = getopt(argc, argv, "vuhp:")) != -1) {
    switch(ch) {
      case 'p':
        port = atoi(optarg);
        break;
      case 'u':
        udp = 1;
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

  scheme_set_input_port_file  (sc, stdin);
  scheme_set_output_port_file (sc, stdout);

  for (index = optind; index < argc; index++) {
    FILE *file;
    file = fopen(argv[index], "ra");
    if (file == NULL) {
      error();
    }
    scheme_load_named_file (sc, file, argv[index]);
    fclose(file);
  }

  if (udp) {
    skfd = socket(PF_INET, SOCK_DGRAM,  IPPROTO_UDP);
  } else {
    skfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  }

  if (skfd < 0) {
    error();
  }

  optval = 1;
  if (setsockopt(skfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval) < 0) {
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

  if (!udp) {
    if (listen(skfd, 10) < 0) {
      error();
    }
  }

  scheme_set_output_port_string(sc, output, output+BUFFER_SIZE);
  sc->gc_verbose = 0;

  for ( ; ; ) {

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
    if (!udp && FD_ISSET(skfd, &fds)) {
      err = accept(skfd, (struct sockaddr *)&addr2, &addrlen2);
      if (err < 0) {
        error();
      }
      skfd2 = err;
    }
    if (skfd2 != -1) {
      err = recvfrom(skfd2, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&addr2, &addrlen2);
    } else {
      err = recvfrom(skfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&addr2, &addrlen2);
    }
    if (err < 0) {
      error();
    }
    buffer[err] = '\0';
    if (err > 0) {
      int i;
      pointer vector;

      vector = sc->vptr->mk_vector(sc, err);

      for(i = 0; (i < err) && (i < BUFFER_SIZE); i++) {
        sc->vptr->set_vector_elem(vector, i, mk_character(sc, buffer[i]));
      }

      (void)scheme_apply1(sc, "receive", _cons(sc, vector, sc->NIL, 0));

      /*sc->vptr->gc(sc, sc->NIL, sc->NIL);*/

      if (skfd2 > 0) {
        err = send(skfd2, output, strlen(output), 0);
      } else {
        err = sendto(skfd, output, strlen(output), 0, (struct sockaddr *)&addr2, addrlen2);
      }
      if (err < 0) {
        error();
      }
    }

    if (!udp && skfd2 > 0) {
      close(skfd2);
    }
  }
  close(skfd);
  scheme_deinit(sc);
  closelog();

  return err;
}

