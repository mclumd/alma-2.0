if(bel(Agent, quote(if(and(fire(A), wet(A)), smoke(A)))),
   foo(Agent)).
fif(bel(Agent, quote(if(and(fire(A), wet(A)), smoke(A)))),
   foo(Agent)).
if(bel(Agent, quote(if(and(fire(A), wet(B)), smoke(B)))),
   foo(Agent)).

bel(agent_1, quote(if(and(fire(X), wet(X)), smoke(X)))).
bel(agent_2, quote(if(and(fire(X), wet(Y)), smoke(X)))).
bel(agent_3, quote(if(and(fire(X), wet(X)), smoke(Y)))).

if(bel2(Agent, quote(bel(alma, quote(if(and(fire(A), wet(A)), smoke(A)))))),
   foo(Agent)).
fif(bel2(Agent, quote(bel(alma, quote(if(and(fire(A), wet(A)), smoke(A)))))),
   foo(Agent)).
if(bel2(Agent, quote(bel(alma, quote(if(and(fire(A), wet(B)), smoke(B)))))),
   foo(Agent)).

bel2(agent_4, quote(bel(alma, quote(if(and(fire(X), wet(X)), smoke(X)))))).
bel2(agent_5, quote(bel(alma, quote(if(and(fire(X), wet(Y)), smoke(X)))))).
bel2(agent_6, quote(bel(alma, quote(if(and(fire(X), wet(X)), smoke(Y)))))).
