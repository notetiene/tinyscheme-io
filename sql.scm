; sql helper functions
(define run-query
  (lambda (sql)
    (let ((result (sqlite-query sql)))
      (if result result sqlite-error))))

