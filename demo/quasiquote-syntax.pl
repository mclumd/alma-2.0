bel(john, quote(if(and(heard(quote(on_fire(`X))), smoke(X)), on_fire(X)))).
bel(Agent, quote(bel(alma, quote(if(and(fire(`A), wet(`A)), smoke(`A)))))).
bel(Agent, quote(bel(alma, quote(if(and(fire(`A), wet(``````A)), smoke(`A)))))).
bel(```X).

fif(and(heard(quote(on_fire(`X))), smoke(X)), conclusion(on_fire(X))).

fif(and(heard(quote(on_fire(`Y))), smoke(Y)), conclusion(on_fire(Y))).
fif(and(heard(quote(on_fire(``Y))), smoke(Y)), conclusion(on_fire(Y))).
fif(and(heard(quote(on_fire(`Y))), smoke(Y)), conclusion(on_fire(`````Y))).

fif(and(heard(quote(on_fire(Y))), smoke(Y)), conclusion(on_fire(Y))).

if(and(bel(Agent_a, quote(color(sky, X))), bel(Agent_b, quote(color(sky, X)))), probably(quote(color(sky, X)))).
if(and(bel(Agent_a, quote(color(sky, `X))), bel(Agent_b, quote(color(sky, `X)))), probably(quote(color(sky, X)))).
if(and(bel(Agent_a, quote(color(sky, `X))), bel(Agent_b, quote(color(sky, `X)))), probably(quote(color(sky, `X)))).

if(and(foo, and(bel(Agent_a, quote(color(sky, `X))), bel(Agent_b, quote(color(sky, `X))))), probably2(quote(color(sky, X)))).
