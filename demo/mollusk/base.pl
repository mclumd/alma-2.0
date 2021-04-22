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

% Contradiction response for when both contradictands are inferred, ancestors have is-a relationship, and more specific case is negative
fif(and(contra(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(ancestor(quote(fif(rel(`Kind1, Obj), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T),
    and(ancestor(quote(fif(rel(`Kind2, Obj), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T),
    rel(is_a, Kind1, Kind2)))),
and(update(quote(fif(rel(`Kind2, Obj), rel(`Pred, Obj))), quote(fif(and(rel(`Kind2, Obj), neg_int(quote(abnormal(`Obj, ``Kind2, ``Pred)))), rel(`Pred, Obj)))),
and(abnormal(Obj, Kind2, Pred),
and(reinstate(quote(not(rel(`Pred, `Obj))), T),
fif(and(rel(Kind2, Ab_Obj), not(rel(Pred, Ab_Obj))), abnormal(Ab_Obj, Kind2, Pred)))))).

% Contradiction response for when both contradictands are inferred, ancestors have is-a relationship, and more specific case is negative
fif(and(contra(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(ancestor(quote(fif(rel(`Kind1, Obj), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T),
    and(ancestor(quote(fif(rel(`Kind2, Obj), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T),
    rel(is_a, Kind1, Kind2)))),
and(update(quote(fif(rel(`Kind2, Obj), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind2, Obj), neg_int(quote(abnormal(`Obj, ``Kind2, ``Pred)))), not(rel(`Pred, Obj))))),
and(abnormal(Obj, Kind2, Pred),
and(reinstate(quote(rel(`Pred, `Obj)), T),
fif(and(rel(Kind2, Ab_Obj), rel(Pred, Ab_Obj)), abnormal(Ab_Obj, Kind2, Pred)))))).


% Contradiction response for when negative contradictand was observed, and positive isn't derived from default
fif(and(contra(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(obs(quote(not(rel(`Pred, `Obj)))),
    ancestor(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T))),
and(update(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), rel(`Pred, Obj)))),
and(abnormal(Obj, Kind, Pred),
and(reinstate(quote(not(rel(`Pred, `Obj))), T),
fif(and(rel(Kind, Ab_Obj), not(rel(Pred, Ab_Obj))), abnormal(Ab_Obj, Kind, Pred)))))).

% Contradiction response for when positive contradictand was observed, and negative isn't derived from default
fif(and(contra(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(obs(quote(rel(`Pred, `Obj))),
    ancestor(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T))),
and(update(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), not(rel(`Pred, Obj))))),
and(abnormal(Obj, Kind, Pred),
and(reinstate(quote(rel(`Pred, `Obj)), T),
fif(and(rel(Kind, Ab_Obj), rel(Pred, Ab_Obj)), abnormal(Ab_Obj, Kind, Pred)))))).


% Contradiction response for distrusted abnormality: When Obj's abormality with respect to Pred is inferred from it being Pred, and a default rule for neg_int abnormality was a parent of Obj as Pred, reinstate abnormality
% Essentially, this covers positive case for Pred where default applied too soon, and abnormal was still derivable
fif(and(contra(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(distrusted(quote(abnormal(`Obj, `Kind, `Pred)), T),
    and(ancestor(quote(rel(`Pred, `Obj)), quote(abnormal(`Obj, `Kind, `Pred)), T),
    ancestor(quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T)))),
reinstate(quote(abnormal(`Obj, `Kind, `Pred)), T)).

% Version of the above rule for negative case
fif(and(contra(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(distrusted(quote(abnormal(`Obj, `Kind, `Pred)), T),
    and(ancestor(quote(not(rel(`Pred, `Obj))), quote(abnormal(`Obj, `Kind, `Pred)), T),
    ancestor(quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T)))),
reinstate(quote(abnormal(`Obj, `Kind, `Pred)), T)).


% Given abnormality, reinstate negative contradictand when positive is descended from default requiring lack of that abnormality
fif(and(abnormal(Obj, Kind, Pred),
    and(contra(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    ancestor(quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T))),
reinstate(quote(not(rel(`Pred, `Obj))), T)).

% Given abnormality, reinstate positive contradictand when negative is descended from default requiring lack of that abnormality
fif(and(abnormal(Obj, Kind, Pred),
    and(contra(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    ancestor(quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T))),
reinstate(quote(rel(`Pred, `Obj)), T)).

