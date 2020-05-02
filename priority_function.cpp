#include <iostream>
extern "C" {
  #include "compute_priority.h"
  #include "alma_kb.h"
}

extern "C" double rl_priority(kb *knowledge_base, res_task *t);



/*
  Eventually, this will be a full-fledged deep-RL solution.  For now, we estimate

     Q*(k_t, res) = gamma * Val(k')

  where k' is the KB after res has been executed.  Then Val(k') is by
  definition the average (discounted) reward after being in state k'.
  The Bellman equation defines it as:

     Val(k_t) = E(R_t + gamma*Val(k'))

   We estimate it by using a representation F(k_t) and employing the Belmman eqn:

      Val(F(k_t)) = E(R_t + gamma*Val(F(k')))

   For F, we'll initially use something very simple.  For example,
   let's say F(k) = avg_{s \in k} f(s) where f(s) is a relevance score
   assigned to a sentence s.  Initially, this can be based on a very
   simple heuristic like
   
      f(s) == 2 if it is an axiom
      f(s) == max_{t in s} (relevance(t), t a term of s) otherwise.



  New idea; algorithm 1.

  g(subj) = 1 if s 

 */
double rl_priority(kb *knowledge_base, res_task *t) {
  //std::cout << "In the C++ code";
  return 0;  //temporary
}
