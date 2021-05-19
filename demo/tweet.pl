penguin(tweety).
if(penguin(X), bird(X)).

fif(bird(X), fly(X)).
fif(penguin(X), not(fly(X))).

prefer(quote(fif(penguin(X), not(fly(X)))), quote(fif(bird(X), fly(X)))).

fif(and(prefer(Rx, Ry),
    and(contradicting(X, Y, T),
    and(ancestor(Rx, X, T, bound(Rx, X)),
    ancestor(Ry, Y, T, bound(Ry, Y))))),
  reinstate(X, T)).
fif(and(prefer(Rx, Ry),
    and(contradicting(Y, X, T),
    and(ancestor(Ry, Y, T, bound(Ry, Y)),
    ancestor(Rx, X, T, bound(Rx, X))))),
  reinstate(X, T)).
