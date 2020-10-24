#!/usr/bin/env python

import subprocess
import sys
import string
import time
import argparse
import re
from threading import Lock
import signal, os
from copy import deepcopy
import numpy as np
import alma
import functools
import rl_functions

default_rl_params = {
  'training':  True,
  'use_prb': True
  }
   
def string_replace(S, a, b):
  if sys.version_info.major == 3:
    return str(str(S).replace(a,b))
  else:
    return string.replace(S, a, b)


def clean_clause(clause_tree):
  if clause_tree == []:
    return []
  elif type(clause_tree) != type([]):
    return clause_tree
  # Get rid of unary 'or', 'and'
  elif clause_tree[0] in [ 'or', 'and'] and len(clause_tree[1]) == 1:
    return clean_clause(clause_tree[1][0])
  else:
    return [clause_tree[0]] + [ clean_clause(c) for c in clause_tree[1:]]



def clause_constants(clause_tree, note_place=False):
  """ Returns the constants in a clause.  These are essentially the 0-ary functions. """
  if clause_tree == []:
    return []
  elif clause_tree[0] == 'func':
    assert(len(clause_tree) == 3)
    if clause_tree[2] == []:
      return [clause_tree[1]]
    else:
      pass1 =  [clause_constants(c, False) for c in clause_tree[2]]
      if not note_place:
        return pass1
      else:
        pass2 = [ ''.join(x + ( [] if x == [] else ['/'+str(i)]))  for i,x in enumerate(pass1)]
        return pass2
  elif clause_tree[0] == 'var':
    return []
  elif clause_tree[0] == 'if':
    return [clause_constants(clause_tree[1], note_place), clause_constants(clause_tree[2], note_place)]
  elif clause_tree[0] in ['and', 'or']:  # Binary operators
    return [clause_constants(c, note_place)  for c in clause_tree[1]]
  elif clause_tree[0] in ['not']:  # Unary operator
    return [clause_constants(c[1], note_place)]
  else:
    return [clause_constants(c, note_place) for c in clause_tree[1:]]

def clause_words(clause_tree):
  """ Returns the constants in a clause.  These will be everything that is not a keyword. """
  if clause_tree == []:
    return []
  elif clause_tree[0] == 'func':
    assert(len(clause_tree) == 3)
    if clause_tree[2] == []:
      return [clause_tree[1]]
    else:
      return [clause_tree[1]] + [clause_words(c) for c in clause_tree[2]]
  elif clause_tree[0] == 'var':
    return []
  else:
    return [clause_words(c) for c in clause_tree[1:]]




def flatten_tree(S):
  if S == []:
      return S
  if isinstance(S[0], list):
      return flatten_tree(S[0]) + flatten_tree(S[1:])
  return S[:1] + flatten_tree(S[1:])

def alma_constants(list_of_clauses, note_place=False):
  """ Returns the constants in a list of clauses
  """
  res = []
  for c in list_of_clauses:
    res.append(flatten_tree(clause_constants(clean_clause(c), note_place)))
  S = functools.reduce(set.union, [ set(x) for x in res if x != '']).difference(set(['']))
  return S
  #return set(flatten_tree([clause_constants(clean_clause(c)) for c in list_of_clauses]))

def alma_words(list_of_clauses):
  """ Returns the constants in a list of clauses
  """
  res = []
  for c in list_of_clauses:
    res.append(flatten_tree(clause_words(clean_clause(c))))
  S = functools.reduce(set.union, [ set(x) for x in res])
  return S

def kb_constants(alma_agent):
  clauses = alma_agent.kb_list()
  return alma_constants(clauses)

def kb_words(alma_agent):
  clauses = alma_agent.kb_list()
  return alma_words(clauses)
