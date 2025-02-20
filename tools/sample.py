import os
import string
from typing import List
import secrets

inputfile = "parse/bugs-22-23.csv"
outputfile = "parse/bugs-bisect-sampled.csv"
limit = 200

def read_file_lines(filename : str) -> List[str]:
    file = open(filename, 'r')
    filelines = file.readlines()
    file.close()
    return filelines

def main():
    lines = read_file_lines(inputfile)
    bound = len(lines)
    chosen = []
    for i in range(limit):
        c = secrets.randbelow(bound)
        while c in chosen:
            c = secrets.randbelow(bound)
        chosen.append(c)
    
    chosen.sort()

    file = open(outputfile, 'w')
    for x in chosen:
        file.write(lines[x])
    file.close()

if __name__ == "__main__":
    main()
