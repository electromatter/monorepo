(tagbody
  repl
  (let ((val (read)))
    (cond
      ((equal val '+EOF+) (go end))
      (t (eval val) (go repl))))
  end)

(set-macro-and-name 'defmacro
  (lambda (form)
    (let ((name (cons 'quote (cons (car form) nil)))
          (func (cons 'lambda (cdr form))))
      (cons 'set-macro-and-name (cons name (cons func nil))))))

(defmacro defun (form)
  (let ((name (cons 'quote (cons (car form) nil)))
        (func (cons 'lambda (cdr form))))
    (cons 'set-function-and-name (cons name (cons func nil)))))

(defmacro defvar (form)
  (cons 'set form))

(defmacro if (form)
  (cons 'cond
    (cons (cons (car form) (cons (car (cdr form)) nil))
    (cond ((cdr (cdr form)) (cons (cons t (cdr (cdr form))) nil))))))

(defmacro when (form)
  (cons 'cond (cons form nil)))

(defun not (x)
  (if x
    nil
    t))

(defun macro-assoc-right (f default form)
  (let ((head default) (tail nil))
    (tagbody
      loop (when form
             (let ((cell (cons f (cons (car form) (cons default nil)))))
               (if tail
                 (progn (setcar tail cell) (set tail (cdr (cdr (car tail)))))
                 (progn (set head cell) (set tail (cdr (cdr cell))))))
             (set form (cdr form))
             (go loop)))
    head))

(defun macro-assoc-left (f default form)
  (let ((result default))
    (tagbody
      loop (when form
             (set result (cons f (cons result (cons (car form) nil))))
             (set form (cdr form))
             (go loop)))
    result))

(defmacro list (form)
  (macro-assoc-right 'cons nil form))

(defmacro or (form)
  (if form
    (let ((sym (gensym "or")))
      (list 'let (list (list sym (car form)))
        (list 'cond
          (list sym sym)
          (list 't (cons 'or (cdr form))))))
    nil))

(defmacro and (form)
  (if form
    (macro-assoc-left 'if (car form) (cdr form))
    't))

(defmacro append-head-tail (form)
  (let ((head-sym (car form))
        (tail-sym (car (cdr form)))
        (val-expr (car (cdr (cdr form))))
        (cell-sym (gensym "cell")))
    (when (or (cdr (cdr (cdr form)))
              (not (symbolp head-sym))
              (not (symbolp tail-sym)))
      (die "Expected (append-head-tail head-sym tail-sym val-expr)"))
    (list 'let (list (list cell-sym (list 'cons val-expr nil)))
      (list 'if tail-sym
        (list 'setcdr tail-sym cell-sym)
        (list 'set head-sym cell-sym))
      (list 'set tail-sym cell-sym))))

(defmacro map (form)
  (let ((func-expr (car form))
        (func (gensym "func")) (tag (gensym "tag"))
        (head (gensym "head")) (tail (gensym "tail"))
        (bindings (list (list func func-expr) (list head nil) (list tail nil)))
        (bindings-tail (cdr (cdr bindings)))
        (names nil) (names-tail nil)
        (cars nil) (cars-tail nil)
        (cdrs nil) (cdrs-tail nil)
        (lists (cdr form)))
    (when lists
      (tagbody
        loop (when lists
               (let ((expr (car lists))
                     (name (gensym "list")))
                 (append-head-tail bindings bindings-tail (list name expr))
                 (append-head-tail names names-tail name)
                 (append-head-tail cars cars-tail (list 'car name))
                 (append-head-tail cdrs cdrs-tail (list 'set name (list 'cdr name)))
                 (set lists (cdr lists))
                 (go loop))))
      (list 'let bindings
        (list 'tagbody
              tag (list 'when (cons 'and names)
                    (list 'append-head-tail head tail (cons 'funcall (cons func cars)))
                    (cons 'progn cdrs)
                    (list 'go tag)))
        head))))

(defmacro function (form)
  (list 'get-function (cons 'quote form)))

(defmacro macro-function (form)
  (list 'get-macro (cons 'quote form)))

(defun function-name (func)
  (get-function-name func))

(defmacro + (form)
  (macro-assoc-right 'add 0 form))

(defmacro * (form)
  (macro-assoc-right 'mul 1 form))

(defmacro - (form)
  (cond
    ((cdr form) (macro-assoc-left 'sub (car form) (cdr form)))
    (form (cons 'sub (cons 0 (cons (car form) nil))))
    (t 0)))

(defmacro / (form)
  (if form
    (macro-assoc-left 'quot (car form) (cdr form))
    1))

(defmacro logand (form)
  (macro-assoc-right 'logand2 -1 form))

(defmacro logior (form)
  (macro-assoc-right 'logior2 0 form))

(defmacro logxor (form)
  (macro-assoc-right 'logxor2 0 form))

(defmacro case (form)
  (let ((sym (gensym "case"))
        (fix (lambda (frm)
               (if (or (equal 'otherwise (car frm)) (equal 't (car frm)))
                 (cons t (cdr frm))
                 (cons (list 'equal sym (list 'quote (car frm))) (cdr frm))))))
    (list 'let (list (list sym (car form)))
      (cons 'cond (map fix (cdr form))))))

(defun special-operator-p (name)
  (case name
    (lambda t)
    (let t)
    (progn t)
    (quote t)
    (cond t)
    (set t)
    (tagbody t)
    (go t)))

(defun assoc (name alist)
  (let ((result nil))
    (tagbody
      loop (when alist
             (if (equal (car (car alist)) name)
               (set result (car alist))
               (progn (set alist (cdr alist)) (go loop)))))
    result))

(defun append2 (a b)
  (let ((head nil) (tail nil))
    (tagbody
      loop (when a
             (let ((cell (cons (car a) nil)))
               (if head
                 (setcdr tail cell)
                 (set head cell))
               (set tail cell)
               (set a (cdr a))
               (go loop))))
    (when b
      (if head
        (setcdr tail b)
        (set head b)))
    head))

(defmacro append (form)
  (macro-assoc-right 'append2 nil form))

(defvar *setf-forms* '())

(defmacro defsetf (form)
  (let ((access-fn (car form))
        (update-fn (car (cdr form))))
    (set *setf-forms*
      (cons
        (cons
          access-fn
          (if (symbolp update-fn)
            update-fn
            (eval (cons 'lambda (cdr form)))))
        *setf-forms*)))
  nil)

(defmacro setf (form)
  (let ((access-form (car form))
        (value-form (car (cdr form))))
    (if (symbolp access-form)
      (cons 'set form)
      (let ((setf-assoc (assoc (car access-form) *setf-forms*))
            (update-fn (cdr setf-assoc)))
        (when (not update-fn)
          (die "unknown setf form"))
        (append
          (if (symbolp update-fn)
            (list update-fn)
            (list 'funcall update-fn))
          (cdr access-form)
          (list value-form))))))

(defsetf car setcar)
(defsetf cdr setcdr)
(defsetf char setchar)
(defsetf fill-pointer set-fill-pointer)
(defsetf function set-function)
(defsetf macro-function set-macro)
(defsetf function-name set-function-name)

(defmacro incf (form)
  (list 'setf (car form) (list '+ (car form) 1)))

(defmacro decf (form)
  (list 'setf (car form) (list '- (car form) 1)))

(defmacro append-string (form)
  (macro-assoc-left 'strcat '(make-string) form))

(defun defcarfuns (name form)
  (when (> (fill-pointer name) 1)
    (let ((sym (symbol (append-string "c" name "r"))))
      (eval (list 'defun sym '(x) form))
      (eval (list 'defsetf sym '(x v) (list 'setf form 'v)))))
  (when (< (fill-pointer name) 4)
    (defcarfuns (append-string "a" name) (list 'car form))
    (defcarfuns (append-string "d" name) (list 'cdr form))))
(defcarfuns "" 'x)

(defun find (item seq)
  (tagbody
    loop (when (and seq (not (equal item (car seq))))
           (setf seq (cdr seq))
           (go loop)))
  (car seq))

(defmacro let* (form)
  (let ((orig-bindings (car form))
        (fresh-bindings (map (lambda (x) (cons (gensym "let") (cdr x))) orig-bindings))
        (updated-bindings (map (lambda (x y) (list (car x) (car y))) orig-bindings fresh-bindings))
        (body (cdr form)))
    (cons 'let (cons (append fresh-bindings updated-bindings) body))))

(defmacro named-lambda (form)
  (list 'set-function-name (cons 'lambda (cdr form)) (list 'quote (car form))))

(defun replace-funcs (fenv body)
  (let ((process-get-func (cons nil nil)) (process-set-func (cons nil nil))
        (process-call (cons nil nil)) (process-form (cons nil nil))
        (process-expr (cons nil nil)) (process-progn (cons nil nil))
        (process-let (cons nil nil)) (process-bind (cons nil nil))
        (g1 (setcar process-get-func (named-lambda process-get-func (form)
          (when (not (equal (caadr form) 'quote))
            (die "Expected (function name)"))
          (let ((found (cdr (assoc (cadadr form) fenv))))
            (if found found form)))))
        (g2 (setcar process-set-func (named-lambda process-set-func (form)
          (when (not (equal (caadr form) 'quote))
            (die "Expected (function name)"))
          (let ((found (cdr (assoc (cadadr form) fenv))))
            (if found
              (cons 'set (cons found (funcall (car process-progn) (cddr form))))
              (cons (car form) (cons (cadr form) (funcall (car process-progn) (cddr form)))))))))
        (g3 (setcar process-call (named-lambda process-call (form)
          (let ((found (cdr (assoc (car form) fenv))))
            (if found
              (cons 'funcall (cons found (funcall (car process-progn) (cdr form))))
              (cons (car form) (funcall (car process-progn) (cdr form))))))))
        (g4 (setcar process-form (named-lambda process-form (x)
          (let ((form (macroexpand x)))
            (case (car form)
              (progn (cons (car form) (funcall (car process-progn) (cdr form))))
              (tagbody (cons (car form) (funcall (car process-progn) (cdr form))))
              (funcall (cons (car form) (funcall (car process-progn) (cdr form))))
              (go form)
              (quote form)
              (lambda (cons (car form) (cons (cadr form) (funcall (car process-progn) (cddr form)))))
              (let (funcall (car process-let) form))
              (set (cons (car form) (cons (cadr form) (funcall (car process-progn) (cddr form)))))
              (cond (cons (car form) (cons (funcall (car process-expr) (cadr form)) (map (car process-progn) (cddr form)))))
              (set-function (funcall (car process-set-func) form))
              (get-function (funcall (car process-get-func) form))
              (t (funcall (car process-call) form)))))))
        (g5 (setcar process-expr (named-lambda process-expr (x)
          (if (consp x) (funcall (car process-form) x) x))))
        (g6 (setcar process-progn (named-lambda process-progn (x)
          (map (car process-expr) x))))
        (g7 (setcar process-let (named-lambda process-let (x)
          (cons (car x) (cons (map (car process-bind) (cadr x)) (funcall (car process-progn) (cddr x)))))))
        (g8 (setcar process-bind (named-lambda process-bind (x)
          (cons (car x) (cons (funcall (car process-expr) (cadr x)) nil))))))
    (funcall (car process-progn) body)))

(defmacro flet (form)
  (let ((orig-bindings (car form))
        (body (cdr form))
        (fenv nil)
        (head nil) (tail nil))
    (tagbody
      loop (when orig-bindings
             (let ((def (car orig-bindings))
                   (lam (cons 'named-lambda (cons (car def) (cons (cadr def) (replace-funcs fenv (cddr def))))))
                   (name (gensym (string (car def)))))
               (append-head-tail head tail (list name lam))
               (set fenv (cons (cons (car def) name) fenv))
               (set orig-bindings (cdr orig-bindings))
               (go loop))))
    (cons 'let (cons head (replace-funcs fenv body)))))

(defun set-intersection (a b)
  (let ((head nil) (tail nil))
    (tagbody
      loop (when a
             (let ((item (car a)))
               (when (and (not (find item head)) (find item b))
                 (append-head-tail head tail item)))
             (set a (cdr a))
             (go loop)))
    head))

(defun set-add (item s)
  (if (find item s)
    s
    (cons item s)))

(defmacro for-each (form)
  (let ((binds (car form))
        (body (cdr form))
        (tag (gensym "for-each"))
        (list-binds (map (lambda (b) (cons (gensym (append-string (string (car b)) "-list")) (cdr b))) binds))
        (item-binds (map (lambda (v l) (list (car v) (list 'car (car l)))) binds list-binds))
        (next-sets (map (lambda (l) (list 'set (car l) (list 'cdr (car l)))) list-binds))
        (names (map (function car) list-binds)))
    (when names
      (list 'let list-binds
        (list 'tagbody
          tag (list 'when (cons 'and names)
                (append (list 'let item-binds)
                  body
                  next-sets)
                (list 'go tag))
          'break)))))

(defmacro push (form)
  (when (cddr form)
    (die "expected (push val place)"))
  (let ((val (car form))
        (place (cadr form))
        (sym (gensym "push")))
    (list 'let (list (list sym place))
      (list 'setf place (list 'cons val sym)))))

(defmacro pop (form)
  (when (cdr form)
    (die "expected (pop place)"))
  (let ((place (car form))
        (sym (gensym "pop")))
    (list 'let (list (list sym place))
      (list 'setf place (list 'cdr sym))
      (list 'car sym))))

(defun member (item lst)
  (tagbody
    loop (when (and lst (not (equal (car lst) item)))
           (setf lst (cdr lst))
           (go loop)))
  lst)

(defmacro pushnew (form)
  (when (cddr form)
    (die "expected (pushnew val place)"))
  (let ((lst-sym (gensym "pushnew-lst"))
        (val-sym (gensym "pushnew-val"))
        (memb-sym (gensym "pushnew-memb"))
        (val (car form))
        (place (cadr form)))
    (list 'let (list (list lst-sym place)
                     (list val-sym val)
                     (list memb-sym (list 'member val-sym lst-sym)))
      (list 'if memb-sym
        val-sym
        (list 'setf place (list 'cons val-sym lst-sym))))))

(defmacro assoc-update (form)
  (when (cdddr form)
    (die "expected (assoc-update key value alist-place)"))
  (let ((key-sym (gensym "assoc-update-key"))
        (val-sym (gensym "assoc-update-val"))
        (alist-sym (gensym "assoc-update-alist"))
        (memb-sym (gensym "assoc-update-memb"))
        (key (car form))
        (val (cadr form))
        (alist-place (caddr form)))
    (list 'let (list (list key-sym key)
                     (list val-sym val)
                     (list alist-sym alist-place)
                     (list memb-sym (list 'assoc key-sym alist-sym)))
      (list 'if memb-sym
        (list 'setf (list 'cdr memb-sym) val-sym)
        (list 'setf alist-place (list 'cons (list 'cons key-sym val-sym) alist-sym))))))

(defmacro define-cons-struct (form)
  (let ((struct-name (car form))
        (constructor-name (symbol (append-string "make-" (string struct-name))))
        (accessor (cons '(cdr x) nil))
        (fields (cdr form))
        (accessors (map (lambda (field)
                          (let ((f (car accessor)))
                            (setf (car accessor) (list 'cdr f))
                            (list 'car f))) fields)))
    (list 'progn
      (list 'defun constructor-name ()
        (cons 'list (cons (list 'quote struct-name) (map (lambda (field) nil) fields))))
      (cons 'progn
        (map (lambda (field acc)
               (let ((name (symbol (append-string (string struct-name) "-" (string field)))))
                 (list 'progn
                  (list 'defun name '(x)
                    (list 'when (list 'not (list 'equal '(car x) (list 'quote struct-name)))
                      (list 'die (append-string "expected a " (string struct-name) " struct")))
                    acc)
                  (list 'defsetf name '(x v)
                    (list 'when (list 'not (list 'equal '(car x) (list 'quote struct-name)))
                      (list 'die (append-string "expected a " (string struct-name) " struct")))
                    (list 'setf acc 'v)))))
             fields
             accessors)))))

(defmacro unquote-splicing (form)
  (die "naked unquote-splicing"))

(defmacro unquote (form)
  (die "naked unquote"))

(defun expand-quasiquote (expr)
  (if (consp expr)
    (case (car expr)
      (unquote-splicing (die "invalid unquote-splicing not in list"))
      (unquote
        (when (or (cddr expr) (not (cdr expr)))
          (die "unquote must be (unquote expr)"))
        (cadr expr))
      (quasiquote
        (list 'quote expr))
      (t (let ((part-head nil) (part-tail nil)
               (head nil) (tail nil))
           (for-each ((item expr))
             (if (consp item)
               (case (car item)
                 (unquote
                   (when (or (cddr item) (not (cdr item)))
                     (die "unquote must be (unquote expr)"))
                   (if (equal (caar tail) 'list)
                     (append-head-tail part-head part-tail (cadr item))
                     (progn
                       (setf part-head (list 'list (cadr item)))
                       (setf part-tail (cdr part-head))
                       (append-head-tail head tail part-head))))
                 (unquote-splicing
                   (when (or (cddr item) (not (cdr item)))
                     (die "unquote-splicing must be (unquote-splicing expr)"))
                   (set part-head nil)
                   (set part-tail nil)
                   (append-head-tail head tail (cadr item)))
                 (t
                   (let ((val (expand-quasiquote item)))
                     (if (equal (caar tail) 'list)
                       (append-head-tail part-head part-tail val)
                       (progn
                         (setf part-head (list 'list val))
                         (setf part-tail (cdr part-head))
                         (append-head-tail head tail part-head))))))
               (if (equal (caar tail) 'quote)
                 (append-head-tail part-head part-tail item)
                 (progn
                   (setf part-head (list 'quote (list item)))
                   (setf part-tail (cadr part-head))
                   (append-head-tail head tail part-head)))))
             (if (eql head tail)
               (car head)
               (cons 'append head)))))
    (cons 'quote expr)))

(defmacro quasiquote (form)
  (when (cdr form)
    (die "quasiquote must be (quasiquote expr)"))
  (expand-quasiquote (car form)))

(define-cons-struct env
  lexical-macros
  dynamic-macros
  lexical-symbol-macros
  dynamic-symbol-macros
  atoms
  lexical-variables
  dynamic-variables
  lexical-functions
  dynamic-functions
  lexical-tags
  closures)

(defun env-get-macro (env name)
  (let ((macro nil))
    (for-each ((context (env-lexical-macros env)))
      (let ((lex-macro (cdr (assoc name context))))
        (when lex-macro
          (setf macro lex-macro)
          (go break))))
    (if macro
      macro
      (cdr (assoc name (env-dynamic-macros env))))))

(defun env-get-symbol-macro (env name)
  (let ((macro nil))
    (for-each ((context (env-lexical-symbol-macros env)))
      (let ((pair (assoc name context)))
        (when pair
          (setf macro pair)
          (go break))))
    (if macro
      macro
      (assoc name (env-dynamic-symbol-macros env)))))

(defun env-macroexpand (env expr)
  (cond
    ((and (consp expr) (symbolp (car expr)))
     (let ((macro (env-get-macro env (car expr))))
       (if macro
         (funcall macro (cdr expr) env)
         expr)))
    ((symbolp expr)
     (let ((macro (env-get-symbol-macro env expr)))
       (if macro
         (cdr macro)
         expr)))
    (t expr)))

(defun visit-defmacro (body env)
  (let ((name (car body))
        (macro-body (cdr body))
        (macro (eval (cons 'lambda macro-body))))
    (setf (function-name macro) name)
    (assoc-update name macro (env-dynamic-macros env))
    (visit-atom name env)))

(defun visit-macrolet (body env)
  (push () (env-lexical-macros env))
  (let ((binds (car body))
        (let-body (cdr body)))
    (for-each ((bind binds))
      (let ((name (car bind))
            (macro-body (cdr bind))
            (macro (eval (cons 'lambda macro-body))))
        (setf (function-name macro) name)
        (assoc-update name macro (car (env-lexical-macros env)))))
    (let ((reg (visit-progn let-body env)))
      (pop (env-lexical-macros env))
      reg)))

(defun visit-define-symbol-macro (body env)
  (when (cddr body)
    (die "expected (define-symbol-macro symbol expansion)"))
  (let ((name (car body))
        (expansion (cadr body)))
    (assoc-update name expansion (env-dynamic-symbol-macros env))
    (visit-atom name env)))

(defun visit-symbol-macrolet (body env)
  (push () (env-lexical-symbol-macros env))
  (let ((binds (car body))
        (let-body (cdr body)))
    (for-each ((bind binds))
      (when (cddr bind)
        (die "expected (symbol-macrolet ((name expansion)) implicit-progn)"))
      (assoc-update (car bind) (cadr bind) (car (env-lexical-symbol-macros env))))
    (let ((reg (visit-progn let-body env)))
      (pop (env-lexical-symbol-macros env))
      reg)))

(defun visit-closure (expr env )
  (let ((name (car expr))
        (args (cadr expr))
        (body (cddr expr)))
    (visit-progn body env)
    ;TODO: return closure
    ))

(defun visit-lambda (body env)
  (visit-closure (cons 'lambda body) env))

(defun visit-func (name env)
  (cond
    ((and (consp name) (equal (car name) 'lambda))
      (visit-closure name env))
    ((symbolp name)
     ; TODO load function
     )
    (t
      (die "expected function name"))))

(defun visit-call (args env)
  (for-each ((arg args))
    (visit-expr arg env))
  ;TODO: gather arguments and return value
  )

(defun visit-funcall (body env)
  (for-each ((item body))
    (if (symbolp item)
      (visit-tag item env)
      (visit-expr item env)))
  (visit-atom nil env))

(defun visit-operator (op args env)
  (visit-func op env)
  (visit-call args env))

(defun visit-flet (body env)
  (when (or (not body) (not (consp (car body))))
    (die "expected bindings in flet form"))
  (visit-progn (cdr body) env))

(defun visit-labels (body env)
  (when (or (not body) (not (consp (car body))))
    (die "expected bindings in labels form"))
  (visit-progn (cdr body) env))

(defun visit-let (body env)
  (when (or (not body) (not (consp (car body))))
    (die "expected bindings in let form"))
  (visit-progn (cdr body) env))

(defun visit-let* (body env)
  (when (or (not body) (not (consp (car body))))
    (die "expected bindings in let* form"))
  (visit-progn (cdr body) env))

(defun visit-expr1 (body env)
  (when (or (not body) (cdr body))
    (die "let binding must be (name expr)"))
  (visit-expr (car body) env))

(defun visit-var (name env)
  ;TODO var register or insert a load
  )

(defun visit-setq (body env)
  (when (cddr body)
    (die "setq must be (setq varname expr)"))
  (when (not (symbolp (car body)))
    (die "setq must be (setq varname expr)"))
  (visit-expr (cadr body) env))

(defun visit-atom (val env)
  (pushnew val (env-atoms env))
  ;TODO atom register
  )

(defun visit-tag (tag env)
  ;TODO define tag location
  )

(defun visit-go (body env)
  (print "GO")
  (when (cdr body)
    (die "go must be (go tag)"))
  (when (not (env-lexical-tags env))
    (die "naked go"))
  (let ((tag (car body)))
    (print tag))
  nil)

(defun visit-progn (body env)
  (for-each ((e body))
    (visit-expr e env)))

(defun visit-if (body env)
  (when (or (cdddr body) (not (cdr body)))
    (die "if must be (if cond yes no) or (if cond yes)"))
  ;TODO result register, jump
  (visit-expr (car body) env)
  (visit-expr (cadr body) env)
  (when (cddr body)
    (visit-expr (caddr body) env)))

(defun visit-quote (body env)
  (when (cddr body)
    (die "quote must have exactly one form"))
  (visit-atom (cadr body) env))

(defun visit-function (body env)
  (when (cdr body)
    (die "function must be (function name)"))
  (visit-func (car body) env))

(defun visit-tagbody (body env)
  (push () (env-lexical-tags env))
  (for-each ((item body))
    (if (symbolp item)
      (visit-tag item env)
      (visit-expr item env)))
  (pop (env-lexical-tags env))
  (visit-atom nil env))

(defun visit-defun (env body)
  ;TODO defun
  (visit-closure env body))

(defun visit-the (body env)
  (when (cddr body)
    (die "the must be (the type expr)"))
  (let ((expr-type (car body))
        (expr (cadr body)))
    (visit-expr expr env)))

(defun visit-expr (expr env)
  (let ((expr (env-macroexpand env expr)))
    (cond
      ((symbolp expr)
        (visit-var expr env))
      ((consp expr)
        (let ((operator (car expr))
              (body (cdr expr)))
          (case operator
            (defmacro (visit-defmacro body env))
            (macrolet (visit-macrolet body env))
            (define-symbol-macro (visit-define-symbol-macro body env))
            (symbol-macrolet (visit-symbol-macrolet body env))
            (defun (visit-defun body env))
            (lambda (visit-lambda body env))
            (flet (visit-flet body env))
            (labels (visit-labels body env))
            (let (visit-let body env))
            (let* (visit-let* body env))
            (setq (visit-setq body env))
            (the (visit-the body env))
            (function (visit-function body env))
            (if (visit-if body env))
            (tagbody (visit-tagbody body env))
            (go (visit-go body env))
            (quote (visit-quote body env))
            (progn (visit-progn body env))
            (funcall (visit-funcall body env))
            (t (visit-operator operator body env)))))
        (t (visit-atom expr env)))))

(visit-expr '(lambda ()
    (if 1 2)
    (if 1 2 3)
    (setq a 1)
    (tagbody (let ((x 1)) (go y)) y)
    (flet ((a (x) x)) (a 2))
    (labels ((a (x) (b x)) (b (x) x)) (a 3))
    (let ((x 1)) x)
    (let* ((x 1)) x)
    (defun a (x) x)
    (a 2)
    (lambda (x) x)
    ((lambda (x) x) 1)
    (defvar a 1)
    (symbol-macrolet ((s b))
      s)
    (define-symbol-macro z a)
    (defmacro m (body env)
      (cons 'a (cons 'a nil)))
    (macrolet ((m2 (body env) (car body)))
      (m2 1))
    (a z)
    a
    (the fixnum 1)
    (funcall (lambda (x) x) 1)
    (funcall (function (lambda (x) x)) 1)
  ) (make-env))

;struct gc_record {
;  void *prev;
;  int n;
;  value *vars;
;};
;
;;need to recognize switch/flatten cond
;;closure->let and function inlining
;
;void *gc_record_mark(void *p) {
;  struct gc_record_funcname *r = p;
;  value i = 0;
;  while (i < r->n) {
;    mark(r->vars[i]);
;    i++;
;  }
;  return r->prev;
;}
;
;struct closure {
;  void (*func)();
;  int nargs, n;
;  value a;
;  value b;
;  value c;
;};
;
;struct funcname {
;  void *prev;
;  int n;
;  value var1;
;  value var2;
;};
;
;; closure
;value funcname(void *g, void *c, value arg1, value arg2) {
;  struct funcname l = {g, 10, arg1, arg2, 0, 0, 0};
;  struct closure *cl = c;
;  struct func *func = l.f;
;  if (func->nargs == 3)
;    die();
;  func->func((void*)&l, (void*)func, l.x, l.y, c->a);
;  switch (c) {
;  case imm:
;  }
;label:
;  if (x) {
;  } else if (y) {
;    goto label;
;  } else {
;  }
;  return 0;
;}
;

;----------------------------
; Delay evaluation

;(defmacro defcons (form)
;  nil)
;
;(defmacro defcfun (form)
;  nil)
;
;(defvar internalized nil)
;
;;----------------------------
;; Low-level immediate values
;
;; All immediate values are C longs. The lower few bits determine the type.
;; native-fixnum  = long
;; native-pointer = void*
;
;; Pointers to conses are 0 mod 8. This is guaranteed by the allocator.
;(defcfun cons-pointerp (val)
;  (equal (logand (transmute native-fixnum val) 7) 0))
;
;(defcfun box-cons-pointer (val)
;  (declare (type native-pointer val))
;  (transmute t val))
;
;(defcfun unbox-cons-pointer (val)
;  (declare (type t val))
;  (transmute native-pointer val))
;
;; Fixnums are 1,3,5,7 mod 8.
;(defcfun fixnump (val)
;  (= (logand (transmute native-fixnum val) 1) 1))
;
;(defcfun box-fixnum (val)
;  (declare (type native-fixnum val))
;  (transmute fixnum (logior (ash val 1) 1)))
;
;(defcfun unbox-fixnum (val)
;  (declare (type fixnum val))
;  (ash (transmute native-fixnum val) -1))
;
;; Characters are 2 mod 8.
;(defcfun characterp (val)
;  (= (logand (transmute native-fixnum val) 7) 2))
;
;(defcfun box-character (val)
;  (declare (type native-fixnum val))
;  (transmute character (logior (ash val 3) 2)))
;
;(defcfun unbox-character (val)
;  (declare (type character val))
;  (ash (transmute native-fixnum val) -3))
;
;; Compile time interned symbols are 4 mod 8.
;; As a special case, nil is always compiled to 0.
;(defcfun internp (val)
;  (or
;    (= (transmute native-fixnum val) 0)
;    (= (logand (transmute native-fixnum val) 7) 4)))
;
;;----------------------------
;; Define types that can be consed
;
;(defcons cell
;  (car t)
;  (cdr t))
;
;(defcons fixvec
;  (fill fixnum)
;  (data block))
;
;(defcons bigvec
;  (fix t))
;
;(defcons bignum
;  (nums '(block fixnum)))
;
;(defcons sym
;  (name '(block character)))
;
;(defcons file
;  (f ptr))
;
;;----------------------------
;; The garbage collector and allocator
;
;(defcfun make-cons (ty size)
;  ;alloc
;  ;fill-in object header
;  )
;
;(defcfun gc ()
;  ;mark+alloc to-space
;  ;fix pointers
;  ;call destructors
;  ;move objects
;  )
;
;;----------------------------
;; The C generator
;
;; Basic functions
;; Stream Input/Output
;; Arithmetic functions
;; Vector functions
;; Character functions
;; The top-level
;; The reader
;; The printer
;; The interpreter
;; The bytecode compiler
;
;(defun emit (filename)
;  (let ((file (fopen filename t)))
;    ;
;    (fclose file)))
;
;
;; Finally generate the C sources
;(emit "out.c")
;
;;(compile '(
;;
;;(from-libc 'stdlib.h (fun abort () nil)
;;(from-libc "stdlib.h" malloc (native-fixnum) native-pointer)
;;(from-libc "stdlib.h" realloc (native-pointer native-fixnum) native-pointer)
;;(from-libc "stdlib.h" free (native-pointer) ())
;;(from-libc "stdio.h" fopen (native-pointer native-pointer) native-pointer)
;;(from-libc "stdio.h" fclose (native-pointer) nil)
;;(from-libc "stdio.h" fputc (native-fixnum native-pointer) native-fixnum)
;;(from-libc "stdio.h" fgetc (native-pointer) native-fixnum)
;;(from-libc "stdio.h" fseek (native-pointer native-fixnum native-fixnum) native-fixnum)
;;(from-libc "stdio.h" ftell (native-pointer) native-fixnum)
;;(from-libc "stdio.h" EOF native-fixnum)
;;(from-libc "stdio.h" stdin native-pointer)
;;(from-libc "stdio.h" stdout native-pointer)
;;(from-libc "stdio.h" stderr native-pointer)
;;
;;(defun getc (stream))
;;(defun ungetc (stream c))
;;
;;(defun make-string ())
;;(defun intern (s))
;;(defun vector-push-extend (c s))
;;(defun char-code (c))
;;(defun code-char (code))
;;(defun setcar (c x))
;;(defun setcdr (c x))
;;(defun car (c))
;;(defun cdr (c))
;;
;;(defmacro setf (form))
;;
;;(defun skipline (stream)
;;  (tagbody
;;    loop (let ((c (getc stream)))
;;           (when (and c (not (equal c #\Newline)))
;;             (go loop)))))
;;
;;(defun skipws (stream)
;;  (tagbody
;;    loop (let ((c (getc stream)))
;;           (case c
;;             (#\;       (skipline stream) (go loop))
;;             (#\Space   (go loop))
;;             (#\Tab     (go loop))
;;             (#\Page    (go loop))
;;             (#\Return  (go loop))
;;             (#\Newline (go loop))
;;             (t         (ungetc stream c))))))
;;
;;(defun read-string (stream)
;;  (let ((result (make-string)))
;;    (tagbody
;;      loop (let ((c (getc stream)))
;;             (case c
;;               (nil (die "Reached EOF with unclosed \""))
;;               (#\")
;;               (#\\ (let ((c (getc stream)))
;;                      (when (not c)
;;                        (die "Expected single escape after \\"))
;;                      (vector-push-extend c result)
;;                      (go loop)))
;;               (t   (vector-push-extend c result)
;;                    (go loop)))))
;;    result))
;;
;;(defun read-quote (stream kind)
;;  (let ((form (read stream)))
;;    (when (not form)
;;      (die "Expected from after ' or `"))
;;    (cons kind form)))
;;
;;(defun read-comma (stream)
;;  (let ((c (getc stream))
;;        (kind (if (equal #\@ c)
;;                'unquote-splicing
;;                (progn (ungetc stream c) 'unquote)))
;;        (form (read stream)))
;;    (when (not form)
;;      (die "Expected form after ,"))
;;      (cons kind form)))
;;
;;(defun read-cons (stream)
;;  (let ((head nil) (tail nil))
;;    (tagbody
;;      loop
;;        (skipws stream)
;;        (let ((c (getc stream)))
;;          (case c
;;            (nil (die "Reached EOF with unclosed ("))
;;            (#\))
;;            (t   (let ((form (read stream))
;;                       (cell (cons form nil)))
;;                   (when (not form)
;;                     (die "Reached EOF but expected form"))
;;                   (if tail
;;                     (setf head cell)
;;                     (setf head (cons form nil)))
;;                   (setf tail cell))))))
;;    head))
;;
;;(defun is-delimiter (c)
;;  (case c
;;    (#\Space t) (#\Tab t) (#\Page t) (#\Return t) (#\Newline t)
;;    (#\" t)     (#\' t)   (#\( t)    (#\) t)      (#\, t)
;;    (#\` t)     (#\; t)   (nil t)
;;    (t nil)))
;;
;;(defun digit (base c)
;;  (let ((zero (char-code #\0)) (nine (char-code #\9))
;;        (a    (char-code #\a)) (z    (char-code #\z))
;;        (acap (char-code #\A)) (zcap (char-code #\Z))
;;        (val (cond
;;               ((and (>= code zero) (<= code nine)) (- code zero))
;;               ((and (>= code a)    (<= code z))    (+ (- code a) 10))
;;               ((and (>= code capa) (<= code capz)) (+ (- code capa) 10))
;;               (t 36))))
;;    (when (< val base)
;;      val)))
;;
;;(defun parse-number (tok)
;;  nil);COMBAK number
;;
;;(defun parse-character (tok)
;;  (case tok
;;    ("#\Nul"       #\Null)
;;    ("#\Null"      #\Null)
;;    ("#\Delete"    #\Rubout)
;;    ("#\Formfeed"  #\Page)
;;    ("#\Linefeed"  #\Page)
;;    ("#\Newline"   #\Newline)
;;    ("#\Page"      #\Page)
;;    ("#\Return"    #\Return)
;;    ("#\Rubout"    #\Rubout)
;;    ("#\Space"     #\Space)
;;    ("#\Tab"       #\Tab)
;;    (t             nil)));COMBAK symbol
;;
;;(defun parse-symbol (tok)
;;  (let ((i 0) (j 0))
;;    (tagbody
;;      loop (when ((< i (length tok)))
;;             (when (equal (char tok i) #\\)
;;               (setf i (+ i 1)))
;;             (setf (char tok j) (char tok i))
;;             (setf i (+ i 1))
;;             (setf j (+ j 1))
;;             (go loop)))
;;    (setf (fill-pointer tok) j))
;;  (intern tok))
;;
;;(defun read-token (stream)
;;  (let ((tok (make-string)))
;;    (tagbody
;;      loop (let ((c (getc stream)))
;;             (cond
;;               ((is-delimiter c) (ungetc stream c))
;;               ((equal c #\\)    (vector-push-extend c tok)
;;                                 (let ((c (getc stream)))
;;                                   (when (not c)
;;                                     (die "Expected character after \\"))
;;                                   (vector-push-extend c tok)))
;;               (t                (vector-push-extend c tok)
;;                                 (go loop)))))
;;    (let ((num (parse-number tok))
;;          (sym (parse-symbol tok)))
;;      (cond
;;        (num num)
;;        (sym sym)
;;        (t (parse-symbol tok))))))
;;
;;(defun read (stream)
;;  (skipws stream)
;;  (let ((c (getc stream)))
;;    (case c
;;      (nil nil)
;;      (#\" (read-string stream))
;;      (#\' (read-quote stream 'quote))
;;      (#\` (read-quote stream 'quasiquote))
;;      (#\, (read-comma stream))
;;      (#\( (read-cons stream))
;;      (#\) (die "Extra )"))
;;      (t   (read-token stream)))))
;;
;;))

(gc)
(print (symbol (append-string "(room) => " (int->str (/ (room) 1000) 10) " KB")))
