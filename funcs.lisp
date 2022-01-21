; ======================================================================
; Some Lisp functions
; ======================================================================

(defun abs (x) 
	(if (< x 0) (* -1 x) x))

(defun consp (x) (not (atom x)))

(defun reverse (x)
	(if (null x) nil
			(append (reverse (cdr x)) (list (car x)))))
			
(defun first   (x) (car x))
(defun rest    (x) (cdr x))
(defun last    (x) (car (reverse x)))
(defun butlast (x) (reverse (cdr (reverse x))))

; ======================================================================
; qsort on lists of numbers
; ======================================================================

(defun qsort (L)
	(if (null L)
		nil
		(append
			(qsort (qsort-list< (car L) (cdr L)))
			(cons (car L) nil)
			(qsort (qsort-list>= (car L) (cdr L))))))

; Makes a copy of list L containing only elements that are < N.
(defun qsort-list< (N L)
	(if (null L)
		nil
		(if (< (car L) N)
			(cons (car L) (qsort-list< N (cdr L)))
			(qsort-list< N (cdr L)))))


; Makes a copy of list L containing only elements that are >= N.
(defun qsort-list>= (N L)
	(if (null L)
		nil
		(if (not (< (car L) N))
			(cons (car L) (qsort-list>= N (cdr L)))
			(qsort-list>= N (cdr L)))))

; ======================================================================
; List symbols
; ======================================================================

(defun list-symbols ()
	(do-symbols (s) 
		(cond ((boundp s) (print 'var...>) (prin1 s))
			  (t          (print 'func..>) (prin1 s))))
	(print '*done*) t)

