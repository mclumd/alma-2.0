wiseMan(first).
wiseMan(last).

hatColor(black).
hatColor(white).

sees(last,hatColor(first, white)).

believes(first,canSeeSomething(last)).

fif(
  and(
    believes(A,hatColor(A, C)),
    hatColor(C)),
  conclusion(announces(A,hatColor(A,C)))).

fif(
  sees(A,X),
  conclusion(believes(A,X))).
fif(
  believes(A,sees(B,C)),
  conclusion(believes(A,believes(B,C)))).
fif(
  believes(A,believes(B,C)),
  conclusion(believes(A,C))).

fif(
  and(
    and(
      hatColor(C),
      hatColor(D)),
    and(
      believes(A,hatColor(A,not(C))),
      eval_bound(C,D))),
  conclusion(believes(A,hatColor(A,D)))).

fif(
  and(
    and(
      wiseMan(X),
      wiseMan(Y)),
    and(
      believes(X, hatColor(Y, black)),
      eval_bound(X,Y))),
  conclusion(believes(X, hatColor(X, white)))).

fif(
  and(
    wiseMan(Y),
    and(
      idling(X),
      eval_bound(X,Y))),
  conclusion(noAnnounce(Y, at(X)))).

fif(
  and(
    and(
      wiseMan(X),
      wiseMan(Y)),
    and(
      noAnnounce(Y,T),
      eval_bound(X,Y))),
  conclusion(believes(X,doesNotKnowAt(Y,T)))).

fif(
  believes(X,doesNotKnowAt(Y,_)),
  conclusion(believes(X, not(sees(Y, hatColor(X,black)))))).

fif(
  and(
    believes(X,not(sees(Y,Z))),
    believes(X,canSeeSomething(Y))),
  conclusion(believes(X,sees(Y,not(Z))))).

fif(
  believes(X,sees(Y,not(hatColor(W,Z)))),
  conclusion(believes(X,sees(Y,hatColor(W,not(Z)))))).
