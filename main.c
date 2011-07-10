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
static unsigned is_daemon = 0;

static unsigned short iport = 8000;
static scheme *sc = NULL;
static const char *entry_point = "receive";

static const char help[] =
"ioscheme \n"
"  -d          run in the background\n"
"  -e FUNCTION scheme entry point\n"
"  -p PORT     port number\n"
"  -u          udp mode, otherwise http\n"
"  -t          tcp mode, otherwise http\n"
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

  struct evkeyvalq *hdrs;

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

  sc_path = mk_string(sc, path);
  sc_return = scheme_apply1(sc, entry_point, _cons(sc, sc_method, _cons(sc, sc_path, sc->NIL, 0), 0));

  evb = evbuffer_new();
  evbuffer_add_printf(evb, "%s", sc->vptr->string_value(sc_return));
  hdrs = evhttp_request_get_output_headers(req);
  evhttp_add_header(hdrs, "Server", "ioscheme");
  evhttp_add_header(hdrs, "Content-Type", "text/html");
  evhttp_add_header(hdrs, "Connection", "close");
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

#if 0
static void conn_writecb(struct bufferevent *bev, void *user_data)
{

}

static void conn_readcb(struct bufferevent *bev, void *user_data)
{

}

static void conn_eventcb(struct bufferevent *bev, short events, void *user_data){

}

static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *sa, int socklen, void *user_data)
{
  struct event_base *base = user_data;
  struct bufferevent *bev;
  bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb(bev, conn_readcb, conn_writecb, conn_eventcb, NULL);
  bufferevent_enable(bev, EV_WRITE);
  bufferevent_disable(bev, EV_READ);
bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
}
#endif


static void udp_event_cb(evutil_socket_t fd, short events, void *user_data)
{
  if ((events & EV_READ) == EV_READ) {
    char bytes[BUFFER_SIZE];
    int err;
    struct sockaddr_in addr2;
    socklen_t addrlen2 = sizeof addr2;

    pointer sc_return;
    pointer vector;

    err = recvfrom(fd, bytes, BUFFER_SIZE, 0, (struct sockaddr *)&addr2, &addrlen2); 
    if (err > 0) {
      int i;
      vector = sc->vptr->mk_vector(sc, err);
      for(i = 0; (i < err) && (i < BUFFER_SIZE); i++) {
        sc->vptr->set_vector_elem(vector, i, mk_character(sc, bytes[i]));
      }
      sc_return = scheme_apply1(sc, entry_point, _cons(sc, vector, sc->NIL, 0));
    }
  }
}

static void
signal_cb(evutil_socket_t sig, short events, void *user_data)
{
  struct event_base *base = user_data;
  struct timeval delay = { 0, 0 };
  syslog(LOG_ERR, "Caught signal; exiting");
  event_base_loopexit(base, &delay);
}

static void
event_logger(int sev, const char *msg) {
  int p = (sev == _EVENT_LOG_ERR) ? LOG_ERR : LOG_DEBUG;
  syslog(p, msg);
}

int main (int argc, char *argv[]) {

  enum { HTTP, TCP, UDP } server_type = HTTP;

  int ch;
  int index;

  struct event_base *base;
  struct evhttp *http;
  struct evhttp_bound_socket *handle;
  struct event *signal_event;
  struct event *socket_event;

  while ((ch = getopt(argc, argv, "dvuthp:e:")) != -1) {
    switch(ch) {
      case 'd':
        is_daemon = 1;
        break;
      case 'e':
        entry_point = optarg;
        break;
      case 'p':
        iport = (unsigned short) atoi(optarg);
        break;
      case 'h':
        puts(help);
        return(0);
      case 'u':
        server_type = UDP; 
        break;
      case 't':
        server_type = TCP; 
        break;
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

  if (is_daemon) {
    daemon(0,0);
  }

  sc = scheme_init_new ();

  scheme_set_input_port_file  (sc, stdin);
  scheme_set_output_port_file (sc, stdout);

  /* load all the other arguments as files into scheme */
  for (index = optind; index < argc; index++) {
    FILE *file = NULL;
    file = fopen(argv[index], "ra");
    if (file == NULL) {
      error();
    }
    scheme_load_named_file (sc, file, argv[index]);
    fclose(file);
  }

  base = event_base_new();
  if (!base) {
    syslog(LOG_ERR, "Couldn't create an event_base: exiting\n");
    return 1;
  }
  event_set_log_callback(event_logger);

  if (verbose) {
    syslog(LOG_INFO, "libevent %s ", event_get_version());
    syslog(LOG_INFO, "libevent base using %s", event_base_get_method(base));
  }

  signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);
  if (!signal_event || event_add(signal_event, NULL)<0) {
    syslog(LOG_ERR, "Could not create/add a signal event!\n");
    return 1;
  }

  if (server_type == HTTP) {
    http = evhttp_new(base);
    evhttp_set_gencb(http, send_document_cb, NULL);
    handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", iport);
    if (!handle) {
      error();
    }
  } else if (server_type == TCP) {
#if 0
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    listener = evconnlistener_new_bind(base, tcp_listener_cb, (void *)base,
         LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
         (struct sockaddr*)&sin,
         sizeof(sin));
#endif
  } else if (server_type == UDP) {
    evutil_socket_t sock;
    struct sockaddr_in sin;
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port   = htons(iport);
    sin.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&sin, sizeof(sin))<0) {
      error();
    }
    socket_event = event_new(base, sock, EV_READ|EV_PERSIST, udp_event_cb, sc);
    if (socket_event == NULL) {
      error();
    }
    event_add(socket_event, NULL);
    if (verbose) {
      syslog(LOG_INFO, "bound to %s:%u", 
       inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
    }
  }
  event_base_dispatch(base);
  scheme_deinit(sc);
  closelog();
  return 0;
}

/* EOF */
