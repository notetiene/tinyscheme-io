

(define (sexp->html html)
    (cond ((pair? html) (string-append (apply string-append "<" (symbol->string (car html)) ">\n" (map sexp->html (cdr html))) "</" (symbol->string (car html)) ">\n"))
                  ((symbol? html) (string-append (symbol->string html) " "))
                          (#t html)))

(define (*error-hook*)
  (display
    (string-append
      "HTTP/1.1 500 Internal Server Error\n"
      "\n")))

(define (http_response html)
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

(define (receive data)
  (display
    (http_response
      (sexp->html
        '(html (head (title My page))
           (body
             (p hello world!)))))))

(display "Loaded web script.") (newline)

