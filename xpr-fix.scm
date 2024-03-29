;; xpr-fix.scm - xpr-fix in CHICKEN Scheme
;;
;; USAGE
;;     csc -o xpr-fix xpr-fix.scm
;;     ./xpr-fix FIX EXPR
;; where: FIX = prefix | infix | postfix
;;
;; DEPENDS
;; - string-tokenize: <github.com/foggynight/string-tokenize-egg>
;;
;; Copyright (C) 2022 Robert Coffey
;; Released under the MIT license.

;; Grammars --------------------------------------------------------------------
;;
;; For all grammars, the following symbols are defined:
;;
;;     N -> [0-9]+
;;     O -> + | - | * | /
;;
;;------------------------------------------------------------------------------

(import (chicken format)
        (chicken io)
        (chicken process-context)
        string-tokenize)

;; string ----------------------------------------------------------------------

(define (string-downcase str)
  (define len (string-length str))
  (if (zero? len)
      ""
      (let ((out (make-string len)))
        (do ((i 0 (+ i 1)))
            ((= i len))
          (string-set! out i (char-downcase (string-ref str i))))
        out)))

;; tree ------------------------------------------------------------------------

(define (tree-invert tree)
  (if (not (list? tree))
      tree
      (list (car tree)
            (tree-invert (caddr tree))
            (tree-invert (cadr tree)))))

;; token -----------------------------------------------------------------------
;;
;; token types: 'n (number), 'o (operation), 'p (parenthesis)

(define-record-type token
  (%make-token t v)
  token?
  (t tok-t)  ; type: symbol
  (v tok-v)) ; value: number

(define-record-printer (token tok out)
  (fprintf out "#<token ~A ~A>" (tok-t tok) (tok-v tok)))

(define (make-token t #!optional (v (void)))
  (cond ((not (symbol? t)) (error 'make-token "type must be a symbol" t))
        ((not (memq t '(n o lp rp))) (error 'make-token "invalid token type" t))
        (else (%make-token t v))))

;; lexer -----------------------------------------------------------------------

(define (lex-num word)
  (define num (string->number word))
  (if num (make-token 'n num) #f))

(define (lex-op word)
  (if (and (= (string-length word) 1)
           (memv (string-ref word 0) '(#\+ #\- #\* #\/)))
      (make-token 'o (string->symbol word))
      #f))

(define (lex-paren word)
  (cond ((not (= (string-length word) 1)) #f)
        ((char=? (string-ref word 0) #\() (make-token 'lp word))
        ((char=? (string-ref word 0) #\)) (make-token 'rp word))
        (else #f)))

(define (lex-line line)
  (define (lex-words words)
    (define tokens '())
    (define tok)
    (let loop ((w words))
      (cond ((null? w) (set! tok #f))
            ((begin (set! tok (lex-num (car w))) tok))
            ((begin (set! tok (lex-op (car w))) tok))
            ((begin (set! tok (lex-paren (car w))) tok))
            (else (error 'lex-line "invalid token" word)))
      (if (not tok)
          (reverse tokens)
          (begin (set! tokens (cons tok tokens))
                 (loop (cdr w))))))
  (lex-words (string-tokenize line " \t\n" "+-*/()")))

;; parser ----------------------------------------------------------------------

(define tok-head)
(define tok-tail)

(define (init! tok-lst)
  (set! tok-tail tok-lst)
  (consume!))

(define (consume!)
  (if (null? tok-tail)
      (set! tok-head #f)
      (begin (set! tok-head (car tok-tail))
             (set! tok-tail (cdr tok-tail)))))

(define (next) tok-head)
(define (done?) (not (next)))
(define (match-type? type) (eq? (tok-t (next)) type))
(define (match-value? value) (eqv? (tok-v (next)) value))

(define (expect! type)
  (if (match-type? type)
      (consume!)
      (error 'expect!
             (printf "token type mismatch\n  expected: ~A\n  received: ~A\n"
                     type (tok-t (next))))))

(define (parse-num!)
  (if (match-type? 'n)
      (let ((tok (next)))
        (consume!)
        (tok-v tok))
      (error 'parse-num! "failed to parse number" (next))))

(define (parse-op!)
  (if (match-type? 'o)
      (let ((tok (next)))
        (consume!)
        (tok-v tok))
      (error 'parse-op! "failed to parse operator" (next))))

;; LL(1) recursive descent parser for the following grammar:
;;
;;     E -> N | O E E
;;
(define (parse-prefix line)
  (define (parse-expr!)
    (cond ((done?) (error 'parse-expr! "missing tokens"))
          ((match-type? 'n) (parse-num!))
          ((match-type? 'o) (list (parse-op!)
                                  (parse-expr!)
                                  (parse-expr!)))
          (else (error 'parse-expr! "invalid token" (next)))))
  (init! (lex-line line))
  (let ((tree (parse-expr!)))
    (if (done?)
        tree
        (error 'parse-prefix "too many tokens"))))

;; Stack automaton parser for the following grammar:
;;
;;     E -> N | E E O
;;
(define (parse-postfix line)
  (define stk '())
  (define (parse-expr!)
    (unless (done?)
      (cond ((match-type? 'n) (set! stk (cons (parse-num!) stk)))
            ((match-type? 'o)
             (if (or (null? stk) (null? (cdr stk)))
                 (error 'parse-expr! "missing arguments" (tok-v (next)))
                 (set! stk (cons (list (parse-op!)
                                       (cadr stk)
                                       (car stk))
                                 (cddr stk)))))
            (else (error 'parse-expr! "invalid token" (next))))
      (parse-expr!)))
  (init! (lex-line line))
  (parse-expr!)
  (cond ((null? stk) (error 'parse-postfix "missing tokens"))
        ((not (null? (cdr stk))) (error 'parse-postfix "too many tokens"))
        (else (car stk))))

;; LL(1) recursive descent parser for the following grammar:
;;
;;     E -> N R
;;     R -> e | O E
;;
;; RNN: Right-Associative No-Precedence No-Parenthesis
(define (parse-infix-rnn line)
  (define (E!)
    (cond ((done?) (error 'E! "missing tokens"))
          ((match-type? 'n)
           (let* ((left (parse-num!))
                  (right (R!)))
             (if (not right)
                 left
                 (list (car right) left (cadr right)))))
          (else (error 'E! "invalid token" (next)))))
  (define (R!)
    (cond ((done?) #f)
          ((match-type? 'o) (list (parse-op!) (E!)))
          (else (error 'R! "invalid token" (next)))))
  (init! (lex-line line))
  (let ((tree (E!)))
    (if (done?)
        tree
        (error 'parse-infix-rnn "too many tokens"))))

;; LL(1) recursive descent parser for the following grammar:
;;
;;     EXPR -> TERM | TERM [+,-] EXPR
;;     TERM -> FACT | FACT [*,/] TERM
;;     FACT -> NUM
;;
;; RPN: Right-Associative Precedence No-Parenthesis
(define (parse-infix-rpn line)
  (define (EXPR!)
    (cond ((done?) (error 'EXPR! "missing tokens"))
          (else (let ((term (TERM!)))
                  (cond ((done?) term)
                        ((or (match-value? '+)
                             (match-value? '-))
                         (let ((op (parse-op!)))
                           (list op term (EXPR!))))
                        (else (error 'EXPR! "invalid token" (next))))))))
  (define (TERM!)
    (cond ((done?) (error 'TERM! "missing tokens"))
          (else (let ((fact (FACT!)))
                  (cond ((done?) fact)
                        ((or (match-value? '*)
                             (match-value? '/))
                         (let ((op (parse-op!)))
                           (list op fact (TERM!))))
                        (else fact))))))
  (define (FACT!)
    (cond ((done?) (error 'FACT! "missing tokens" (next)))
          ((match-type? 'n) (parse-num!))
          (else (error 'FACT! "invalid token" (next)))))
  (init! (lex-line line))
  (let ((tree (EXPR!)))
    (if (done?)
        tree
        (error 'parse-infix-rpn "too many tokens"))))

;; RR(1) recursive descent parser for grammar:
;;
;;     E  -> EL T
;;     EL -> EL T [+,-] | e
;;     T  -> TL F
;;     TL -> TL F [*,/] | e
;;     F  -> ( E ) | NUM
;;
;; LPP: Left-Associative Precedence Parenthesis
(define (parse-infix-lpp line)
  (define (E!)
    (cond ((done?) (error 'E! "missing tokens"))
          (else (let* ((t (T!))
                       (el (EL!)))
                  (if (not el) t (list (car el) t (cadr el)))))))
  (define (EL!)
    (cond ((done?) #f)
          ((or (match-value? '+)
               (match-value? '-))
           (let* ((op (parse-op!))
                  (t (T!))
                  (el (EL!)))
             (if (not el)
                 (list op t)
                 (list op (list (car el) t (cadr el))))))
          (else #f)))
  (define (T!)
    (cond ((done?) (error 'T! "missing tokens"))
          (else (let* ((f (F!))
                       (tl (TL!)))
                  (if (not tl) f (list (car tl) f (cadr tl)))))))
  (define (TL!)
    (cond ((done?) #f)
          ((or (match-value? '*)
               (match-value? '/))
           (let* ((op (parse-op!))
                  (f (F!))
                  (tl (TL!)))
             (if (not tl)
                 (list op f)
                 (list op (list (car tl) f (cadr tl))))))
          (else #f)))
  (define (F!)
    (cond ((done?) (error 'F! "missing tokens"))
          ((match-type? 'n) (parse-num!))
          ((match-type? 'rp)
           (consume!)
           (let ((e (E!)))
             (if (match-type? 'lp)
                 (begin (consume!) e)
                 (error 'F! "missing open bracket"))))
          ((match-type? 'lp) (error 'F! "invalid open bracket"))
          (else (error 'F! "invalid token" (next)))))
  (init! (reverse (lex-line line)))
  (let ((tree (E!)))
    (if (done?)
        (tree-invert tree)
        (error 'parse-infix-lpp "too many tokens"))))

;; main ------------------------------------------------------------------------

(define (main fix expr)
  (define parser
    (cond
     ((member fix '("pre" "prefix")) parse-prefix)
     ((member fix '("in" "infix")) parse-infix-lpp)
     ((member fix '("post" "postfix")) parse-postfix)))
  (print (parser expr)))

(let* ((args (command-line-arguments))
       (argc (length args)))
  (unless (= argc 2) (error 'args "invalid number of arguments"))
  (main (string-downcase (car args)) (cadr args)))
