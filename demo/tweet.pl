penguin(tweety).
if(penguin(X), bird(X)).

fif(bird(X), conclusion(fly(X))).
fif(penguin(X), conclusion(not(fly(X)))).

prefer(quote(fif(penguin(X), conclusion(not(fly(X))))), quote(fif(bird(X), conclusion(fly(X))))).

fif(and(prefer(Rx, Ry),
    and(contra(X, Y, T),
    and(proc(idx_to_form(X, Fx), bound(X)),
    and(proc(ancestor(Rx, Fx, T), bound(Rx, Fx)),
    and(proc(idx_to_form(Y, Fy), bound(Y)),
    proc(ancestor(Ry, Fy, T), bound(Ry, Fy))))))),
  conclusion(reinstate(Fx, T))).
fif(and(prefer(Rx, Ry),
    and(contra(Y, X, T),
    and(proc(idx_to_form(Y, Fy), bound(Y)),
    and(proc(ancestor(Ry, Fy, T), bound(Ry, Fy)),
    and(proc(idx_to_form(X, Fx), bound(X)),
    proc(ancestor(Rx, Fx, T), bound(Rx, Fx))))))),
  conclusion(reinstate(Fx, T))).
