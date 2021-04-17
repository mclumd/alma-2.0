fif(and(hearing(julia, T),
    and(neg_int(quote(talking)),
    and(pos_int(quote(now(`X))),
    pos_int(quote(saw(person,`A,`B,`C,`D,`E,`F)))))),
  ros(raise_arm(B,C), X)).
fif(and(hearing(julia, T),
    and(neg_int(quote(talking)),
    and(pos_int(quote(now(`X))),
    pos_int(quote(saw(person,`A,`B,`C,`D,`E,`F)))))),
  ros(speak, X)).
