% Simple-question answering for cases when ALMA is asked if it believes X, or asked if X is true

% When X is part of a contradiction
% Belief query: no
fif(and(query_belief(X, Asktime),
    and(now(Asktime),
    contradicting(X, Y, T))),
answer(quote(query_belief(`X, `Asktime)), no, reason(quote(contradicting(`X, `Y, `T))))).
fif(and(query_belief(X, Asktime),
    and(now(Asktime),
    contradicting(Y, X, T))),
answer(quote(query_belief(`X, `Asktime)), no, reason(quote(contradicting(`Y, `X, `T))))).

% Truth query: unsure
fif(and(query_truth(X, Asktime),
    and(now(Asktime),
    contradicting(X, Y, T))),
answer(quote(query_truth(`X, `Asktime)), unsure, reason(quote(contradicting(`X, `Y, `T))))).
fif(and(query_truth(X, Asktime),
    and(now(Asktime),
    contradicting(Y, X, T))),
answer(quote(query_truth(`X, `Asktime)), unsure, reason(quote(contradicting(`Y, `X, `T))))).


% When X is a current belief
% Belief query: yes
fif(and(query_belief(X, Asktime),
    and(now(Asktime),
    and(pos_int(X),
    acquired(X, Learnedtime)))),
answer(quote(query_belief(`X, `Asktime)), yes, reason(quote(acquired(`X, `Learnedtime))))).

% Truth query: yes
fif(and(query_truth(X, Asktime),
    and(now(Asktime),
    and(pos_int(X),
    acquired(X, Learnedtime)))),
answer(quote(query_truth(`X, `Asktime)), yes, reason(quote(acquired(`X, `Learnedtime))))).


% When ~X is a current belief, in cases where X is a literal with particular arities
% Belief query: no
% Unary case
fif(and(query_belief(quote(rel(`Pred, `Arg)), Asktime),
    and(now(Asktime),
    and(not(rel(Pred, Arg)),
    acquired(quote(not(rel(`Pred, `Arg))), Learnedtime)))),
answer(quote(query_belief(quote(rel(``Pred, ``Arg)), `Asktime)), no, reason(quote(acquired(quote(not(rel(``Pred, ``Arg))), `Learnedtime))))).
% Binary case
fif(and(query_belief(quote(rel(`Pred, `Arg_a, `Arg_b)), Asktime),
    and(now(Asktime),
    and(not(rel(Pred, Arg_a, Arg_b)),
    acquired(quote(not(rel(`Pred, `Arg_a, `Arg_b))), Learnedtime)))),
answer(quote(query_belief(quote(rel(``Pred, ``Arg_a, ``Arg_b)), `Asktime)), no, reason(quote(acquired(quote(not(rel(``Pred, ``Arg_a, ``Arg_b))), `Learnedtime))))).
% Ternary case
fif(and(query_belief(quote(rel(`Pred, `Arg_a, `Arg_b, `Arg_c)), Asktime),
    and(now(Asktime),
    and(not(rel(Pred, Arg_a, Arg_b, Arg_c)),
    acquired(quote(not(rel(`Pred, `Arg_a, `Arg_b, `Arg_c))), Learnedtime)))),
answer(quote(query_belief(quote(rel(``Pred, ``Arg_a, ``Arg_b, ``Arg_c)), `Asktime)), no, reason(quote(acquired(quote(not(rel(``Pred, ``Arg_a, ``Arg_b, ``Arg_c))), `Learnedtime))))).
% Negation flipped from the above three: queries about negative formula when positive is believed
% Unary case
fif(and(query_belief(quote(not(rel(`Pred, `Arg))), Asktime),
    and(now(Asktime),
    and(rel(Pred, Arg),
    acquired(quote(rel(`Pred, `Arg)), Learnedtime)))),
answer(quote(query_belief(quote(not(rel(``Pred, ``Arg))), `Asktime)), no, reason(quote(acquired(quote(rel(``Pred, ``Arg)), `Learnedtime))))).
% Binary case
fif(and(query_belief(quote(not(rel(`Pred, `Arg_a, `Arg_b))), Asktime),
    and(now(Asktime),
    and(rel(Pred, Arg_a, Arg_b),
    acquired(quote(rel(`Pred, `Arg_a, `Arg_b)), Learnedtime)))),
answer(quote(query_belief(quote(not(rel(``Pred, ``Arg_a, ``Arg_b))), `Asktime)), no, reason(quote(acquired(quote(rel(``Pred, ``Arg_a, ``Arg_b)), `Learnedtime))))).
% Ternary case
fif(and(query_belief(quote(not(rel(`Pred, `Arg_a, `Arg_b, `Arg_c))), Asktime),
    and(now(Asktime),
    and(rel(Pred, Arg_a, Arg_b, Arg_c),
    acquired(quote(rel(`Pred, `Arg_a, `Arg_b, `Arg_c)), Learnedtime)))),
answer(quote(query_belief(quote(not(rel(``Pred, ``Arg_a, ``Arg_b, ``Arg_c))), `Asktime)), no, reason(quote(acquired(quote(rel(``Pred, ``Arg_a, ``Arg_b, ``Arg_c)), `Learnedtime))))).

% Truth query: no
% Unary case
fif(and(query_truth(quote(rel(`Pred, `Arg)), Asktime),
    and(now(Asktime),
    and(not(rel(Pred, Arg)),
    acquired(quote(not(rel(`Pred, `Arg))), Learnedtime)))),
answer(quote(query_truth(quote(rel(``Pred, ``Arg)), `Asktime)), no, reason(quote(acquired(quote(not(rel(``Pred, ``Arg))), `Learnedtime))))).
% Binary case
fif(and(query_truth(quote(rel(`Pred, `Arg_a, `Arg_b)), Asktime),
    and(now(Asktime),
    and(not(rel(Pred, Arg_a, Arg_b)),
    acquired(quote(not(rel(`Pred, `Arg_a, `Arg_b))), Learnedtime)))),
answer(quote(query_truth(quote(rel(``Pred, ``Arg_a, ``Arg_b)), `Asktime)), no, reason(quote(acquired(quote(not(rel(``Pred, ``Arg_a, ``Arg_b))), `Learnedtime))))).
% Ternary case
fif(and(query_truth(quote(rel(`Pred, `Arg_a, `Arg_b, `Arg_c)), Asktime),
    and(now(Asktime),
    and(not(rel(Pred, Arg_a, Arg_b, Arg_c)),
    acquired(quote(not(rel(`Pred, `Arg_a, `Arg_b, `Arg_c))), Learnedtime)))),
answer(quote(query_truth(quote(rel(``Pred, ``Arg_a, ``Arg_b, ``Arg_c)), `Asktime)), no, reason(quote(acquired(quote(not(rel(``Pred, ``Arg_a, ``Arg_b, ``Arg_c))), `Learnedtime))))).
% Negation flipped from the above six: queries about negative formula when positive is believed
% Unary case
fif(and(query_truth(quote(not(rel(`Pred, `Arg))), Asktime),
    and(now(Asktime),
    and(rel(Pred, Arg),
    acquired(quote(rel(`Pred, `Arg)), Learnedtime)))),
answer(quote(query_truth(quote(not(rel(``Pred, ``Arg))), `Asktime)), no, reason(quote(acquired(quote(rel(``Pred, ``Arg)), `Learnedtime))))).
% Binary case
fif(and(query_truth(quote(not(rel(`Pred, `Arg_a, `Arg_b))), Asktime),
    and(now(Asktime),
    and(rel(Pred, Arg_a, Arg_b),
    acquired(quote(rel(`Pred, `Arg_a, `Arg_b)), Learnedtime)))),
answer(quote(query_truth(quote(not(rel(``Pred, ``Arg_a, ``Arg_b))), `Asktime)), no, reason(quote(acquired(quote(rel(``Pred, ``Arg_a, ``Arg_b)), `Learnedtime))))).
% Ternary case
fif(and(query_truth(quote(not(rel(`Pred, `Arg_a, `Arg_b, `Arg_c))), Asktime),
    and(now(Asktime),
    and(rel(Pred, Arg_a, Arg_b, Arg_c),
    acquired(quote(rel(`Pred, `Arg_a, `Arg_b, `Arg_c)), Learnedtime)))),
answer(quote(query_truth(quote(not(rel(``Pred, ``Arg_a, ``Arg_b, ``Arg_c))), `Asktime)), no, reason(quote(acquired(quote(rel(``Pred, ``Arg_a, ``Arg_b, ``Arg_c)), `Learnedtime))))).


% When X isn't a current belief, but was a belief in the past
% Belief query: no
% X became distrusted
fif(and(query_belief(X, Asktime),
    and(now(Asktime),
    and(neg_int(X),
    and(pos_int_past(X, Learnedtime, Endtime),
    distrusted(X, Endtime))))),
answer(quote(query_belief(`X, `Asktime)), no, reason(quote(distrusted(`X, `Endtime))))).
% X became handled
fif(and(query_belief(X, Asktime),
    and(now(Asktime),
    and(neg_int(X),
    and(pos_int_past(X, Learnedtime, Endtime),
    handled(X, Endtime))))),
answer(quote(query_belief(`X, `Asktime)), no, reason(quote(handled(`X, `Endtime))))).
% X became retired
fif(and(query_belief(X, Asktime),
    and(now(Asktime),
    and(neg_int(X),
    and(pos_int_past(X, Learnedtime, Endtime),
    retired(X, Endtime))))),
answer(quote(query_belief(`X, `Asktime)), no, reason(quote(retired(`X, `Endtime))))).

% Truth query: unsure
% X became distrusted
fif(and(query_truth(X, Asktime),
    and(now(Asktime),
    and(neg_int(X),
    and(pos_int_past(X, Learnedtime, Endtime),
    distrusted(X, Endtime))))),
answer(quote(query_truth(`X, `Asktime)), unsure, reason(quote(distrusted(`X, `Endtime))))).
% X became handled
fif(and(query_truth(X, Asktime),
    and(now(Asktime),
    and(neg_int(X),
    and(pos_int_past(X, Learnedtime, Endtime),
    handled(X, Endtime))))),
answer(quote(query_truth(`X, `Asktime)), unsure, reason(quote(handled(`X, `Endtime))))).
% X became retired
fif(and(query_truth(X, Asktime),
    and(now(Asktime),
    and(neg_int(X),
    and(pos_int_past(X, Learnedtime, Endtime),
    retired(X, Endtime))))),
answer(quote(query_truth(`X, `Asktime)), unsure, reason(quote(retired(`X, `Endtime))))).


% When X isn't a current belief, and wasn't a belief in the past either
% Belief query: no
fif(and(query_belief(X, Asktime),
    and(now(Asktime),
    and(neg_int(X),
    neg_int_past(X, Learnedtime, Endtime)))),
answer(quote(query_belief(`X, `Asktime)), no, reason(quote(never_believed_formula(`X))))).

% Truth query: no
fif(and(query_truth(X, Asktime),
    and(now(Asktime),
    and(neg_int(X),
    neg_int_past(X, Learnedtime, Endtime)))),
answer(quote(query_truth(`X, `Asktime)), unsure, reason(quote(never_believed_formula(`X))))).


% Meta-formula managing query answers: if an "unsure" answer exists with a "no" answer, update the former to reflect the stronger "no"
fif(and(answer(Query, unsure, Reason_a),
    answer(Query, no, Reason_b)),
update(quote(answer(`Query, unsure, `Reason_a)), quote(answer(`Query, no, `Reason_a)))).
