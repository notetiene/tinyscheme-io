/*
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h> /* event keyvalq */
#include <sys/stat.h>
#include <syslog.h>
#include <fcntl.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_compat.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include "scheme-private.h"

extern scheme *sc;
extern const char *entry_point;
extern char *docroot;
extern unsigned verbose;

void error();
pointer scheme_base64_decode(scheme *sp, pointer args);

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

static pointer query_fi_cons(const char *qs) {

  pointer scl = sc->NIL;
  struct evkeyvalq params;
  struct evkeyval  *param;

  TAILQ_INIT(&params);
  evhttp_parse_query_str(qs, &params);

  if (qs != NULL && (strlen(qs) > 0)) {
    TAILQ_FOREACH(param, &params, next) {
      pointer pp = sc->NIL, k, v;
      if (param->key && param->value) {
        k  = mk_string(sc, param->key);
        v  = mk_string(sc, param->value);
        pp = _cons(sc, v, sc->NIL, 0);
        pp = _cons(sc, k, pp, 0);
        scl = _cons(sc, pp, scl, 0);
      }
    }
  }
  scl = _cons(sc, scl, sc->NIL, 0);
  scl = _cons(sc, mk_symbol(sc, "quote"), scl, 0);

  return scl;
}

static pointer headers_fi_cons(struct evkeyvalq * hdrs) {
  pointer scl = sc->NIL;
  struct evkeyval  *param;
  TAILQ_FOREACH(param, hdrs, next) {
    pointer pp = sc->NIL;
    pointer k, v;
    if (param->key && param->value) {
      /*printf("%s=%s\n",param->key,param->value);*/
      k  = mk_string(sc, param->key);
      v  = mk_string(sc, param->value);
      pp = _cons(sc, v, sc->NIL, 0);
      pp = _cons(sc, k, pp, 0);
      scl = _cons(sc, pp, scl, 0);
    }
  }
  scl = _cons(sc, scl, sc->NIL, 0);
  scl = _cons(sc, mk_symbol(sc, "quote"), scl, 0);
  return scl;
}

void
send_document_cb(struct evhttp_request *req, void *arg)
{
  struct evbuffer *evb = NULL;
  const char *uri = evhttp_request_get_uri(req);
  struct evhttp_uri *decoded = NULL;
  const char *method = NULL;
  const char *path   = NULL;
  const char *query  = NULL;
  char *decoded_path = NULL;
  unsigned char *data = NULL;
  char local_path[255];
  int len = 255;
  int status_code = 0;
  char *phrase = NULL;
  int bytes_out = 0;

  struct evkeyvalq *ihdrs;
  struct evkeyvalq *ohdrs;

  pointer sc_method = sc->NIL;
  pointer sc_path   = sc->NIL;
  pointer sc_params = sc->NIL;
  pointer sc_data   = sc->NIL;
  pointer sc_return = sc->NIL;
  pointer sc_args   = sc->NIL;
  pointer sc_headers= sc->NIL;

  pointer tmp = sc->NIL;

  struct stat st;

  method  = evhttp_method(evhttp_request_get_command(req));
  decoded = evhttp_uri_parse(uri);

  path    = evhttp_uri_get_path(decoded);
  query   = evhttp_uri_get_query(decoded);

  ihdrs   = evhttp_request_get_input_headers(req);

  /* uri unescape */
  decoded_path = evhttp_uridecode(path, 0, NULL);
  evutil_snprintf(local_path, len, "%s/%s", docroot, decoded_path);

  evb  = evbuffer_new();
  ohdrs = evhttp_request_get_output_headers(req);
  evhttp_add_header(ohdrs, "Server", "ioscheme");
  evhttp_add_header(ohdrs, "Connection", "close");

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

  sc_headers = headers_fi_cons(ihdrs);
  sc_params = query_fi_cons(query);
  sc_path   = mk_string(sc, path);
  sc_method = mk_string(sc, method);

  data      = evbuffer_pullup(evhttp_request_get_input_buffer(req), -1);
  if (data && (strlen((char*)data) > 0)) {
    sc_data   = mk_string(sc, (char*)data);
  } else {
    sc_data   = mk_string(sc, "");
  }
  if (evhttp_request_get_command(req) == EVHTTP_REQ_POST) {
    if (data && (strlen((char*)data) > 0)) {
      sc_data = query_fi_cons((char*)data);
    }
  }
  sc_args   = _cons(sc, sc_headers,sc_args, 0);
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
      evhttp_add_header(ohdrs, n, v);
    } else if (sc->vptr->is_string(p)) {
      const char *c;
      c = sc->vptr->string_value(p);
      evbuffer_add_printf(evb, "%s", c);
    } else {
      syslog(LOG_ERR, "unexpected return type from scheme '%s' function", entry_point);
    }

    if (tmp == sc->vptr->pair_cdr(tmp)) {
      syslog(LOG_ERR, "infinite loop detected");
      exit(1);
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

