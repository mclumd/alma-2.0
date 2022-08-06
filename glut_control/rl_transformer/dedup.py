"""Given a primary and secondary file, remove lines from the
secondary file that are also present in the primary file.  Since the
secondary file is likely to be significantly shorter, we'll do our
preprocessing on it.
"""

import argparse
import tempfile
import os

def dedup(primary_fname, secondary_fname, replace=False):
    pfile = open(primary_fname, "rt")
    sfile = open(secondary_fname, "rt")
    if replace:
        tmpfile = tempfile.NamedTemporaryFile(mode="wt", delete=False)

    slines = set()
    sline_locations = {}
    for linenum, line in enumerate(sfile):
        if line == "\n":
            continue
        slines.add(line)
        if line in sline_locations:
            sline_locations[line].append(linenum)
        else:
            sline_locations[line] = [linenum]

    skiplines = []
    for pline in pfile:
        if pline in slines:
            #print("Found duplicate line: {}".format(pline))
            skiplines.extend(sline_locations[pline])

    sfile.seek(0)
    prev_newline = False
    for linenum, line in enumerate(sfile):
        if line == "\n":
            if prev_newline:
                continue
            else:
                prev_newline = True
        else:
            prev_newline = False
        if linenum not in skiplines:
            if replace:
                tmpfile.write(line)
            else:
                print(line, end="")
    pfile.close()
    sfile.close()
    if replace:
        tname = tmpfile.name
        tmpfile.close()
        os.rename(tname, secondary_fname)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Remove duplicate lines from a file")
    parser.add_argument("--primary_fname", default="/tmp/off_data_tds.txt", help="The primary file")
    parser.add_argument("--secondary_fname", default="/tmp/off_data_tds_val.txt", help="The secondary file")
    dedup(args.primary_fname, args.secondary_fname)

