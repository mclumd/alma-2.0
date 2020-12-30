if(bel(Agent, quote(if(and(fire(A), wet(A)), smoke(A)))),
   foo(Agent)).
fif(bel(Agent, quote(if(and(fire(A), wet(A)), smoke(A)))),
   conclusion(foo(Agent))).
if(bel(Agent, quote(if(and(fire(A), wet(B)), smoke(B)))),
   foo(Agent)).

bel(alice1, quote(if(and(fire(X), wet(X)), smoke(X)))).
bel(alice2, quote(if(and(fire(X), wet(Y)), smoke(X)))).
bel(alice2, quote(if(and(fire(X), wet(X)), smoke(Y)))).
