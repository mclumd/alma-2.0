if(bel(Agent, quote(if(and(fire(X), wet(X)), smoke(X)))),
   foo(Agent)).
fif(bel(Agent, quote(if(and(fire(X), wet(X)), smoke(X)))),
   conclusion(foo(Agent))).

bel(alice1, quote(if(and(fire(X), wet(X)), smoke(X)))).
bel(alice2, quote(if(and(fire(X), wet(Y)), smoke(X)))).
