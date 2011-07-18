
(load "html.scm")

(sqlite-open ":memory:")

(define run-query
  (lambda (sql)
    (let ((result (sqlite-query sql)))
      (if result
        (display result)
        (display (sqlite-error))))
      (newline)))

(run-query "select sqlite_version()")
(run-query "create table hello ( mycolumn text )")
(run-query "insert into hello (mycolumn) values ('test1')")
(run-query "select * from hello")

(define (receive method path)
  (sexp->html
    `(html 
       (head 
         (style @ ((type "text/css")))
         (title My page))
         (body
           (p hello world!)
           (p method: ,method)
           (p uri:    ,path)))))

(display "Loaded web script.") (newline)

