
(define (substring? str s)
  (letrec ((subsearch (lambda (x)
                        (if (= x (- (string-length str) (string-length s))) 
                          #f  
                          (if (string=? s (substring str x (+ x (string-length s))))
                            #t
                            (subsearch (+ 1 x)))))))
    (subsearch 0) ))


(define (string-split str c)
  (letrec ((splitr (lambda (x y l)
                    (if (= y (string-length str))
                      (reverse l)
                      (if (char=? c (string-ref str y))
                        (splitr (+ 1 y) (+ 1 y) (cons (substring str x y) l))
                        (splitr x (+ 1 y) l))))))
    (splitr 0 0 '())))

(define tests
  (list (substring? "HELLO WORLD" "WORLD")
        (substring? "HELLO WORLD" "YELLOW")
        (substring? "HELLO WORLD" "H")
        (substring? "HELLO WORLD" "W")
        (substring? "HELLO WORLD" "ZZZ")
         ))

(define run-test
  (lambda(x)
    (display (if x "YES" "NO")) (newline)))

(for-each run-test tests)

(display (string-split "HELLOXWORLDXTHISXWORKSX" #\X))(newline)
(display (string-split "GET / HTTP/1.0\nContent-accept: text/html\n\n" #\lf))(newline)

