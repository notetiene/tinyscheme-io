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
#include <sys/queue.h> /* event keyvalq */
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_compat.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

/*#include "scheme.h"*/
#include "scheme-private.h"

#define BUFFER_SIZE 4096

extern void base64_decode(char *src, char *dst);
extern void init_scheme_sqlite3(scheme *);
void send_document_cb(struct evhttp_request *req, void *arg);

scheme *sc = NULL;
const char *entry_point = "receive";
char *docroot = NULL;
unsigned verbose = 0;

static unsigned is_daemon = 0;
static unsigned want_chdir = 0;
static unsigned short iport = 8000;
static char *workdir = NULL;

static const char help[] =
"ioscheme \n"
"  -c DIRECTORY  change to this directory\n"
"  -d            run in the background\n"
"  -e FUNCTION   scheme entry point\n"
"  -h            this help\n"
"  -p PORT       port number\n"
"  -r DIRECTORY  the document root directory for serving regular files from\n"
"  -t            tcp mode, otherwise http\n"
"  -u            udp mode, otherwise http\n"
"  -v            verbose\n";

void error() {
  syslog(LOG_ERR, "ERROR: (%d) %s", errno, strerror(errno));
  exit(1);
}

pointer scheme_base64_decode(scheme *sp, pointer args) {
  char *str1, *str2;
  pointer arg1, retval;
  arg1 = sp->vptr->pair_car(args);
  if (!sp->vptr->is_string(arg1))
    return sp->F;
  str1 = sp->vptr->string_value(arg1);
  str2 = malloc( (strlen(str1) * 3 / 4) + 1 );
  base64_decode(str1, str2);
  retval = sp->vptr->mk_string(sp, str2);
  free(str2);
  return retval;
}

static void conn_readcb(struct bufferevent *bev, void *user_data)
{
  struct evbuffer *src, *dst;
  size_t len;
  int i;
  unsigned char *bytes;

  pointer sc_return;
  pointer vector;

  src = bufferevent_get_input(bev);
  evbuffer_lock(src);
  len = evbuffer_get_length(src);

  bytes = evbuffer_pullup(src, -1);

  vector = sc->vptr->mk_vector(sc, len);
  for(i = 0; (i < len) && (i < BUFFER_SIZE); i++) {
    sc->vptr->set_vector_elem(vector, i, mk_character(sc, bytes[i]));
  }
  evbuffer_unlock(src);
  sc_return = scheme_apply1(sc, entry_point, _cons(sc, vector, sc->NIL, 0));

  if (sc_return != sc->NIL) {
    dst = evbuffer_new();
    evbuffer_add_printf(dst, "%s", sc->vptr->string_value(sc_return));
    len = bufferevent_write_buffer(bev, dst);
    if (len < 0) {
      syslog(LOG_ERR, "%s:%d error", __FILE__ , __LINE__);
    }
  }
}

static void conn_eventcb(struct bufferevent *bev, short events, void *user_data){
  if (events & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
    if (events & (BEV_EVENT_ERROR)) {
      syslog(LOG_INFO, "%s:%d error", __FILE__, __LINE__);
    }
    bufferevent_free(bev);
  }
}

static void accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *sa, int socklen, void *user_data)
{
  struct event_base *base = user_data;
  struct bufferevent *bev_in;

  bev_in = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  bufferevent_setcb(bev_in, conn_readcb, NULL, conn_eventcb, NULL);
  bufferevent_enable(bev_in, EV_READ|EV_WRITE);
}

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
  syslog(p, "%s", msg);
}

int main (int argc, char *argv[]) {

  enum { HTTP, TCP, UDP } server_type = HTTP;

  int ch;
  int index;
  int opt;

  struct event_base *base;
  struct evhttp *http;
  struct evhttp_bound_socket *handle;
  struct event *signal_event;
  struct event *socket_event;
  struct evconnlistener * listener;

  docroot = getenv("PWD");

  while ((ch = getopt(argc, argv, "c:de:hp:r:tuv")) != -1) {
    switch(ch) {
      case 'c':
        want_chdir = 1;
        workdir = optarg;
        break;
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
        return(1);
      case 'u':
        server_type = UDP;
        break;
      case 't':
        server_type = TCP;
        break;
      case 'v':
        verbose = 1;
        break;
      case 'r':
        docroot = optarg;
        break;
      case '?':
        if (optopt == 'c' || optopt == 'e' || optopt == 'p' || optopt == 'r') {
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        }
        return(1);
    }
  }

  opt = LOG_PID | LOG_NDELAY | LOG_PERROR;
  openlog("ioscheme", opt, LOG_USER);

  if (is_daemon) {
    daemon(0,0);
  }

  if (want_chdir) {
    if (chdir(workdir) < 0) {
      error();
    }
  }

  sc = scheme_init_new ();

  scheme_set_input_port_file  (sc, stdin);
  scheme_set_output_port_file (sc, stdout);

  /* register sqlite3 ffi with scheme environment */
  init_scheme_sqlite3(sc);

  /* register base64 decode function */
  sc->vptr->scheme_define(sc,sc->global_env,
    sc->vptr->mk_symbol(sc,"base64-decode"),
    sc->vptr->mk_foreign_func(sc, scheme_base64_decode));


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
    syslog(LOG_INFO, "document root: %s", docroot);
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
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(iport);
    listener = evconnlistener_new_bind(base, accept_cb, (void *)base,
         LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
         (struct sockaddr*)&sin,
         sizeof(sin));
    if (listener == NULL) {
      error();
    }
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
