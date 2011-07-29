

(define (receive method path body parameters)
  (list
    (cons 200 "OK")
    (cons "Content-Type" "text/html")
    "Hello, World!"))

