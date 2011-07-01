
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

void error() {
  syslog(LOG_ERR, "%s", strerror(errno));
  exit(errno);
}

int main (int argc, char *argv[], char *arge[]) {

  int skfd = -1, err = 0;
  socklen_t addrlen = 0;
  struct sockaddr_in addr;
  unsigned short port = 8000;

  FILE *file;
  scheme *sc;

  int ch;

  while ((ch = getopt(argc, argv, "p:")) != -1) {
    switch(ch) {
      case 'p':
        port = atoi(optarg);
        printf("using port %d\n", port);
        break;
    }
  }


  openlog("iodispatch", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);

  sc = scheme_init_new ();

  /*scheme_set_input_port_file  (sc, stdin);*/
  scheme_set_output_port_file (sc, stdout);

  file = fopen("init.scm", "ra");
  if (file == NULL) {
    error();
  }
  scheme_load_named_file (sc, file, "init.scm");
  fclose(file);

  file = fopen("fn.scm", "ra");
  if (file == NULL) {
    error();
  }
  scheme_load_named_file (sc, file, "fn.scm");
  fclose(file);

  skfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (skfd < 0) {
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

  for ( ; ; ) {
    fd_set fds;
    struct sockaddr_in addr2;
    socklen_t addrlen2;
    unsigned char buffer[BUFFER_SIZE];
    FD_ZERO(&fds);
    FD_SET(skfd, &fds);
    err = select(skfd + 1, &fds, NULL, NULL, NULL);
    if (err < 0) {
      error();
    }
    err = recvfrom(skfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&addr2, &addrlen2);
    if (err < 0) {
      error();
    }
    buffer[err] = '\0';
    if (err > 0) {
      pointer vector;
      int i;
      vector = sc->vptr->mk_vector(sc, err);
      for(i = 0; i < err; i++) {
        sc->vptr->set_vector_elem(vector, i, mk_character(sc, buffer[i]));
      }
      sc->vptr->scheme_define(sc, sc->global_env, 
          mk_symbol (sc, "data"),
          vector
          );

      (void)scheme_apply0(sc, "main");
    }
  }
  close(skfd);

  scheme_deinit(sc);

  closelog();

  return err;
}

