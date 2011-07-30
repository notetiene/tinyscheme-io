; sql helper functions
(define run-query
  (lambda (sql)
    (let ((result (sqlite-query sql)))
      (if result result sqlite-error))))

(define (sql-quote-escape s)
  (letrec ((escaper (lambda (x es)
    (if (= x (string-length s))
      es
      (if (char=? #\' (string-ref s x))
        (escaper (+ x 1) (string-append es (string #\' #\')))
        (escaper (+ x 1) (string-append es (string (string-ref s x)))))))))
  (escaper 0 "")))


;(display (sql-quote-escape "what's up")) (newline)

