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
# Example:
./alma.x demo/fc-test.pl
```
#### Commands
`step`: Makes one derivation step. Will print `Idling...` if there are no derivations left to make.
`print`: Prints out the contents of the knowledge base

`add <pred>.`: Will add a predicate to the knowledge base the next time `step` is called. Note that the predicate must end in a `.`

`del <pred>.`: Immediately deletes a predicate from the knowledge base. Note that the predicate must end in a `.`

`halt`: Stops ALMA
#### Syntax
```
alma         : /^/ <almaformula>* /$/ ;                    
almaformula  : (<fformula> | <bformula> | <formula>) '.' ; 
formula      : \"and(\" <formula> ',' <formula> ')'        
             | \"or(\" <formula> ','  <formula> ')'        
             | \"if(\" <formula> ',' <formula> ')'         
             | <literal> ;                                 
fformula     : \"fif(\" <conjform> ',' \"conclusion(\"     
               <poslit> ')' ')' ;                          
bformula     : \"bif(\" <formula> ',' <formula> ')' ;      
conjform     : \"and(\" <conjform> ',' <conjform> ')'      
             | <literal> ;                                 
literal      : <neglit> | <poslit> ;                       
neglit       : \"not(\" <poslit> ')' ;                     
poslit       : <predname> '(' <listofterms> ')'            
             | <predname> ;                                
listofterms  : <term> (',' <term>)* ;                      
term         : <funcname> '(' <listofterms> ')'            
             | <variable> | <constant> ;                   
predname     : <prologconst> ;                             
constant     : <prologconst> ;                             
funcname     : <prologconst> ;                             
variable     : /[A-Z_][a-zA-Z0-9_]*/ ;                     
prologconst  : /[a-zA-Z0-9_]*/      ;                      
```
## Troubleshooting 
```
make clean
make
```
