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
  announces(A,hatColor(A,C))).

fif(
  sees(A,X),
  believes(A,X)).
fif(
  believes(A,sees(B,C)),
  believes(A,believes(B,C))).
fif(
  believes(A,believes(B,C)),
  believes(A,C)).

fif(
  and(
    and(
      hatColor(C),
      hatColor(D)),
    and(
      believes(A,hatColor(A,not(C))),
      eval_bound(C,D))),
  believes(A,hatColor(A,D))).

fif(
  and(
    and(
      wiseMan(X),
      wiseMan(Y)),
    and(
      believes(X, hatColor(Y, black)),
      eval_bound(X,Y))),
  believes(X, hatColor(X, white))).

fif(
  and(
    wiseMan(Y),
    and(
      idling(X),
      eval_bound(X,Y))),
  noAnnounce(Y, at(X))).

fif(
  and(
    and(
      wiseMan(X),
      wiseMan(Y)),
    and(
      noAnnounce(Y,T),
      eval_bound(X,Y))),
  believes(X,doesNotKnowAt(Y,T))).

fif(
  believes(X,doesNotKnowAt(Y,_)),
  believes(X, not(sees(Y, hatColor(X,black))))).

fif(
  and(
    believes(X,not(sees(Y,Z))),
    believes(X,canSeeSomething(Y))),
  believes(X,sees(Y,not(Z)))).

fif(
  believes(X,sees(Y,not(hatColor(W,Z)))),
  believes(X,sees(Y,hatColor(W,not(Z))))).
