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

(run-query "SELECT sqlite_version();")
(run-query "CREATE TABLE notices (id INTEGER PRIMARY KEY, content TEXT);")

(define all-notices "SELECT id, content FROM notices;")

; HTML fragment for a record
(define (notice-html-fragment notice)
  (sexp->html
    `(div @ ((class "notice"))
        ,(number->string (car notice))
        ":"
        ,(apply string-append (cdr notice)))))

; Loops through SQL results
(define (query->html v)
  (letrec ((fn (lambda (i s)
    (if (= (vector-length v) i)
         s
         (fn (+ i 1) 
             (string-append (notice-html-fragment (vector-ref v i)) s))))))
    (fn 0 "")))

; View for index
(define (index-html notices)
  (sexp->html
    `(html
       (head
         (link @ ((type "text/css")(href "base.css")(rel "stylesheet")(media "screen")))
         (title "ioscheme demonstration"))
       (body
         (div @ ((id "main"))
           (p @ ((id "header")) (a @ ((href "/index")) (h1 "(ioscheme)")))
           (p (form @ ((method "post") (action "/index"))
                (input @ ((type "text") (name "content") (size "24")))
                (input @ ((type "hidden") (name "token") (value "1234")))
                (input @ ((type "submit") (value "post notice")))))
           (p ,(query->html notices))
           (p @ ((class "center")) (a @ ((href "/delete-all")) "delete notices")))))))

(define (index-html-auth)
  (sexp->html
    `(html
       (body
         (div @ ((id "main"))
           (p "401 Unauthorized"))))))

; Controller for /
(define (index-handler method body parameters headers)
  (cond
    ((string-ci=? method "get") 
      (list 
        (cons 200 "OK")
        (cons "Content-Type" "text/html")
        (index-html (run-query all-notices))))
    ((string-ci=? method "post")
      (run-query
        (string-append
          "INSERT INTO notices (id, content) VALUES (NULL, '"
            (sql-quote-escape (car(cdr(assoc "content" body))))
          "');"))
      (list 
        (cons 200 "OK")
        (cons "Content-Type" "text/html")
        (index-html (run-query all-notices))))
    (#t (list 
          (cons 405 "Method Not Allowed")
          (cons "Content-Type" "text/html")
          "Method not allowed"))))

(define (index-html-unauth)
  (sexp->html
    `(html
       (body
         (div @ ((id "main"))
           (p "401 Unauthorized"))))))

; Controller for /delete-all
(define (delete-handler method body parameters headers)
  (cond
    ((string-ci=? method "get") 
      (run-query "DELETE FROM notices;") 
        (list 
          (cons 200 "OK")
          (cons "Content-Type" "text/html")
          (index-html (run-query all-notices))))
    (#t (list 
          (cons 405 "Method Not Allowed")
          (cons "Content-Type" "text/html")
          "Method not allowed"))))

(define (authorized? headers)
  (if (assoc "Authorization" headers)
    (let ((auth (car (cdr (assoc "Authorization" headers)))))
      ;(display (base64-decode (substring auth 6 (string-length auth))))(newline)
      (if (string=? (substring auth 0 5) "Basic")
        (if (string=? (base64-decode (substring auth 6 (string-length auth))) "admin:password") #t #f)
      #f))
    #f))

(define (admin-handler method body parameters headers)
  (cond
    ((string-ci=? method "get")
      (if (authorized? headers)
        (list
          (cons 200 "OK")
          (cons "Content-Type" "text/html")
          (index-html (run-query all-notices)))
        (list 
          (cons 401 "Authorization Required")
          (cons "WWW-Authenticate" "Basic realm='Administration'")
          (cons "Content-Type" "text/html")
          (index-html-unauth))))))

; URL router
(define (receive method path body parameters headers)
  (cond ((string-ci=? path "/")           (index-handler method body parameters headers))
        ((string-ci=? path "/index")      (index-handler method body parameters headers))
        ((string-ci=? path "/delete-all") (delete-handler method body parameters headers))
        ((string-ci=? path "/admin")      (admin-handler method body parameters headers))
    (#t (list 
          (cons 404 "Not Found")
          (cons "Content-Type" "text/html")
          "Page not found" ))))

(display "Loaded web script.")(newline)

