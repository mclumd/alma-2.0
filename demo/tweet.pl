penguin(tweety).
if(penguin(X), bird(X)).

fif(bird(X), conclusion(fly(X))).
fif(penguin(X), conclusion(not(fly(X)))).

prefer(quote(fif(penguin(X), conclusion(not(fly(X))))), quote(fif(bird(X), conclusion(fly(X))))).

fif(and(prefer(Rx, Ry),
    and(contra(X, Y, T),
    and(proc(ancestor(Rx, X, T), bound(Rx, X)),
    proc(ancestor(Ry, Y, T), bound(Ry, Y))))),
  conclusion(reinstate(X, T))).
fif(and(prefer(Rx, Ry),
    and(contra(Y, X, T),
    and(proc(ancestor(Ry, Y, T), bound(Ry, Y)),
    proc(ancestor(Rx, X, T), bound(Rx, X))))),
  conclusion(reinstate(X, T))).
