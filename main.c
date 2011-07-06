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

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>


/*#include "scheme.h"*/
#include "scheme-private.h"

#define BUFFER_SIZE 4096

static unsigned verbose = 0;

static unsigned short iport = 8000;
static scheme *sc = NULL;
static const char *entry_point = "receive";

static const char help[] =
"ioscheme \n"
"  -e FUNCTION scheme entry point\n"
"  -p PORT     port number\n"
/*"  -u          udp mode, otherwise tcp\n"*/
"  -v          verbose\n"
"  -h          this help\n";

static void error() {
  syslog(LOG_ERR, "ERROR: %s", strerror(errno));
  exit(errno);
}

static void 
send_document_cb(struct evhttp_request *req, void *arg)
{
  struct evbuffer *evb = NULL;
  const char *uri = evhttp_request_get_uri(req);
  struct evhttp_uri *decoded = NULL;
  const char *path;
  char *decoded_path;
  char *whole_path = NULL;

  pointer sc_method;
  pointer sc_path;
  pointer sc_return;

  switch(evhttp_request_get_command(req)) {
    case EVHTTP_REQ_GET:
      sc_method = mk_string(sc, "GET"); break;
    case EVHTTP_REQ_POST:
      sc_method = mk_string(sc, "POST"); break;
  }

  decoded = evhttp_uri_parse(uri);
  path = evhttp_uri_get_path(decoded);

  sc_path = mk_string(sc, path);

  sc_return = scheme_apply1(sc, entry_point, _cons(sc, sc_method, _cons(sc, sc_path, sc->NIL, 0), 0));

  evb = evbuffer_new();
  evbuffer_add_printf(evb, "%s", sc->vptr->string_value(sc_return));
  evhttp_add_header(evhttp_request_get_output_headers(req),
                          "Content-Type", "text/html");
  evhttp_send_reply(req, 200, "OK", evb);
  if (decoded)
    evhttp_uri_free(decoded);
  if (decoded_path)
    free(decoded_path);
  if (whole_path)
    free(whole_path);
  if (evb)
    evbuffer_free(evb);
}

int main (int argc, char *argv[]) {

  int ch;
  int index;

  struct event_base *base;
  struct evhttp *http;
  struct evhttp_bound_socket *handle;

  while ((ch = getopt(argc, argv, "vuhp:e:")) != -1) {
    switch(ch) {
      case 'e':
        entry_point = optarg;
        break;
      case 'p':
        iport = (unsigned short) atoi(optarg);
        break;
      case 'h':
        puts(help);
        return(0);
      case 'v':
        verbose = 1;
        break;
      case '?':
        if (optopt == 'p' || optopt == 'e') {
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        } else if (isprint (optopt)) {
          /*fprintf (stderr, "Unknown option `-%c'.\n", optopt);*/
        }
        return(1);
    }
  }

  openlog("ioscheme", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);

  base = event_base_new();
  if (!base) {
    fprintf(stderr, "Couldn't create an event_base: exiting\n");
    return 1;
  }
  http = evhttp_new(base);
  evhttp_set_gencb(http, send_document_cb, argv[1]);

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

  handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", iport);
  if (!handle) {
    error();
  }

  event_base_dispatch(base);

  scheme_deinit(sc);
  closelog();

  return 0;
}


/* EOF */

