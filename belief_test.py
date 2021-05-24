#!/usr/bin/env python

import os
import subprocess
import re
import pprint
import json
import pickle
import argparse

def timesteps(almalog):
  messages = []
  
  with open(almalog) as file_in:
    timesteps = []
    currenttime = {}
    splitter = re.compile(": | \(parents: | \(children: |, children: ")
    for line in file_in:
      if "Idling..." in line:
        continue

      while line.startswith("alma: "):
        line = line[6:]

      if line.startswith("-a: "):
        # Record info from ALMA messages that matter for calculating inference count
        line = line[4:]
        if line.startswith("Duplicate clause "):
          line = line[17:-1].split(" merged into ")
          for kind, arg1, _ in messages:
            if (kind == "add" or kind == "obs") and arg1 == line[0]:
              messages.remove((kind, arg1, _))
              break
          messages.append(("dupe", line[0], line[1]))

        elif line.endswith(" removed\n"):
          line = line[:-9]
          for kind, arg1, arg2 in messages:
            if (((kind == "add" or kind == "obs") and (line == arg1))
                or (kind == "update" and (line == arg2 or line.startswith(arg2 + " (")))):
              messages.remove((kind, arg1, arg2))
              break
          messages.append(("del", line, None))
          
        elif line.endswith(" added\n"):
          messages.append(("add", line[:-7], None))

        elif line.endswith(" observed\n"):
          messages.append(("obs", line[:-10], None))

        elif "updated to" in line:
          line = line[:-1].split(" updated to ")
          for kind, arg1, arg2 in messages:
            if ((kind == "add" or kind == "obs") and line[0] == arg1) or (kind == "update" and line[0] == arg2):
              messages.remove((kind, arg1, arg2))
              break
          messages.append(("update", line[0], line[1]))

      else:
        # Process KB line
        line = splitter.split(line)
        lenLine = len(line)
        if lenLine >= 2:
          pieces = len(line)

          sentence = None
          parents = None
          children = None

          if pieces == 2:
            sentence = line[1][:-1]
          else:
            sentence = line[1]

          if pieces == 3:
            if '[' in line[2]:
              parents = line[2][:-2]
            else:
              children = line[2][:-2]
          elif pieces == 4:
            parents = line[2]
            children = line[3][:-2]

          if parents:
            parents = parents.split("], [")
            parents[0] = parents[0][1:]
            parents[-1] = parents[-1][:-1]
            parents = [parent_set.split(", ") for parent_set in parents]
            for index, parent_set in enumerate(parents):
              parents[index] = [int(parent) for parent in parent_set]

          if children:
            children = children.split(", ")
            children = [int(child) for child in children]

          currenttime[int(line[0])] = (sentence, parents, children)
        elif lenLine == 1:
          timesteps.append((currenttime, messages))
          currenttime = {}
          messages = []

  #pp = pprint.PrettyPrinter(indent=2)
  #for time in timesteps:
  #  pp.pprint(time)
  #  print("")

  return timesteps


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument('-b', '--base', required=True, help='Base axiom file', type=str)
  ap.add_argument('-d', '--dir', required=True, help='.pl and .txt directory', type=str)
  args = vars(ap.parse_args())

  distrust_re = re.compile("distrusted\(\"(.*)\", ([0-9]*)\)")

  expected_dir = os.path.join(args['dir'], "expected")
  pl_dir = os.path.join(args['dir'], "almafiles")

  for filename in os.listdir(expected_dir):
    prefix = filename[:-4]

    # Run with base and corresponding pl file from almafiles dir
    print("Running " + prefix)

    with open(os.devnull, 'w') as devnull:
      subprocess.call(["./alma.x", "-f" , args['base'], "-f", os.path.join(pl_dir, prefix + ".pl"), "-r"], stdout = devnull, stderr = devnull)

    # Parse timesteps structure from ALMA log
    log = max(os.listdir("."), key=os.path.getctime)
    log_info = timesteps(log)
    
    # Open file of expected final beliefs
    with open(os.path.join(expected_dir, filename)) as file_expected:
      expecteds = [line.strip() for line in file_expected.readlines()]
      expected_believed = [-1] * len(expecteds)
      expected_distrusted = [-1] * len(expecteds)

      for time, (timestep, messages) in enumerate(log_info):
        for index, (sentence, parents, child) in timestep.items():
          if sentence in expecteds:
            expected_believed[expecteds.index(sentence)] = time
          distrust = distrust_re.search(sentence)
          if distrust and distrust.group(1) in expecteds:
            expected_distrusted[expecteds.index(distrust.group(1))] = time

      present = 0
      for index, expected in enumerate(expecteds):
        if expected_believed[index] > expected_distrusted[index]:
          print("Found " + expected + " at timestep " + str(expected_believed[index]))
          present = present + 1
        else:
          print(expected + " missing!")
      print(str(present) + "/" + str(len(expecteds)) + " expected beliefs found in log " + log + "\n")

if __name__ == "__main__":
  main()
