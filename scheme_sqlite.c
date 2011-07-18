
#include "scheme-private.h"

/* the cons define collides with scheme interface */
#undef _cons
#undef cons

/*#include "sqlite3.h"*/
#include "sqlite3.c"

static sqlite3 *db = NULL;

static pointer scheme_sqlite_open(scheme *sp, pointer args) {
  char *filename;
  pointer arg1;
  int err;

  arg1 = sp->vptr->pair_car(args);
  if (!sp->vptr->is_string(arg1))
    return sp->F;
  args = sp->vptr->pair_cdr(args);
  filename = sp->vptr->string_value(arg1);

  err = sqlite3_open(filename, &db);

  return (err == SQLITE_OK) ? sp->T : sp->F;
}

static pointer scheme_sqlite_close(scheme *sp, pointer args) {
  sqlite3_close(db);
}

static pointer scheme_sqlite_query(scheme *sp, pointer args) {
  sqlite3_stmt *pst;
  pointer arg1;
  const char *sql;
  const char *sql2;
  int err;
  int cols;
  pointer sc_row;
  pointer sc_results;
  int num_rows = 0;
  int row_index = 0;
  pointer *rows;
  int i = 0;
  int page_size = 0;
  int max_rows = 65535;
  int rows_per_page;

  page_size = sysconf(_SC_PAGESIZE); /*getpagesize();*/
  rows_per_page = page_size / sizeof ( pointer );

#if 0
  printf("rows per page: %d\n", rows_per_page);
  printf("page_size: %d\n", page_size);
  printf("num of pages: %d\n", (int)(1 + (max_rows * sizeof(pointer) / page_size)) );
  printf("allocating: %d\n", (int) (1 + (max_rows * sizeof(pointer) / page_size)) * page_size );
#endif

  rows = valloc( (1 + (max_rows * sizeof(pointer) / page_size) * page_size ));

  arg1 = sp->vptr->pair_car(args);
  if (!sp->vptr->is_string(arg1))
    return sp->F;
  args = sp->vptr->pair_cdr(args);
  sql = sp->vptr->string_value(arg1);

  err = sqlite3_prepare_v2(db, sql, strlen(sql), &pst, &sql2);
  if (err == SQLITE_ERROR) {
    return sp->F;
  }
  sc_results = sp->vptr->mk_vector(sp, num_rows);
  while((SQLITE_ROW == (err = sqlite3_step(pst)))) {
    if (num_rows > max_rows) 
      break;
    cols = sqlite3_data_count(pst);
    if (cols > 0) {
      int i;
      sc_row = sp->NIL;

      for(i=(cols-1); i>=0; i--) {
        pointer sc_field;
        const char *field;
        /*
        sqlite3_value *v;
        int type;
        type = sqlite3_column_type(pst, i);
        if (type == SQLITE_INTEGER) {
        } else if (type == SQLITE3_TEXT) {
        }
        */
        field = sqlite3_column_text(pst, i);
        sc_field = sp->vptr->mk_string(sp, field);
        sc_row   = sp->vptr->cons(sp, sc_field, sc_row);
        /*v = sqlite3_column_value(pst, i);*/
      }
      rows[num_rows] = sc_row;
      num_rows++;
    } else {
      printf("no columns?!\n");
    }
  }

  sc_results = sp->vptr->mk_vector(sp, num_rows);
  for(i = 0 ; i < num_rows ; i++) {
    sp->vptr->set_vector_elem(sc_results, row_index, rows[i]);
  }

  free(rows);
  /*printf("sqlite err: %s\n", sqlite3_errmsg(db));*/
  err = sqlite3_finalize(pst);

  return (err == SQLITE_OK) ? sc_results : sp->F;
}

pointer scheme_sqlite_error(scheme *sp, pointer args) {
  const char *msg;
  msg = sqlite3_errmsg(db);
  return sp->vptr->mk_string(sp, msg);
}

void init_scheme_sqlite3(scheme *sp)
{
  /* register sqlite functions with scheme */
  sp->vptr->scheme_define(sp,sp->global_env,
    sp->vptr->mk_symbol(sp,"sqlite-open"),
    sp->vptr->mk_foreign_func(sp, scheme_sqlite_open));
  sp->vptr->scheme_define(sp,sp->global_env,
    sp->vptr->mk_symbol(sp,"sqlite-close"),
    sp->vptr->mk_foreign_func(sp, scheme_sqlite_close));
  sp->vptr->scheme_define(sp,sp->global_env,
    sp->vptr->mk_symbol(sp,"sqlite-query"),
    sp->vptr->mk_foreign_func(sp, scheme_sqlite_query));
  sp->vptr->scheme_define(sp,sp->global_env,
    sp->vptr->mk_symbol(sp,"sqlite-error"),
    sp->vptr->mk_foreign_func(sp, scheme_sqlite_error));
}
