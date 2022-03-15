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
./alma.x [-r] [-v] [-f file] [-a agent] [-n nesting]
```

required arguments:  
  `-f file`   provides an input file for beginning ALMA KB; may be repeated for multiple input files

optional arguments:  
  `-r`         runs automatically until halting when reasoner first idles
  `-v`         runs with verbose output
  `-a agent`   provides the agent name to identify this ALMA instance
  `-n nesting` sets the maximum depth of nesting for modeling agent beliefs

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
almaformula  : <sentence> '.'
sentence     : <fformula> | <bformula> | <formula>
formula      : \"and(\" <formula> ',' <formula> ')' | \"or(\" <formula> ','  <formula> ')'
             | \"if(\" <formula> ',' <formula> ')' | \"not(\" <formula> ')' | <literal>
fformula     : \"fif(\" <conjform> ',' <fformconc> ')'
fformconc    : \"and(\" <fformconc> ',' <fformconc> ')' | <fformula>
             | \"not(\" <literal> ')' | <literal>
bformula     : \"bif(\" <formula> ',' <formula> ')'
conjform     : \"and(\" <conjform> ',' <conjform> ')' | \"not(\" <literal> ')' | <literal>
literal      : <predname> '(' <listofterms> ')' | <predname>
listofterms  : <term> (',' <term>)*
term         : \"quote\" '(' <sentence> ')' | <funcname> '(' <listofterms> ')' | <variable> | <constant>
predname     : <prologconst>
constant     : <prologconst>
funcname     : <prologconst>
variable     : '`' <variable>/ | [A-Z_][a-zA-Z0-9_]*/
prologconst  : /[a-z0-9][a-zA-Z0-9_]*/
```
## Troubleshooting
```
make clean
make
```

## Testing
Testing is automated with the Python script `belief-test.py`. Test script usage is:
```
python belief_test.py [-h] -b BASE -t TOPIC -d DIR -a AXIOM_COUNT
```

Arguments:
`-b BASE, --base BASE` Base axiom file

`-t TOPIC, --topic TOPIC` Topic axiom file specific to test domain (mollusk or car)

`-d DIR, --dir DIR` .pl and .txt directory

`-a AXIOM_COUNT, --axiom_count AXIOM_COUNT` Base axiom count, summed at the end for analysis

The directory provided should contain a subdirectory `almafiles` with .pl files, and a subdirectory `expected` with .txt files of expected steady-state beliefs.

Examples to run the car or mollusk test batches:
```
python belief_test.py -b demo/default/base.pl -t demo/default/car/topic.pl -d demo/default/car/ -a 19
python belief_test.py -b demo/default/base.pl -t demo/default/mollusk/topic.pl -d demo/default/mollusk/ -a 19
```
