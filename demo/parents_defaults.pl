fif(and(a, neg_int(quote(abnormal(i, j, k)))), conc).
fif(a, conc2).
a.

fif(and(conc, parents_defaults(quote(conc), 2)), from_defaults(conc)).
fif(and(conc2, parents_defaults(quote(conc2), 2)), from_defaults(conc2)).

fif(and(conc, parent_non_default(quote(conc), 2)), not(from_defaults(conc))).
fif(and(conc2, parent_non_default(quote(conc2), 2)), not(from_defaults(conc2))).
