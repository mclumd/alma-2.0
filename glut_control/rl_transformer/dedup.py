"""Given a primary and secondary file, remove lines from the
secondary file that are also present in the primary file.  Since the
secondary file is likely to be significantly shorter, we'll do our
preprocessing on it.
"""

def dedup(primary_fname, secondary_fname):
    pfile = open(primary_fname, "rt")
    sfile = open(secondary_fname, "rt")

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
            print(line, end="")

dedup("/tmp/primary.txt", "/tmp/secondary.txt")

