fif(and(foo,
    proc(ancestor(bar, foo), bound)),
    conclusion(yes_foo)).
foo.
fif(and(foo2,
    proc(ancestor(bar, foo2), bound)),
    conclusion(yes_foo2)).
bar.
if(bar, foo2).
