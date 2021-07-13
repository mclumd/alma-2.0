% Knowledge of is_a transitivity and is_a producing inference
fif(and(rel(is_a, X, Y), rel(is_a, Y, Z)), rel(is_a, X, Z)).
fif(and(rel(is_a, A, B), rel(A, X)), rel(B, X)).

% Ontology of mollusk types
rel(is_a, cephalopod, mollusk).
rel(is_a, nautilus, cephalopod).
rel(is_a, naked_nautilus, nautilus).

% Knowledge of shell_bearer status for creature categories
fif(rel(mollusk, X), rel(shell_bearer, X)).
fif(rel(cephalopod, X), not(rel(shell_bearer, X))).
fif(rel(nautilus, X), rel(shell_bearer, X)).
fif(rel(naked_nautilus, X), not(rel(shell_bearer, X))).

% Observation rule
fif(obs(X), true(X)).

% The reason for an object's abnormality is true
fif(abnormal(Obj, Kind, Reason), true(Reason)).

% Contradiction response for when both contradictands are inferred, ancestors have is-a relationship, and more specific case is negative
% Abnormality case is thus when object *does not* have Pred apply to it; and likewise distrust abnormality when the predicate *does* apply
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(rel(Kind, Obj),
    and(parent(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T),
    and(rel(is_a, Kind_spec, Kind),
    parent(quote(rel(`Kind_spec, `Obj)), quote(not(rel(`Pred, `Obj))), T))))),
and(update(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, quote(not(rel(```Pred, ``Obj))))))), rel(`Pred, Obj)))),
and(reinstate(quote(not(rel(`Pred, `Obj))), T),
fif(and(rel(Kind, Ab_Obj), not(rel(Pred, Ab_Obj))), abnormal(Ab_Obj, Kind, quote(not(rel(`Pred, `Ab_Obj)))))))).

% Contradiction response for when both contradictands are inferred, ancestors have is-a relationship, and more specific case is negative
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(rel(Kind, Obj),
    and(parent(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T),
    and(rel(is_a, Kind_spec, Kind),
    parent(quote(rel(`Kind_spec, `Obj)), quote(rel(`Pred, `Obj)), T))))),
and(update(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, quote(rel(```Pred, ``Obj)))))), not(rel(`Pred, Obj))))),
and(reinstate(quote(rel(`Pred, `Obj)), T),
fif(and(rel(Kind, Ab_Obj), rel(Pred, Ab_Obj)), abnormal(Ab_Obj, Kind, quote(rel(`Pred, `Ab_Obj))))))).


% Contradiction response for when negative contradictand was observed, and positive isn't derived from default
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(obs(quote(not(rel(`Pred, `Obj)))),
    and(rel(Kind, Obj),
    parent(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T)))),
and(update(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, quote(not(rel(```Pred, ``Obj))))))), rel(`Pred, Obj)))),
and(reinstate(quote(not(rel(`Pred, `Obj))), T),
fif(and(rel(Kind, Ab_Obj), not(rel(Pred, Ab_Obj))), abnormal(Ab_Obj, Kind, quote(not(rel(`Pred, `Ab_Obj)))))))).

% Contradiction response for when positive contradictand was observed, and negative isn't derived from default
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(obs(quote(rel(`Pred, `Obj))),
    and(rel(Kind, Obj),
    parent(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T)))),
and(update(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, quote(rel(```Pred, ``Obj)))))), not(rel(`Pred, Obj))))),
and(reinstate(quote(rel(`Pred, `Obj)), T),
fif(and(rel(Kind, Ab_Obj), rel(Pred, Ab_Obj)), abnormal(Ab_Obj, Kind, quote(rel(`Pred, `Ab_Obj))))))).


% Reinstatement of narrowest ontology category, as found by the lack of a more-specific is-a instance to be the parent
% Also checks that other contradictand was derived from more general case
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(neg_int(quote(obs(quote(rel(``Pred, ``Obj))))),
    and(rel(Kind, Obj),
    and(rel(is_a, Kind, Kind_gen),
    and(parent(quote(rel(`Kind, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(parent(quote(rel(`Kind_gen, `Obj)), quote(rel(`Pred, `Obj)), T),
    non_ancestor(quote(rel(is_a, `Kind_spec, `Kind)), quote(rel(`Kind, `Obj)), T))))))),
reinstate(quote(not(rel(`Pred, `Obj))), T)).

fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(neg_int(quote(obs(quote(not(rel(``Pred, ``Obj)))))),
    and(rel(Kind, Obj),
    and(rel(is_a, Kind, Kind_gen),
    and(parent(quote(rel(`Kind, `Obj)), quote(rel(`Pred, `Obj)), T),
    and(parent(quote(rel(`Kind_gen, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    non_ancestor(quote(rel(is_a, `Kind_spec, `Kind)), quote(rel(`Kind, `Obj)), T))))))),
reinstate(quote(rel(`Pred, `Obj)), T)).


% Contradiction response for when either contradictand was directly observed
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    obs(quote(not(rel(`Pred, `Obj))))),
reinstate(quote(not(rel(`Pred, `Obj))), T)).

fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    obs(quote(rel(`Pred, `Obj)))),
reinstate(quote(rel(`Pred, `Obj)), T)).
