(load "html.scm")
(load "sql.scm")

(sqlite-open ":memory:")

(define run-query
  (lambda (sql)
    (let ((result (sqlite-query sql)))
      (if result result sqlite-error))))

(define (query->html q)
  (letrec ((fn (lambda (v i s)
            (if (= (vector-length v) i)
                s
                (fn v (+ i 1) (apply string-append s "<br/>" (vector-ref v i)))))))
    (fn q 0 "")))

(run-query "select sqlite_version();")
(run-query "create table hello (mycolumn text);")
(run-query "insert into hello (mycolumn) values ('test1');")
(run-query "insert into hello (mycolumn) values ('test2');")
(run-query "insert into hello (mycolumn) values ('test3');")
(run-query "insert into hello (mycolumn) values ('test4');")

(display (run-query "select * from hello"))
(newline)
(display (query->html (run-query "select * from hello")))
(newline)

(define receive
  (lambda (method path . parameters)
    (display parameters)(newline)
    (sexp->html
      `(html 
         (head 
           (style @ ((type "text/css")))
           (title My page))
           (body
             (p hello world!)
             (p method ,method)
             (p path   ,path))))))

(display "Loaded web script.")(newline)

