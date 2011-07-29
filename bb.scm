; ioscheme bulletin board example
; 
; To make this work, run:
;
;   ./ioscheme -v -p 8000 init.scm bb.scm
;
; Then visit in a web browser:
;   http://localhost:8000/
;
(load "html.scm")
(load "sql.scm")

(sqlite-open "data-bb.sqlite3")

(run-query "SELECT sqlite_version()")
(run-query "CREATE TABLE notices (content TEXT);")

(define all-notices "SELECT * FROM notices;")

(define (notice-html-fragment notice)
  (sexp->html
    `(div @ ((class "notice"))
        ,(apply string-append notice))))

(define (query->html v)
  (letrec ((fn (lambda (i s)
    (if (= (vector-length v) i)
         s
         (fn (+ i 1) 
             (string-append (notice-html-fragment (vector-ref v i)) s))))))
    (fn 0 "")))

(define (index-html notices)
  (sexp->html
    `(html
       (head
         (link @ ((type "text/css")(href "base.css")(rel "stylesheet")(media "screen")))
         (title My page))
       (body
         (div @ ((id "main"))
           (p @ ((id "header")) (a @ ((href "/index")) "(ioscheme)"))
           (p (form @ ((method "post") (action "/index"))
                (input @ ((type "text") (name "content")))
                (input @ ((type "hidden") (name "token") (value "1234")))
                (input @ ((type "submit")))))
           (p ,(query->html notices)))))))

(define (index-handler method body parameters)
  (cond
    ((string-ci=? method "get") (index-html (run-query all-notices)))
    ((string-ci=? method "post")
      (run-query
        (string-append
          "INSERT INTO notices (content) VALUES ('"
          (symbol->string(cdr(assq 'content body)))
          "');"))
      (index-html (run-query all-notices)))
    (#t (cons 405 "Method Not Allowed"))))

(define (delete-handler method body parameters)
  (cond
    ((string-ci=? method "get") 
      (run-query "DELETE FROM notices;") (index-html (run-query all-notices)))
    (#t (cons 405 "Method Not Allowed"))))

(define (receive method path body parameters)
    (cond ((string-ci=? path "/")           (index-handler method body parameters))
          ((string-ci=? path "/index")      (index-handler method body parameters))
          ((string-ci=? path "/delete-all") (delete-handler method body parameters))
      (#t (cons 404 "Not Found"))))

;(display "Loaded web script.")(newline)


