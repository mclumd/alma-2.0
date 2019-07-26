fif(
    and(hearing(julia, T),
    and(proc(neg_int(talking),bound),
    now(X))),
    conclusion(ros(speak(), X))).
fif(
    and(hearing(julia, T),proc(neg_int(talking),bound)),
    conclusion(ros(raise_arm(), X))).
