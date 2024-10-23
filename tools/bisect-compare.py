import os
from time import sleep
import string
from typing import List
from datetime import date
from urllib.request import urlopen
from urllib.error import URLError
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

def read_file_lines(filename : str):
    print("Reading:", filename)
    file = open(filename, 'r')
    filelines = file.readlines()
    file.close()
    return filelines

sleeptime = 1.0
mult = 0.75
scale = 0.75
def request_backoff():
    global sleeptime, mult
    sleeptime = sleeptime*(1+mult)
    mult = mult*scale
    print("Backing off. New sleeptime:", sleeptime)
    sleep(10)

def request_success():
    global sleeptime, mult, scale
    sleeptime = sleeptime*(1-mult)

def fetch_link(link):
    doagain = True
    while doagain:
        doagain = False
        print("Fetching:", link)
        sleep(sleeptime)
        try: rawpage = urlopen(link)
        except URLError as e:
            if (e.code == 429):
                doagain = True
                backoff()
            else:
                os._exit(os.EX_DATAERR)

    return html.fromstring(rawpage.read())

def fetch_kcommit_date(link : str):
    kcommit_html = fetch_link(link)
    kcommit_dates = kcommit_html.xpath("//body/div[@id='cgit']/div[@class='content']/table[@class='commit-info']/tr/td[@class='right']/text()")
    if (len(kcommit_dates) < 2 or len(kcommit_dates[0]) < 10):
        return date.fromisoformat("1990-01-01") # yeah, I know. key value bad
    kcommit_date = date.fromisoformat(kcommit_dates[0][:10])
    print("Date:", kcommit_date.isoformat())
    return kcommit_date

def link2hash(link : str):
    hsh = link.split('=')[-1]
    print(hsh)
    return hsh

def main():
    buglines = read_file_lines(bugfilename)
    print()

    outfile = open(outfilename, 'w')
    for line in buglines:
        buglink = line.split(',')[0]
        print(buglink, ",", sep='', end='', file=outfile, flush=True)
        print(line.split(',')[1])
        bughtml = fetch_link(buglink)
        bisect_result = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/b[1]/text()")
        if (len(bisect_result) == 0):
            print("No bisection found")
            print()
            print("none,", sep='', file=outfile, flush=True)
            continue
        bisect_result = bisect_result[0]
        if ("Cause bisection:" not in bisect_result):
            print("Failed to parse bisection result: " + bisect_result)
            print()
            print(",,,parse failure", sep='', file=outfile, flush=True)
            continue
        
        if ("introduced by" in bisect_result):
            print("Found good result")
            print("good,", sep='', end='', file=outfile, flush=True)
            kcommit_link = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/span[@class='mono']/a/@href")
            if (len(kcommit_link) == 0):
                print("Failed to find bisect commit link")
                print()
                print(",,parse failure", sep='', file=outfile, flush=True)
                continue
            kcommit_link = kcommit_link[0]
            bisect_hash = link2hash(kcommit_link)
            bisect_date = fetch_kcommit_date(kcommit_link)
            if (bisect_date.isoformat() == "1990-01-01"):
                print("Failed to find bisect commit dates")
                print()
                print(",,parse failure", sep='', file=outfile, flush=True)
                continue
            print(bisect_date, ",", sep='', end='', file=outfile, flush=True)
            guilty_link = ''.join(line.split(',')[9].split('\\'))
            guilty_hash = link2hash(guilty_link)
            guilty_date = fetch_kcommit_date(guilty_link)
            if (guilty_date.isoformat() == "1990-01-01"):
                print("Failed to find guilty commit dates")
                print()
                print(",parse failure", sep='', file=outfile, flush=True)
                continue
            
            if (bisect_hash == guilty_hash):
                print("Bisection reached the correct result")
                print("correct,", sep='', file=outfile, flush=True)
            else:
                print("Bisection reached an incorrect result")
                print("incorrect,", sep='', file=outfile, flush=True)
            
            # difference here gives a duration
            print("Date difference:", (bisect_date - guilty_date).days)

        elif ("the cause commit could be any of" in bisect_result):
            print("Found multiple results")
            print("multiple,", sep='', end='', file=outfile, flush=True)
            linklist = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/span[@class='mono']/a/@href")
            if (len(linklist) == 0):
                print("Failed to find bisect commit link")
                print()
                print(",,parse failure", sep='', file=outfile, flush=True)
                continue
            hashes = []
            for link in linklist:
                hashes.append(link2hash(link))
            
            guilty_link = ''.join(line.split(',')[9].split('\\'))
            guilty_hash = link2hash(guilty_link)
            guilty_date = fetch_kcommit_date(guilty_link)
            if (guilty_date.isoformat() == "1990-01-01"):
                print("Failed to find guilty commit dates")
                print()
                print(",,parse failure", sep='', file=outfile, flush=True)
                continue
            
            if (guilty_hash in hashes):
                print("Bisection reached the correct result")
                print(guilty_date, ",correct,", sep='', file=outfile, flush=True)
                print("Date difference: 0")
            else:
                print("Bisection reached an incorrect result")
                bisect_date = fetch_kcommit_date(linklist[0])
                print(bisect_date, ",incorrect,", sep='', file=outfile, flush=True)
                print("Date difference:", (bisect_date - guilty_date).days)

        elif ("failed" in bisect_result):
            print("Found bisect error")
            print("error,", sep='', end='', file=outfile, flush=True)
            errlog_link = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/b[2]/a[1]/@href")
            if (len(errlog_link) < 1):
                print("Failed to find bisect error log")
                print()
                print(",,parse failure", sep='', file=outfile, flush=True)
                continue
            errlog_link = errlog_link[0]
            errlog_html = fetch_link(errlog_link)
            errmsg = errlog_html.xpath("/html/body/p/text()")
            if (len(errmsg) < 1):
                print("Failed to read bisect error log")
                print()
                print(",,parse failure", sep='', file=outfile, flush=True)
                continue
            errmsg = errmsg[0]
            # lxml.html.tostring(element)
            print("Error message:", errmsg)
            print(",incorrect,", errmsg, sep='', file=outfile, flush=True)
        
        else:
            print("Unknown bisection result:", bisect_result)
            print(",,,parse failure", sep='', file=outfile, flush=True)
            continue
        print()
        # buglink, bisect result (good, multiple, error, none), bisect date, correctness (correct, incorrect), err

    outfile.close()
    os._exit(os.EX_OK)

if __name__ == "__main__":
    main()
