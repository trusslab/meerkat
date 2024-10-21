import os
from urllib.request import urlopen
from lxml import html

# For each bug in bugfile
# check for Syz-bisect result
# gather error or commit
    # gather what error
    # gather what commit
        # parse bisect log?
# compare to actual introducing commit (bugfile)

bugfilename = ""
outfilename = ""

def fetch_link(l):
    rawpage = urlopen(l)
    return html.fromstring(rawpage.read())

def main():
    bugfile = open(bugfilename, 'r')
    buglines = bugfile.readlines()
    bugfile.close()

    outfile = open(outfilename, 'w')
    for line in buglines:
        buglink = line.split(',')[0]
        bughtml = fetch_link(buglink)
        # body/div/div<class = bug-bisection-info>
        # b/text() contains Cause Bisection
            # for fail, b/text() contains "failed"
            # for success, b/text() contains "introduced by"
        # span<class = mono>/text() contains:
            # "commit <commit>"
            # "Date: <date>"

    outfile.close()
    os._exit(os.EX_OK)

if __name__ == "__main__":
    main()
