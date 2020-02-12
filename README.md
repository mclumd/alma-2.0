# ALMA 2.0
C-based Active Logic MAchine

## Getting Started
```
git clone https://github.com/mclumd/alma-2.0.git
cd alma-2.0
make
```

## Usage
```
./alma.x [-r] [-f file] [-a agent]
```

required arguments:  
  `-f file`   provides an input file for beginning ALMA KB

optional arguments:  
  `-r`        runs ALMA automatically until halting when it first idles  
  `-a agent`  provides the agent name to identify this ALMA instance

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
alma         : /^/ (<almaformula> | <almacomment>)* /$/         
almacomment  : /%[^\n]*\n/
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
