if(and(bel(john, quote(cause(quote(fire(``Loc)), quote(flammable(``Material, ``Loc))))), nearby(john, Loc)),
removes(john, Material, Loc)).
fif(and(bel(john, quote(cause(quote(fire(``Loc)), quote(flammable(``Material, ``Loc))))), nearby(john, Loc)),
removes(john, Material, Loc)).

bel(john, quote(cause(quote(fire(room(``ID))), quote(flammable(worn_wiring, room(``ID)))))).
bel(john, quote(cause(quote(fire(room(`ID))), quote(flammable(worn_wiring, room(`ID)))))).

nearby(john, room(closet_b)).

if(bel(Agent, X), true(X)).
