; sql helper functions
(define run-query
  (lambda (sql)
    ;(display sql)(newline)
    (let ((result (sqlite-query sql)))
      (if result result sqlite-error))))

