# ALMA 2.0
Beginning steps toward a new version of ALMA

## Getting Started
```
git clone https://github.com/mclumd/alma-2.0.git
cd alma-2.0
make
```

## Usage
```
cd alma-2.0
./alma.x \path\to\pl\file
# Examples:
./alma.x demo/fc-test.pl
./alma.x demo/cycle.pl run
```
#### Commands
`step`: Makes one derivation step. Will print `Idling...` if there are no remaining tasks which might produce further derivations.

`print`: Prints out the contents of the knowledge base

`add <almaformula>.`: Will add a formula to the knowledge base the next time `step` is called. Note that the predicate must end in a `.`, as in the grammar rules.

`obs <literal>.`: Adds a literal with an additional added parameter for the time added.

`del <almaformula>.`: Immediately deletes a formula from the knowledge base. Note that the formula must end in a `.`

`update <almaformula>. <almaformula>.`: Replaces an existing formula with another. First formula must already be present, second cannot be present. Neither is allowed to be a fif formula.

`bs <literal>.`: Initiates a backward search for the argument literal.

`halt`: Stops ALMA

#### Grammar
```
alma         : /^/ <almaformula>* /$/         
almaformula  : (<fformula> | <bformula> | <formula>) '.'
formula      : \"and(\" <formula> ',' <formula> ')' | \"or(\" <formula> ','  <formula> ')'
             | \"if(\" <formula> ',' <formula> ')' | \"not(\" <formula> ')' | <literal>
fformula     : \"fif(\" <conjform> ',' \"conclusion(\" <literal> ')' ')'
bformula     : \"bif(\" <formula> ',' <formula> ')'
conjform     : \"and(\" <conjform> ',' <conjform> ')' | \"not(\" <literal> ')' | <literal>
literal      : <predname> '(' <listofterms> ')' | <predname>
listofterms  : <term> (',' <term>)*
term         : <funcname> '(' <listofterms> ')'| <variable> | <constant>
predname     : <prologconst>
constant     : <prologconst>
funcname     : <prologconst>
variable     : /[A-Z_][a-zA-Z0-9_]*/
prologconst  : /[a-zA-Z0-9_]*/
```
## Troubleshooting
```
make clean
make
```
