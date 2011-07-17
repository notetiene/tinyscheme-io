
(load "html.scm")

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

