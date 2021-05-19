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
% Abnormality case is thus when object *does not* have Pred apply to it; and likewise distrust abnormality when the predicate *does* apply
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(rel(Kind1, Obj),
    and(ancestor(quote(fif(rel(`Kind1, Obj), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T),
    and(rel(Kind2, Obj),
    and(ancestor(quote(fif(rel(`Kind2, Obj), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T),
    rel(is_a, Kind1, Kind2)))))),
and(update(quote(fif(rel(`Kind2, Obj), rel(`Pred, Obj))), quote(fif(and(rel(`Kind2, Obj), neg_int(quote(abnormal(`Obj, ``Kind2, ``Pred)))), rel(`Pred, Obj)))),
and(abnormal(Obj, Kind2, Pred),
and(reinstate(quote(not(rel(`Pred, `Obj))), T),
and(fif(and(rel(Kind2, Ab_Obj), not(rel(Pred, Ab_Obj))), abnormal(Ab_Obj, Kind2, Pred)),
fif(and(abnormal(Ab, Kind2, Pred), rel(Pred, Ab)), distrust(quote(abnormal(`Ab, `Kind2, `Pred))))))))).

% Contradiction response for when both contradictands are inferred, ancestors have is-a relationship, and more specific case is negative
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(rel(Kind1, Obj),
    and(ancestor(quote(fif(rel(`Kind1, Obj), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T),
    and(rel(Kind2, Obj),
    and(ancestor(quote(fif(rel(`Kind2, Obj), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T),
    rel(is_a, Kind1, Kind2)))))),
and(update(quote(fif(rel(`Kind2, Obj), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind2, Obj), neg_int(quote(abnormal(`Obj, ``Kind2, ``Pred)))), not(rel(`Pred, Obj))))),
and(abnormal(Obj, Kind2, Pred),
and(reinstate(quote(rel(`Pred, `Obj)), T),
and(fif(and(rel(Kind2, Ab_Obj), rel(Pred, Ab_Obj)), abnormal(Ab_Obj, Kind2, Pred)),
fif(and(abnormal(Ab, Kind2, Pred), not(rel(Pred, Ab))), distrust(quote(abnormal(`Ab, `Kind2, `Pred))))))))).


% Contradiction response for when negative contradictand was observed, and positive isn't derived from default
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(obs(quote(not(rel(`Pred, `Obj)))),
    and(rel(Kind, Obj),
    ancestor(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(rel(`Pred, `Obj)), T)))),
and(update(quote(fif(rel(`Kind, Obj), rel(`Pred, Obj))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), rel(`Pred, Obj)))),
and(abnormal(Obj, Kind, Pred),
and(reinstate(quote(not(rel(`Pred, `Obj))), T),
and(fif(and(rel(Kind, Ab_Obj), not(rel(Pred, Ab_Obj))), abnormal(Ab_Obj, Kind, Pred)),
fif(and(abnormal(Ab, Kind, Pred), rel(Pred, Ab)), distrust(quote(abnormal(`Ab, `Kind, `Pred))))))))).

% Contradiction response for when positive contradictand was observed, and negative isn't derived from default
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(obs(quote(rel(`Pred, `Obj))),
    and(rel(Kind, Obj),
    ancestor(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(not(rel(`Pred, `Obj))), T)))),
and(update(quote(fif(rel(`Kind, Obj), not(rel(`Pred, Obj)))), quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), not(rel(`Pred, Obj))))),
and(abnormal(Obj, Kind, Pred),
and(reinstate(quote(rel(`Pred, `Obj)), T),
and(fif(and(rel(Kind, Ab_Obj), rel(Pred, Ab_Obj)), abnormal(Ab_Obj, Kind, Pred)),
fif(and(abnormal(Ab, Kind, Pred), not(rel(Pred, Ab))), distrust(quote(abnormal(`Ab, `Kind, `Pred))))))))).


% Given contra and distrusted abnormality, the object being a more specific Kind_spec than that abnormality's Kind, and lack of distrusted abnormality for Kind_spec, we should reinstate the contradictand related to Kind_spec
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(distrusted(quote(abnormal(`Obj, `Kind, `Pred)), T),
    and(rel(is_a, Kind_spec, Kind),
    and(rel(Kind_spec, Obj),
    and(neg_int_spec(quote(distrusted(quote(abnormal(``Obj, ``Kind_spec, ``Pred)), `T))),
    pos_int(quote(fif(and(rel(`Kind_spec, Obj), neg_int(quote(abnormal(`Obj, ``Kind_spec, ``Pred)))), not(rel(`Pred, Obj)))))))))),
reinstate(quote(not(rel(`Pred, `Obj))), T)).

fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(distrusted(quote(abnormal(`Obj, `Kind, `Pred)), T),
    and(rel(is_a, Kind_spec, Kind),
    and(rel(Kind_spec, Obj),
    and(neg_int_spec(quote(distrusted(quote(abnormal(``Obj, ``Kind_spec, ``Pred)), `T))),
    pos_int(quote(fif(and(rel(`Kind_spec, Obj), neg_int(quote(abnormal(`Obj, ``Kind_spec, ``Pred)))), rel(`Pred, Obj))))))))),
reinstate(quote(rel(`Pred, `Obj)), T)).

% Versions for Kind_spec related to contradictand by non-default rule
fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(distrusted(quote(abnormal(`Obj, `Kind, `Pred)), T),
    and(rel(is_a, Kind_spec, Kind),
    and(rel(Kind_spec, Obj),
    and(neg_int_spec(quote(distrusted(quote(abnormal(``Obj, ``Kind_spec, ``Pred)), `T))),
    pos_int(quote(fif(rel(`Kind_spec, Obj), not(rel(`Pred, Obj)))))))))),
reinstate(quote(not(rel(`Pred, `Obj))), T)).

fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
    and(distrusted(quote(abnormal(`Obj, `Kind, `Pred)), T),
    and(rel(is_a, Kind_spec, Kind),
    and(rel(Kind_spec, Obj),
    and(neg_int_spec(quote(distrusted(quote(abnormal(``Obj, ``Kind_spec, ``Pred)), `T))),
    pos_int(quote(fif(rel(`Kind_spec, Obj), rel(`Pred, Obj))))))))),
reinstate(quote(rel(`Pred, `Obj)), T)).

% Given abnormality and a default where negative comes from lack of abnormality, reinstate positive contradictand
%fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
%    and(abnormal(Obj, Kind, Pred),
%    pos_int(quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), not(rel(`Pred, Obj))))))),
%reinstate(quote(rel(`Pred, `Obj)), T)).

% Given abnormality and a default positive comes from lack of abnormality, reinstate negative contradictand
%fif(and(contradicting(quote(rel(`Pred, `Obj)), quote(not(rel(`Pred, `Obj))), T),
%    and(abnormal(Obj, Kind, Pred),
%    pos_int(quote(fif(and(rel(`Kind, Obj), neg_int(quote(abnormal(`Obj, ``Kind, ``Pred)))), rel(`Pred, Obj)))))),
%reinstate(quote(not(rel(`Pred, `Obj))), T)).


% Attempts to reinstate abnormality in general cases
% (Does this do anything?? given re-deriving after reinstate)
%fif(and(distrusted(quote(abnormal(`Obj, `Kind, `Pred)), T),
%    and(rel(Pred, Obj),
%    ancestor(quote(rel(`Pred, `Obj)), quote(abnormal(`Obj, `Kind, `Pred)), T))),
%reinstate(quote(abnormal(`Obj, `Kind, `Pred)), T)).

%fif(and(distrusted(quote(abnormal(`Obj, `Kind, `Pred)), T),
%    and(not(rel(Pred, Obj)),
%    ancestor(quote(not(rel(`Pred, `Obj))), quote(abnormal(`Obj, `Kind, `Pred)), T))),
%reinstate(quote(abnormal(`Obj, `Kind, `Pred)), T)).

