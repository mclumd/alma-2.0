%fif(and(heard(quote(on_fire(`X))), smoke(X)), conclusion(on_fire(X))).
if(and(heard(quote(on_fire(`X))), smoke(X)), on_fire(X)).
heard(quote(on_fire(site1))).
smoke(site1).
