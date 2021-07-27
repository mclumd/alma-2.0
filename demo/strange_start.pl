a.
not(a).
b.
distrust(quote(b)).
c.
update(quote(c), quote(e)).
true(quote(d)).
reinstate(quote(not(e)), 0).
reinstate(quote(e), 0).

fif(contradicting(quote(reinstate(`X, `T)), quote(reinstate(`Y, `T)), T),
reinstate(quote(reinstate(`X, `T)), T)).
