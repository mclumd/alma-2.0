rel(cephalopod, steve).
rel(nautilus, nancy).
rel(naked_nautilus, barry).

rel(is_a, hermit_naked_nautilus, naked_nautilus).
fif(rel(hermit_naked_nautilus, X), rel(shell_bearer, X)).
