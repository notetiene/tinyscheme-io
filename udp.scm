
(define (number->hex c)
  (string-append 
    (if (>= c 16)
      (number->hex (inexact->exact (truncate(/ c 16))))
      "0x")
    (substring "0123456789ABCDEF" (modulo c 16) 
                                   (+ 1 (modulo c 16)))))

(define (list->hex l)
  (for-each (lambda (n) (display (number->hex n))) l))

(define (receive data) 
  (display
    (string-append ": "
      (list->string 
        (vector->list data)))))

(display "Loaded script.") (newline)

