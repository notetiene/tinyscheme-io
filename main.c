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

extern void init_scheme_sqlite3(scheme *);

#define BUFFER_SIZE 4096

static unsigned verbose = 0;
static unsigned is_daemon = 0;
static unsigned want_chdir = 0;

static unsigned short iport = 8000;
static scheme *sc = NULL;
static const char *entry_point = "receive";

static char *docroot = NULL;

static const char help[] =
"ioscheme \n"
"  -c            change to document root directory\n"
"  -d            run in the background\n"
"  -e FUNCTION   scheme entry point\n"
"  -h            this help\n"
"  -p PORT       port number\n"
"  -r DIRECTORY  the document root directory for serving regular files from\n"
"  -t            tcp mode, otherwise http\n"
"  -u            udp mode, otherwise http\n"
"  -v            verbose\n";

static void error() {
  syslog(LOG_ERR, "ERROR: (%d) %s", errno, strerror(errno));
  exit(1);
}

static const struct table_entry {
        const char *extension;
        const char *content_type;
} content_type_table[] = {
        { "txt", "text/plain" },
        { "c", "text/plain" },
        { "h", "text/plain" },
        { "html", "text/html" },
        { "htm", "text/htm" },
        { "css", "text/css" },
        { "gif", "image/gif" },
        { "jpg", "image/jpeg" },
        { "jpeg", "image/jpeg" },
        { "png", "image/png" },
        { "pdf", "application/pdf" },
        { "ps", "application/postsript" },
        { NULL, NULL },
};

/* got this from libevent samples, http-server */
/* Try to guess a good content-type for 'path' */
static const char *
guess_content_type(const char *path)
{
        const char *last_period, *extension;
        const struct table_entry *ent;
        last_period = strrchr(path, '.');
        if (!last_period || strchr(last_period, '/'))
                goto not_found; /* no exension */
        extension = last_period + 1;
        for (ent = &content_type_table[0]; ent->extension; ++ent) {
                if (!evutil_ascii_strcasecmp(ent->extension, extension))
                        return ent->content_type;
        }

not_found:
        return "application/misc";
}


/* got this from libevent, http.c, better if it was exposed from there? */
/** Given an evhttp_cmd_type, returns a constant string containing the
 * equivalent HTTP command, or NULL if the evhttp_command_type is
 * unrecognized. */
static const char *
evhttp_method(enum evhttp_cmd_type type)
{
        const char *method;

        switch (type) {
        case EVHTTP_REQ_GET:
                method = "GET";
                break;
        case EVHTTP_REQ_POST:
                method = "POST";
                break;
        case EVHTTP_REQ_HEAD:
                method = "HEAD";
                break;
        case EVHTTP_REQ_PUT:
                method = "PUT";
                break;
        case EVHTTP_REQ_DELETE:
                method = "DELETE";
                break;
        case EVHTTP_REQ_OPTIONS:
                method = "OPTIONS";
                break;
        case EVHTTP_REQ_TRACE:
                method = "TRACE";
                break;
        case EVHTTP_REQ_CONNECT:
                method = "CONNECT";
                break;
        case EVHTTP_REQ_PATCH:
                method = "PATCH";
                break;
        default:
                method = NULL;
                break;
        }

        return (method);
}

pointer query_fi_cons(const char *qs) {

  pointer scl = sc->NIL;
  struct evkeyvalq params;
  struct evkeyval  *param;

  TAILQ_INIT(&params);
  evhttp_parse_query_str(qs, &params);

  if (qs != NULL && (strlen(qs) > 0)) {
    TAILQ_FOREACH(param, &params, next) {
      pointer pp = sc->NIL, k, v;
      if (param->key && param->value) {
        k  = mk_symbol(sc, param->key);
        v  = mk_symbol(sc, param->value);
        pp = _cons(sc, k, v, 0);
        scl = _cons(sc, pp, scl, 0);
      }
    }
  }
  scl = _cons(sc, scl, sc->NIL, 0);
  scl = _cons(sc, mk_symbol(sc, "quote"), scl, 0);

  return scl;
}

static void
send_document_cb(struct evhttp_request *req, void *arg)
{
  struct evbuffer *evb = NULL;
  const char *uri = evhttp_request_get_uri(req);
  struct evhttp_uri *decoded = NULL;
  const char *method = NULL;
  const char *path   = NULL;
  const char *query  = NULL;
  char *decoded_path = NULL;
  char *data         = NULL;
  char local_path[255];
  int len = 255;
  int status_code = 0;
  char *phrase = NULL;
  int bytes_out = 0;

  struct evkeyvalq *hdrs;

  pointer sc_method = sc->NIL;
  pointer sc_path   = sc->NIL;
  pointer sc_params = sc->NIL;
  pointer sc_data   = sc->NIL;
  pointer sc_return = sc->NIL;
  pointer sc_args   = sc->NIL;

  pointer tmp = sc->NIL;

  struct stat st;

  method  = evhttp_method(evhttp_request_get_command(req));
  decoded = evhttp_uri_parse(uri);

  path    = evhttp_uri_get_path(decoded);
  query   = evhttp_uri_get_query(decoded);

  /* uri unescape */
  decoded_path = evhttp_uridecode(path, 0, NULL);
  evutil_snprintf(local_path, len, "%s/%s", docroot, decoded_path);

  evb  = evbuffer_new();
  hdrs = evhttp_request_get_output_headers(req);
  evhttp_add_header(hdrs, "Server", "ioscheme");
  evhttp_add_header(hdrs, "Connection", "close");

  /* just return regular files */
  if ((stat(local_path, &st)==0) && (S_ISREG(st.st_mode))) {
    int fd;
    const char *type = guess_content_type(decoded_path);
    if ((fd = open(local_path, O_RDONLY)) < 0) {
      error();
    }
    if (fstat(fd, &st) < 0) {
      error();
    }
    status_code = 200;
    phrase = "OK";

    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", type);
    evbuffer_add_file(evb, fd, 0, st.st_size);
    goto done;
  }

  sc_params = query_fi_cons(query);

  sc_path   = mk_string(sc, path);
  sc_method = mk_string(sc, method);

  data      = evbuffer_pullup(evhttp_request_get_input_buffer(req), -1);
  if (data && (strlen(data) > 0)) {
    sc_data   = mk_string(sc, data);
  } else {
    sc_data   = mk_string(sc, "");
  }
  if (evhttp_request_get_command(req) == EVHTTP_REQ_POST) {
    if (data && (strlen(data) > 0)) {
      sc_data = query_fi_cons(data);
    }
  }
  sc_args   = _cons(sc, sc_params, sc_args, 0);
  sc_args   = _cons(sc, sc_data,   sc_args, 0);
  sc_args   = _cons(sc, sc_path,   sc_args, 0);
  sc_args   = _cons(sc, sc_method, sc_args, 0);

  sc_return = scheme_apply1(sc, entry_point, sc_args);

  /* http status code and phrase */ 
  tmp = sc->vptr->pair_car(sc_return);

  status_code = sc->vptr->ivalue(sc->vptr->pair_car(tmp));
  phrase      = sc->vptr->string_value(sc->vptr->pair_cdr(tmp));
  tmp         = sc->vptr->pair_cdr(sc_return);
  for(; tmp != sc->NIL; tmp = sc->vptr->pair_cdr(tmp)) {
    pointer p;
    p = sc->vptr->pair_car(tmp);
    if (sc->vptr->is_pair(p)) {
      /* http headers */
      const char *n, *v;
      n = sc->vptr->string_value(sc->vptr->pair_car(p));
      v = sc->vptr->string_value(sc->vptr->pair_cdr(p));
      evhttp_add_header(hdrs, n, v);
    } else if (sc->vptr->is_string(p)) {
      const char *c;
      c = sc->vptr->string_value(p);
      evbuffer_add_printf(evb, "%s", c);
    } else {
      syslog(LOG_ERR, "unexpected return type from scheme '%s' function", entry_point);
    }
  }
done:
  if (status_code > 0) {
    bytes_out = evbuffer_get_length(evb);
    evhttp_send_reply(req, status_code, phrase, evb);
  }
  if (verbose) {
    /* use common log format */
    /* host ident authuser date request status bytes */
    char timeformat[255];
    const char * host = evhttp_request_get_host(req);
    const char * ident = "-";
    const char * authuser = evhttp_find_header(evhttp_request_get_input_headers(req), "user-agent") ?: "-";
    const char * req  = method;
    struct tm td;
    time_t clk;
    time(&clk);
    localtime_r(&clk, &td);

    sprintf(timeformat, "%02d/%02d/%04d:%02d:%02d:%02d %02ld%02ld",
      td.tm_mday, td.tm_mon+1, td.tm_year+1900, 
      td.tm_hour, td.tm_min, td.tm_sec, 
      (td.tm_gmtoff/3600), (td.tm_gmtoff/60)%60);

    syslog(LOG_INFO, 
        "%s %s %s [%s] \"%s %s\" %d %d", 
        host, ident, authuser, timeformat, req, path, status_code, bytes_out);
  }
  if (decoded)      evhttp_uri_free(decoded);
  if (decoded_path) free(decoded_path);
  if (evb)          evbuffer_free(evb);
}

static void conn_readcb(struct bufferevent *bev, void *user_data)
{
  struct evbuffer *src, *dst;
  size_t len;
  int i;
  char *bytes;

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
  struct evbuffer *evb = NULL;

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

  while ((ch = getopt(argc, argv, "cde:hp:r:tuv")) != -1) {
    switch(ch) {
      case 'c':
        want_chdir = 1;
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
        if (optopt == 'e' || optopt == 'p' || optopt == 'r') {
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        } else if (isprint (optopt)) {
          /*fprintf (stderr, "Unknown option `-%c'.\n", optopt);*/
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
    if (chdir(docroot) < 0) {
      error();
    }
  }

  sc = scheme_init_new ();

  scheme_set_input_port_file  (sc, stdin);
  scheme_set_output_port_file (sc, stdout);

  /* register sqlite3 ffi with scheme environment */
  init_scheme_sqlite3(sc);

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
