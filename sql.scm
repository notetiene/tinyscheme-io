
(define run-query
  (lambda (sql)
    (let ((result (sqlite-query sql)))
      (if result
         (display result)
         (display (sqlite-error))))
      (newline)))

