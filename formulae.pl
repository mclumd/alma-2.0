fif(and(if(not(t(A)), t(B)), atom), conclusion(thing)).

if(and(if(not(t(A)), t(B)), atom), thing).

if(a,b).

if(contra(Y, X, Z), parentsof(Y, Y)).
if(contra(Y, X, Z), parentsof(X, X)).

fif(penguin(X), conclusion(not(fly(X)))).

fif(and(inFridge(X),
    hasFreshness(X)),
  conclusion(fresh(X))).

immediateposlit.

