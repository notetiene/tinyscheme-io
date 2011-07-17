
(load "html.scm")

(sqlite-open ":memory:")

(define run-query
  (lambda (sql)
    (let ((result (sqlite-query sql)))
      (if result
        (display result)
        (display (sqlite-error))))
      (newline)))

(run-query "create table test ( col1 integer );")
(run-query "select distinct sqlite_version() from sqlite_master;")

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

