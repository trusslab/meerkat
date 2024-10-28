import os
from time import sleep
import string
from typing import List
from datetime import date
from urllib.request import urlopen
from urllib.error import URLError
from requests.exceptions import SSLError
from lxml import html

class Bisect_Result:
    def __init__(self) -> None:
        self.link = ""          # bug link
        self.converge = ""      # good/multiple/error/none
        self.hash = ""          # hash of resulting commit
        self.date = ""          # date of resulting commit
        self.best_hash = ""     # the farthest back in bisection where the bug reproduced
        self.best_date = ""     # the date of the above hash
        self.correct = ""       # correct/incorrect
        self.err = ""           # err.what
    
    def reset(self) -> None:
        self.link = ""
        self.converge = ""
        self.hash = ""
        self.date = ""
        self.best_hash = ""
        self.best_date = ""
        self.correct = ""
        self.err = ""

    def tostring(self) -> str:
        st = self.link + ',' + self.converge + ',' + self.hash + ',' + self.date + ',' + self.correct + ',' + self.best_hash + ',' + self.best_date + ',' + self.err
        return st

bugfilename = "../results/batch1/bugs-batch1.csv"
outfilename = "bisect-comp-b1.csv"

GUILTY_INDEX = 9

def read_file_lines(filename : str) -> List[str]:
    print("Reading:", filename, flush=True)
    file = open(filename, 'r')
    filelines = file.readlines()
    file.close()
    return filelines

sleeptime = 1.0
mult = 0.5
scale = 0.5
count = 0
def request_backoff() -> None:
    global sleeptime, mult, count
    count = 0
    sleeptime = sleeptime*2
    mult = mult*scale
    print("Backing off. New sleeptime:", sleeptime, flush=True)
    sleep(10)

def request_success() -> None:
    global sleeptime, mult, scale, count
    count += 1
    if (count > 20):
        count = 0
        sleeptime = sleeptime*(1-mult)

def fetch_link(link : str):
    doagain = True
    while doagain:
        doagain = False
        print("Fetching:", link, flush=True)
        sleep(sleeptime)
        try: rawpage = urlopen(link)
        except URLError as e:
            if (e.code == 429):
                doagain = True
                request_backoff()
            else:
                print("Failed to fetch link", flush=True)
                return None
        except SSLError as e:
            print("Encountered SSL error")
            doagain = True
            sleep(5)

    request_success()
    return html.fromstring(rawpage.read())

def fetch_kcommit_date(link : str) -> date:
    kcommit_html = fetch_link(link)
    kcommit_dates = kcommit_html.xpath("//body/div[@id='cgit']/div[@class='content']/table[@class='commit-info']/tr/td[@class='right']/text()")
    if (len(kcommit_dates) < 2 or len(kcommit_dates[1]) < 10):
        return date.fromisoformat("1990-01-01") # yeah, I know. key value bad
    kcommit_date = date.fromisoformat(kcommit_dates[1][:10])
    print("Date:", kcommit_date.isoformat(), flush=True)
    return kcommit_date

def link2hash(link : str):
    hsh = link.split('=')[-1]
    print(hsh, flush=True)
    return hsh

def bisect_log_best_guess(bugname : str, bughtml : html, result : Bisect_Result) -> Bisect_Result:
    loglink = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/b[2]/a[contains(text(), 'bisect log')]/@href")
    if (len(loglink) == 0):
        print("Failed to find bisect log", flush=True)
        return result

    loglink = loglink[0]
    loghtml = fetch_link(loglink)
    logtext = loghtml.xpath("//body/p/text()")
    if (len(logtext) == 0):
        print("Failed to read bisect log", flush=True)
        return result

    loglines = logtext[0].split('\n')

    cur_hash = ""
    best_hash = ""
    for line in loglines:
        if (line.startswith("testing commit")):
            cur_hash = line.split(' ')[2]
            continue
        if (line.startswith("run #") or line.startswith("all runs:")):
            if (bugname.split(' ')[0] in line and bugname.split(' ')[-1] in line): # check for function name and sanitizer. Should be good enough
                best_hash = cur_hash

    if (len(best_hash) > 0):
        result.best_hash = best_hash
        best_link = "https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id="+best_hash
        result.best_date = fetch_kcommit_date(best_link).isoformat()
    else:
        print("The bug was never found in the bisect log", flush=True)
    
    return result

def handle_good_result(line : str, bughtml : html, result : Bisect_Result) -> Bisect_Result:
    print("Found good result", flush=True)
    result.converge = "good"
    
    guilty_link = ''.join(line.split(',')[GUILTY_INDEX].split('\\'))
    guilty_hash = link2hash(guilty_link)
    guilty_date = fetch_kcommit_date(guilty_link)
    if (guilty_date.isoformat() == "1990-01-01"):
        print("Failed to find guilty commit dates", flush=True)
        result.err = "parse failure"
        return result
    
    kcommit_link = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/span[@class='mono']/a/@href")
    if (len(kcommit_link) == 0):
        print("Failed to find bisect commit link", flush=True)
        result.err = "parse failure"
        return result

    kcommit_link = kcommit_link[0]
    bisect_hash = link2hash(kcommit_link)
    result.hash = bisect_hash
    bisect_date = fetch_kcommit_date(kcommit_link)
    if (bisect_date.isoformat() == "1990-01-01"):
        print("Failed to find bisect commit dates", flush=True)
        result.err = "parse failure"
        return result

    result.date = bisect_date.isoformat()

    result = bisect_log_best_guess(line.split(',')[1], bughtml, result)

    if (bisect_hash == guilty_hash):
        print("Bisection reached the correct result", flush=True)
        result.correct = "correct"
    else:
        print("Bisection reached an incorrect result", flush=True)
        result.correct = "incorrect"
    
    # difference here gives a duration
    print("Date difference:", (bisect_date - guilty_date).days, flush=True)
    return result

def handle_multiple_result(line : str, bughtml : html, result : Bisect_Result) -> Bisect_Result:
    print("Found multiple results", flush=True)
    result.converge = "multiple"
    linklist = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/span[@class='mono']/a/@href")
    if (len(linklist) == 0):
        print("Failed to find bisect commit link", flush=True)
        result.err = "parse failure"
        return result

    hashes = []
    for link in linklist:
        hashes.append(link2hash(link))
    
    guilty_link = ''.join(line.split(',')[GUILTY_INDEX].split('\\'))
    guilty_hash = link2hash(guilty_link)
    guilty_date = fetch_kcommit_date(guilty_link)
    if (guilty_date.isoformat() == "1990-01-01"):
        print("Failed to find guilty commit dates", flush=True)
        result.err = "parse failure"
        return result
    
    result = bisect_log_best_guess(line.split(',')[1], bughtml, result)

    if (guilty_hash in hashes):
        print("Bisection reached the correct result", flush=True)
        result.date = guilty_date.isoformat()
        result.correct = "correct"
        result.hash = guilty_hash
        print("Date difference: 0")
    else:
        print("Bisection reached an incorrect result", flush=True)
        bisect_date = fetch_kcommit_date(linklist[0])
        result.date = bisect_date.isoformat()
        result.correct = "incorrect"
        result.hash = link2hash(linklist[0])
        print("Date difference:", (bisect_date - guilty_date).days, flush=True)
    return result

def handle_error_result(line : str, bughtml : html, result : Bisect_Result) -> Bisect_Result:
    print("Found bisect error", flush=True)
    result.converge = "error"
    errlog_link = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/b[2]/a[1]/@href")
    if (len(errlog_link) < 1):
        print("Failed to find bisect error log", flush=True)
        result.err = "parse failure"
        return result

    errlog_link = errlog_link[0]
    errlog_html = fetch_link(errlog_link)
    errmsg = errlog_html.xpath("/html/body/p/text()")
    if (len(errmsg) < 1):
        print("Failed to read bisect error log", flush=True)
        result.err = "parse failure"
        return result

    errmsg = errmsg[0].split('\n')[0]
    result.err = errmsg
    print("Error message:", errmsg, flush=True)
    # taking too long, oldest tested release
    if ("taking too long" in errmsg or "oldest tested release" in errmsg):
        result = bisect_log_best_guess(line.split(',')[1], bughtml, result)

    result.correct = "incorrect"
    return result

def handle_oldest_result(line : str, bughtml : html, result : Bisect_Result) -> Bisect_Result:
    print("Found bisect error: oldest tested release", flush=True)
    result.converge = "error"
    result.err = "the issue happens on the oldest tested release"
    result = bisect_log_best_guess(line.split(',')[1], bughtml, result)
    return result

def main():
    global sleeptime
    br = Bisect_Result()
    buglines = read_file_lines(bugfilename)
    print()

    outfile = open(outfilename, 'w')
    for line in buglines:
        br.reset()
        buglink = line.split(',')[0]
        br.link = buglink
        print(line.split(',')[1], flush=True)
        bughtml = fetch_link(buglink)
        bisect_result = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/b[1]/text()")
        if (len(bisect_result) == 0):
            print("No bisection found", flush=True)
            br.converge = "none"

        else:
            bisect_result = bisect_result[0]
            if ("Cause bisection:" not in bisect_result):
                print("Failed to parse bisection result: " + bisect_result, flush=True)
                br.err = "parse failure"

            else:
                if ("introduced by" in bisect_result):
                    br = handle_good_result(line, bughtml, br)

                elif ("the cause commit could be any of" in bisect_result):
                    br = handle_multiple_result(line, bughtml, br)

                elif ("failed" in bisect_result):
                    br = handle_error_result(line, bughtml, br)
                
                elif ("oldest tested release" in bisect_result):
                    br = handle_oldest_result(line, bughtml, br)

                else:
                    print("Unknown bisection result:", bisect_result, flush=True)
                    br.err = "unknown result"

        print(br.tostring(), file=outfile, flush=True)
        print()

    outfile.close()
    print("Final sleeptime:", sleeptime)
    os._exit(os.EX_OK)

if __name__ == "__main__":
    main()
