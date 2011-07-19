
(define (sexp->attributes l)
  (apply 
    string-append 
    " "
    (map (lambda (x)
      (string-append
        (symbol->string (car x))
        "="
        "\"" (car(cdr x)) "\""
        ))
     l)))

(define (sexp->html html)
  (cond ((pair? html)
           (string-append 
             (apply string-append
               "<" (symbol->string (car html))
               (if (eq? (cadr html) '@)
                 (sexp->attributes (caddr html))
                  " ")
               ">\n"
               (if (eq? (cadr html) '@)
                 (map sexp->html (cdddr html))
                 (map sexp->html (cdr html))))
                "</" (symbol->string (car html)) ">\n"))
        ((symbol? html) (string-append (symbol->string html) " "))
        (#t html)))

(display (sexp->html '(test @ ((hello "world")))))

