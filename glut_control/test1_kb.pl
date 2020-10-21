%if(and(distanceAt(Item1, D1, T), distanceAt(Item2, D2, T)), distanceBetweenBoundedBy(Item1, Item2, D1, T)).

%if(and(distanceAt(b, D1, T), distanceAt(Item2, D2, T)), distanceBetweenBoundedBy(Item2, b, D1, T)).

if(and(distanceAt(Item2, D2, T), distanceAt(b, D1, T)), distanceBetweenBoundedBy(Item2, b, D1, T)).

