rocket_penguin(tweety).
%penguin(tweety).
if(penguin(X), bird(X)).
if(rocket_penguin(X), penguin(X)).

fif(bird(X), fly(X)).
fif(penguin(X), not(fly(X))).
fif(rocket_penguin(X), fly(X)).

prefer(quote(fif(penguin(X), not(fly(X)))), quote(fif(bird(X), fly(X)))).
prefer(quote(fif(rocket_penguin(X), fly(X))), quote(fif(penguin(X), not(fly(X))))).

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

% Can we relate our preference to contra, so that we have fewer fif cases to expand?
% If we use a common variable for prefer arg conclusion and contra arg, we need to match the rest of preferred formula's structure...
