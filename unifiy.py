# pylint: disable=invalid-name


"An implementation of first-order unification."


class UnifyError(Exception):
    "Unification failed."


class Unify:
    "First-order unification. Following "

    def __init__(self):
        self._state = {}
        self._distinct = {}

    @staticmethod
    def isvar(x):
        "A variable is a string that starts with an uppercase letter."
        return isinstance(x, str) and x and x[0].isupper()

    @staticmethod
    def isatom(x):
        "An atom is a string that does not start with an uppercase letter."
        return isinstance(x, str) and x and not x[0].isupper()

    @staticmethod
    def isapp(x):
        "A function application."
        return isinstance(x, tuple) and x

    def _occurs(self, x, y):
        "Return True if the variable x occurs in the expression y."

        if not self.isvar(x):
            raise TypeError(f"Expected var got {x!r}")

        if self.isvar(y):
            return x == y

        if self.isatom(y):
            return False

        if self.isapp(y):
            return any(self._occurs(x, b) for b in y)

        raise TypeError(f"Expected expr got {y!r}")

    def find(self, x):
        "Replace all variables with their representative values."

        if self.isatom(x):
            return x

        if self.isapp(x):
            return tuple(self.find(a) for a in x)

        if not self.isvar(x):
            raise TypeError(f"Expected var got {x!r}")

        y = self._state.get(x)
        if y is None:
            return x

        y = self.find(y)
        self._state[x] = y
        return y

    def _union(self, x, y):
        "Assign a value y to a variable x."

        if not self.isvar(x):
            raise TypeError(f"Expected var got {x!r}")

        self._state[x] = y

    def canonical(self, x):
        "Promote a variable to be the representative of the class."

        if self.isatom(x):
            return

        if self.isapp(x):
            for a in x:
                self.canonical(a)
            return

        if self.isvar(x):
            a = self.find(x)

            if not self.isvar(a) or x == a:
                return

            self._state[a] = x
            del self._state[x]

            return

        raise TypeError(f"Expected expr got {x!r}")

    def _unify(self, x, y):
        "Unify x and y"

        x = self.find(x)
        y = self.find(y)

        if x == y:
            return

        if self.isvar(x):
            if self._occurs(x, y):
                raise UnifyError(f"Occurs check failed {x!r} in {y!r}")
            self._union(x, y)
            return

        if self.isvar(y):
            if self._occurs(y, x):  # pylint: disable=arguments-out-of-order
                raise UnifyError(f"Occurs check failed {y!r} in {x!r}")
            self._union(y, x)  # pylint: disable=arguments-out-of-order
            return

        if not self.isapp(x) or not self.isapp(y) or len(x) != len(y):
            raise UnifyError(f"Failed to unify {x!r} and {y!r}")

        for a, b in zip(x, y):
            self.unify(a, b)

    def unify(self, x, y):
        """
        Unify two expressions x and y. If successful, add consistent assignment
        of variables to the internal state that can be queried with find.

        Function names must be atoms.

        Raise UnifyError if the expressions are inconsistent.
        Raise TypeError if x or y is not an expression.
        """

        # Save the substitution state in case unification fails.
        saved = dict(self._state)
        try:
            self._unify(x, y)
        except Exception:
            self._state = saved
            raise


if __name__ == "__main__":

    c = ("exists", "a", ("not", ("=", "a", "a")))

    # Axiom
    # This unifies, but it probably should not.
    # Need a way to encode that X is bound by exists.
    # Is there a sane way to teach the unifier about bound variables?
    # Or should I leave variable bounds to the proof context?
    u = Unify()
    u.unify(("exists", "X", ("not", ("=", "X", "Y"))), c)
    print(u._state)

    # I could teach the unifier about lambda terms.
    # But what if I want to do a beta-abstraction or beta-reduction?
    # That I don't think could be done with a unify rewrite rule? Or could it?
    # Typical logical systems build up statements from the bottom.
    # Lambda calculus can allow rewrites at any place in the expression.
    # So do I need that? Maybe not.

    # If the unifier knew about lambda expressions, I could rewrite exists as
    # the (somewhat) wordy

    # This lambda binds X for the context of the inner statement.
    # The bound of X has scope. I can substitute Y for a different X.
    # Or I can teach the unifier to refuse to preform that substitution.
    # Refusing is probably the better idea.
    c = ("exists", ("lambda", "X", ("not", ("=", "X", "Y"))))

    # What if I say this? Then I can infer ("p", "z")
    c = ("forall", ("lambda", "X", ("p", "X")))
    # ("forall", ("lambda", "X", P))
    # ("lambda" "X" P) Y
    # Y is intentionally free.

    # Is there a way to force all variables to be bound in some sense?
    # That would also allow us to erase the bound/free distinction.
    # Meta-math seems to make all variables be free. (with distinctness proviso)

    # I could teach the unifier about lambda and always require bound variables
    # to be distinct from free variables.

    c = ("=", "a", "a")

    # Axiom
    u = Unify()
    u.unify(("=", "X", "X"), c)
    print(u._state)

    # Some Assume we have proof of a and a->b
    h1 = ("implies", "a", "b")
    h2 = "a"
    c = "b"

    ### This is the rule for modus pones
    u = Unify()
    u.unify(("implies", "X", "Y"), h1)
    u.unify("X", h2)
    u.unify("Y", c)
    print(u._state)
