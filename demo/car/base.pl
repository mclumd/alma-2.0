% Knowledge of is_a producing inference
fif(and(rel(is_a, A, B), rel(A, X)), rel(B, X)).

% Observation rule
fif(obs(X), true(X)).

% Contradiction response for when both contradictands are inferred, ancestors have is-a relationship, and more specific case is negative
% Abnormality case is thus when object *does not* have Pred apply to it; and likewise distrust abnormality when the predicate *does* apply
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(rel(Kind, Obj),
    and(ancestor(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T),
    and(rel(is_a, Kind_spec, Kind),
    ancestor(quote(rel(`Kind_spec, `Obj)), quote(not(rel(`Pred, `Obj))), T))))),
and(update(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), rel(`Pred, Obj)))),
and(reinstate(quote(not(rel(`Pred, `Obj))), T),
and(fif(and(rel(Kind, Ab_Obj), not(rel(Pred, Ab_Obj))), abnormal(Ab_Obj, Kind, Pred)),
and(fif(and(abnormal(Ab, Kind, Pred), rel(Pred, Ab)), distrust(quote(abnormal(`Ab, `Kind, `Pred)))),
abnormal(Obj, Kind, Pred)))))).

% Contradiction response for when both contradictands are inferred, ancestors have is-a relationship, and more specific case is negative
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(rel(Kind, Obj),
    and(ancestor(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T),
    and(rel(is_a, Kind_spec, Kind),
    ancestor(quote(rel(`Kind_spec, `Obj)), quote(rel(`Pred, `Obj)), T))))),
and(update(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), not(rel(`Pred, Obj))))),
and(reinstate(quote(rel(`Pred, `Obj)), T),
and(fif(and(rel(Kind, Ab_Obj), rel(Pred, Ab_Obj)), abnormal(Ab_Obj, Kind, Pred)),
and(fif(and(abnormal(Ab, Kind, Pred), not(rel(Pred, Ab))), distrust(quote(abnormal(`Ab, `Kind, `Pred)))),
abnormal(Obj, Kind, Pred)))))).


% Formula for resolving a class of contradiction without hierarchy, or observation
% When we have a contradiction between contradictands obtained from rules of the form Kind /\ Prop --> Pred, and when one contradictand has ancestor A /\ B --> C while we also know A /\ ~B --> ~C, and we know another derivation for C, then weaken the latter's premises into a default and reinstate C
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(rel(Kind_a, Obj),
    and(not(rel(Prop_a, Obj)),
    and(ancestor(quote(fif(and(rel(`Kind_a, Obj), not(rel(`Prop_a, Obj))), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T),
    and(pos_int(quote(fif(and(rel(`Kind_a, Obj), rel(`Prop_a, Obj)), rel(`Pred, Obj)))),
    and(rel(Kind_b, Obj),
    and(rel(Prop_b, Obj),
    and(ancestor(quote(fif(and(rel(`Kind_b, Obj), rel(`Prop_b, Obj)), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T),
    not_equal(quote(fif(and(rel(`Kind_a, Obj), rel(`Prop_a, Obj)), rel(`Pred, Obj))), quote(fif(and(rel(`Kind_b, Obj), rel(`Prop_b, Obj)), rel(`Pred, Obj)))))))))))),
and(update(quote(fif(and(rel(`Kind_a, Obj), rel(`Prop_a, Obj)), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind_a, Obj), and(rel(`Prop_a, Obj), neg_int(quote(abnormal(`Obj, ``Kind_a, ``Pred))))), not(rel(`Pred, Obj))))),
and(reinstate(quote(rel(`Pred, `Obj)), T),
and(fif(and(rel(Kind_a, Ab_Obj), and(not(rel(Prop_a, Ab_Obj)), rel(Pred, Ab_Obj))), abnormal(Ab_Obj, Kind_a, Pred)),
fif(and(abnormal(Ab, Kind_a, Pred), not(rel(Pred, Ab))), distrust(quote(abnormal(`Ab, `Kind_a, `Pred)))))))).
