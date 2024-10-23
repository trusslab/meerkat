import os
from datetime import date
from urllib.request import urlopen
from lxml import html

# For each bug in bugfile
# check for Syz-bisect result
# gather error or commit
    # gather what error
    # gather what commit
        # parse bisect log?
# compare to actual introducing commit (bugfile)

bugfilename = "../results/batch1/bugs-batch1.csv"
outfilename = "bisect-comp.log"

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
        print(line.split(',')[1])
        print(buglink)
        bughtml = fetch_link(buglink)
        bisect_result = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/b[1]/text()")
        if (len(bisect_result) == 0):
            print("No result found")
            print()
            continue
        if ("Cause bisection:" not in bisect_result[0]):
            print("Failed to parse bisection result: " + bisect_result[0])
            print()
            continue
        
        if ("introduced by" in bisect_result[0]):
            print("Found good result")
            kcommit_link = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/span[@class='mono']/a/@href")
            if (len(kcommit_link) == 0):
                print("Failed to find bisect commit link")
                print()
                continue
            kcommit_link = kcommit_link[0]
            print(kcommit_link)
            kcommit_html = fetch_link(kcommit_link)
            kcommit_dates = kcommit_html.xpath("//body/div[@id='cgit']/div[@class='content']/table[@class='commit-info']/tr/td[@class='right']/text()")
            if (len(kcommit_dates) < 2):
                print("Failed to find bisect commit dates")
                print()
                continue
            bisect_date = date.fromisoformat(kcommit_dates[0][:10])
            print(bisect_date.isoformat())

        elif ("failed" in bisect_result[0]):
            print("Found bisect error")
            # go fetch the error log and print it
        print()

    outfile.close()
    os._exit(os.EX_OK)

if __name__ == "__main__":
    main()
