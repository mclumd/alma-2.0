% Knowledge of is_a transitivity and is_a producing inference
fif(and(rel(is_a, X, Y), rel(is_a, Y, Z)), rel(is_a, X, Z)).
fif(and(rel(is_a, A, B), rel(A, X)), rel(B, X)).

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
fif(and(rel(Kind, Ab_Obj), not(rel(Pred, Ab_Obj))), abnormal(Ab_Obj, Kind, quote(not(rel(`Pred, `Ab_Obj))))))).

% Contradiction response for when both contradictands are inferred, ancestors have is-a relationship, and more specific case is negative
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(rel(Kind, Obj),
    and(parent(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T),
    and(rel(is_a, Kind_spec, Kind),
    parent(quote(rel(`Kind_spec, `Obj)), quote(rel(`Pred, `Obj)), T))))),
and(update(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, quote(rel(```Pred, ``Obj)))))), not(rel(`Pred, Obj))))),
fif(and(rel(Kind, Ab_Obj), rel(Pred, Ab_Obj)), abnormal(Ab_Obj, Kind, quote(rel(`Pred, `Ab_Obj)))))).


% Contradiction response for when negative contradictand was observed, and positive isn't derived from default
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(obs(quote(not(rel(`Pred, `Obj)))),
    and(rel(Kind, Obj),
    parent(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T)))),
and(update(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, quote(not(rel(```Pred, ``Obj))))))), rel(`Pred, Obj)))),
fif(and(rel(Kind, Ab_Obj), not(rel(Pred, Ab_Obj))), abnormal(Ab_Obj, Kind, quote(not(rel(`Pred, `Ab_Obj))))))).

% Contradiction response for when positive contradictand was observed, and negative isn't derived from default
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(obs(quote(rel(`Pred, `Obj))),
    and(rel(Kind, Obj),
    parent(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T)))),
and(update(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, quote(rel(```Pred, ``Obj)))))), not(rel(`Pred, Obj))))),
fif(and(rel(Kind, Ab_Obj), rel(Pred, Ab_Obj)), abnormal(Ab_Obj, Kind, quote(rel(`Pred, `Ab_Obj)))))).


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


% Formula for resolving a class of contradiction without hierarchy, or observation
% When we have a contradiction between contradictands obtained from rules of the form Kind /\ Prop --> Pred, and when one contradictand has ancestor A /\ B --> C while we also know A /\ ~B --> ~C, and we know another derivation for C, then weaken the latter's premises into a default and reinstate C
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(rel(Kind_a, Obj),
    and(not(rel(Prop_a, Obj)),
    and(parent(quote(fif(and(rel(`Kind_a, Obj), not(rel(`Prop_a, Obj))), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T),
    and(pos_int(quote(fif(and(rel(`Kind_a, Obj), rel(`Prop_a, Obj)), rel(`Pred, Obj)))),
    and(rel(Kind_b, Obj),
    and(rel(Prop_b, Obj),
    and(parent(quote(fif(and(rel(`Kind_b, Obj), rel(`Prop_b, Obj)), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T),
    not_equal(quote(fif(and(rel(`Kind_a, Obj), rel(`Prop_a, Obj)), rel(`Pred, Obj))), quote(fif(and(rel(`Kind_b, Obj), rel(`Prop_b, Obj)), rel(`Pred, Obj)))))))))))),
and(update(quote(fif(and(rel(`Kind_a, Obj), not(rel(`Prop_a, Obj))), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind_a, Obj), and(not(rel(`Prop_a, Obj)), neg_int(quote(abnormal(`Obj, ``Kind_a, quote(rel(```Pred, ``Obj))))))), not(rel(`Pred, Obj))))),
and(reinstate(quote(rel(`Pred, `Obj)), T),
fif(and(rel(Kind_a, Ab_Obj), and(not(rel(Prop_a, Ab_Obj)), rel(Pred, Ab_Obj))), abnormal(Ab_Obj, Kind_a, quote(rel(`Pred, `Ab_Obj))))))).

fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(rel(Kind_a, Obj),
    and(rel(Prop_a, Obj),
    and(parent(quote(fif(and(rel(`Kind_a, Obj), rel(`Prop_a, Obj)), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T),
    and(pos_int(quote(fif(and(rel(`Kind_a, Obj), not(rel(`Prop_a, Obj))), not(rel(`Pred, Obj))))),
    and(rel(Kind_b, Obj),
    and(not(rel(Prop_b, Obj)),
    and(parent(quote(fif(and(rel(`Kind_b, Obj), not(rel(`Prop_b, Obj))), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T),
    not_equal(quote(fif(and(rel(`Kind_a, Obj), not(rel(`Prop_a, Obj))), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind_b, Obj), not(rel(`Prop_b, Obj))), not(rel(`Pred, Obj))))))))))))),
and(update(quote(fif(and(rel(`Kind_a, Obj), rel(`Prop_a, Obj)), rel(`Pred, Obj))), quote(fif(and(rel(`Kind_a, Obj), and(rel(`Prop_a, Obj), neg_int(quote(abnormal(`Obj, ``Kind_a, quote(not(rel(```Pred, ``Obj)))))))), rel(`Pred, Obj)))),
and(reinstate(quote(not(rel(`Pred, `Obj))), T),
fif(and(rel(Kind_a, Ab_Obj), and(rel(Prop_a, Ab_Obj), not(rel(Pred, Ab_Obj)))), abnormal(Ab_Obj, Kind_a, quote(not(rel(`Pred, `Ab_Obj)))))))).


% Formulas for resolving contradictions where one contradictand is descended from a default, and the other is not

% ~P from defaults and P not from defaults; reinstate P
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(parents_defaults(quote(not(rel(`Pred, `Obj))), T),
    parent_non_default(quote(rel(`Pred, `Obj)), T))),
reinstate(quote(rel(`Pred, `Obj)), T)).

% P from defaults and ~P not from defaults; reinstate ~P
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(parents_defaults(quote(rel(`Pred, `Obj)), T),
    parent_non_default(quote(not(rel(`Pred, `Obj))), T))),
reinstate(quote(not(rel(`Pred, `Obj))), T)).


% Formulas for resolving contradictions between reinstatements
% When we find a reinstatement was derived from is-a ontology, reinstate the other
fif(and(contradicting(quote(reinstate(`X, `Time)), quote(reinstate(`Y, `Time)), T),
    parent(quote(rel(is_a, `Kind, `Kind_gen)), quote(reinstate(`X, `Time)), T)),
reinstate(quote(reinstate(`Y, `Time)), T)).

fif(and(contradicting(quote(reinstate(`X, `Time)), quote(reinstate(`Y, `Time)), T),
    parent(quote(rel(is_a, `Kind, `Kind_gen)), quote(reinstate(`Y, `Time)), T)),
reinstate(quote(reinstate(`X, `Time)), T)).
