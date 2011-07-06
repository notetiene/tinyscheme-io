

(define (sexp->html html)
  (cond ((pair? html) 
           (string-append 
             (apply string-append "<" (symbol->string (car html)) ">\n" (map sexp->html (cdr html))) "</" (symbol->string (car html)) ">\n"))
        ((symbol? html) (string-append (symbol->string html) " "))
        (#t html)))



;(define (*error-hook*)
;  (display (string-append "HTTP/1.1 500 Internal Server Error\n" "\n")))

(define (receive method path)
   (sexp->html
      `(html (head (title My page))
         (body
           (p hello world!)
           (p method: ,method)
           (p uri:    ,path)))))

(display "Loaded web script.") (newline)

