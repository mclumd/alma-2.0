% Idea:  books tend to be on shelves

if( isA(bookshelf, X), contains(X, book)).
if( isA(book, X), isA(book, sequel(X))).
if( isA(book, X), isA(book, prequel(X))).
isa(bookshelf, thisShelf).

% distractors


if(isA(bookshelf, X), isA(furniture, X)).
if(isA(table, X), contains(X, legs)).


if(isA(chair, X), isA(furniture, X)).
if(isA(chair, X), contains(X, legs)).
if(isA(chair, X), affords(X, sitting)).
if(isA(chair, X), affords(X, placing)).
%% if(isA(chair, X), contains(X, legs)).
%% if(isA(chair, X), contains(X, legs)).
%% if(isA(chair, X), contains(X, legs)).
%% if(isA(chair, X), contains(X, legs)).
%% if(isA(chair, X), contains(X, legs)).
%% if(isA(chair, X), contains(X, legs)).


if(isA(table, X), isA(furniture, X)).
if(isA(table, X), affords(X, placing)).
%% if(isA(table, X), contains(X, legs)).
%% if(isA(table, X), contains(X, legs)).
%% if(isA(table, X), contains(X, legs)).
%% if(isA(table, X), contains(X, legs)).
%% if(isA(table, X), contains(X, legs)).
%% if(isA(table, X), contains(X, legs)).
%% if(isA(table, X), contains(X, legs)).
%% if(isA(table, X), contains(X, legs)).
%% if(isA(table, X), contains(X, legs)).

% Goal stuff.  A followup might look for the sequel to myNovel.
desire(find(myNovel)).
isA(book, myNovel).
if(and(contains(X,Y),isA(Y, Z)), lookFor(Z,X)).


% Ultimate goal for initial feasability tests will be to entoken lookFor(bookshelf, myNovel).

