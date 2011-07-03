

(define (sexp->html html)
  (cond ((pair? html) 
           (string-append 
             (apply string-append "<" (symbol->string (car html)) ">\n" (map sexp->html (cdr html))) "</" (symbol->string (car html)) ">\n"))
        ((symbol? html) (string-append (symbol->string html) " "))
        (#t html)))



;(define (*error-hook*)
;  (display (string-append "HTTP/1.1 500 Internal Server Error\n" "\n")))

(define (http-response html)
  (string-append 
   "HTTP/1.1 200 OK\n"
   "Date:\n"
   "Server: ioscheme\n"
   "Last-Modified:\n"
   "ETag:\n"
   "Accept-Ranges: bytes\n"
   (string-append "Content-Length: " (number->string (string-length html)) "\n")
   "Connection: close\n"
   "Content-Type: text/html\n\n"
   html))

(define (http-method x)
  (car (string-split (car (string-split x #\lf)) #\space)))

(define (http-uri x)
  (car (cdr (string-split (car (string-split x #\lf)) #\space))))

(define (receive data)
  (display
    (http-response
      (sexp->html
        `(html (head (title My page))
           (body
             (p hello world!)
             (p method: ,(http-method (list->string (vector->list data))))
             (p uri:    ,(http-uri (list->string (vector->list data))))))))))

(display "Loaded web script.") (newline)

