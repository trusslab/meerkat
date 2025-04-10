import os
import time
import string
from datetime import date
from typing import List
from urllib.request import urlopen
from urllib.error import URLError
from lxml import html

# Crawl Syzbot for bugs that fit criteria

class Commit:
    def __init__(self, h, l, d):
        self.hash = h                       # commit hash
        self.link = l                       # commit link
        self.date = d                       # commit date
    
    def is_upstream(self) -> bool:
        return ("https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/" in self.link)

    def print(self):
        print("Hash:      ", self.hash)
        print("Link:      ", self.link)
        print("Upstream:  ", self.is_upstream())
        print("Date:      ", self.date)

class CrashEntry:
    def __init__(self, k, c, r, m, d):
        self.date = d                       # date of crash
        self.kernel = k                     # kernel commit
        self.config = syzbotlink + c        # config link
        self.repro = r                      # reproducer link
        self.manager = m                    # manager name

    def print(self):
        print("Date:      ", self.date)
        print("Kernel:    ", self.kernel)
        print("Config:    ", self.config)
        print("Repro:     ", self.repro)
        print("Manager:   ", self.manager)

class Bisect_Result:
    def __init__(self) -> None:
        self.converge = ""      # good/multiple/error/none
        self.hash = ""          # hash of resulting commit
        self.date = ""          # date of resulting commit
        self.best_hash = ""     # the farthest back in bisection where the bug reproduced
        self.best_date = ""     # the date of the above hash
        self.correct = ""       # correct/incorrect
        self.err = ""           # err.what
        self.syz_repro = ""     # the repro used in bisection
    
    def reset(self) -> None:
        self.converge = ""
        self.hash = ""
        self.date = ""
        self.best_hash = ""
        self.best_date = ""
        self.correct = ""
        self.err = ""

    def tostring(self) -> str:
        st = self.converge + ',' + self.hash + ',' + self.date + ',' + self.correct + ',' + self.best_hash + ',' + self.best_date + ',' + self.err
        return st
    
    def print(self):
        print("Converge:  ", self.converge)
        print("Hash:      ", self.hash)
        print("Date:      ", self.date)
        print("Repro Hash:", self.best_hash)
        print("Repro Date:", self.best_date)
        print("Syz Repro: ", self.syz_repro)
        print("Err Msg:   ", self.err)

class Bugdata:
    def __init__(self, l):
        self.number = ""                    # bug number
        self.name = ""                      # bug name
        self.link = l                       # bug link
        self.hash = l.split("=")[-1]        # bug hash
        self.bit32 = ""                     # is the crash 32-bit
        self.crashes = []                   # list of valid crashes (anything with a repro we can use)
        self.anchor = None                  # from the earliest upstream crash
        self.truefind = ""                  # the date the bug was first found (out of all crashes)
        self.fixCommits = []                # the latest commit that links the guilty commit
        self.guiltyCommits = []             # the earliest guilty commit
        self.bisectAttempt = False          # was bisection attempted for this bug?
        self.bisectResult = Bisect_Result() # the result of bisection

    def valid_crashes(self) -> List[CrashEntry]:
        crashes = []

        crashes = filter_crashes_kernel(self.crashes)
        crashes = filter_crashes_manager(crashes)

        return crashes
    
    def anchor_crash(self) -> CrashEntry:
        crashes = self.valid_crashes()
        crashes.sort(key=sort_date)

        if len(crashes) == 0:
            return None

        return crashes[0]

    def reproducers(self):
        repros = []

        for c in self.crashes:
            if c.repro != "":
                repros.append(c.repro)

        return repros

    def validate(self) -> bool:
        if self.name == "":
            print("Bug: Bad name", flush=True)
            return False
        elif self.link == "":
            print("Bug: Bad link", flush=True)
            return False
        elif self.hash == "":
            print("Bug: Bad hash", flush=True)
            return False
        elif len(self.reproducers()) == 0:
            print("Bug: No reproducer", flush=True)
            return False
        elif self.bit32 == "":
            print("Bug: Bad 32-bit decision", flush=True)
            return False
        elif self.bit32 == "i386":
            print("Bug: Ignoring 32-bit bugs", flush=True)
            return False
        elif len(self.crashes) == 0:
            print("Bug: No crashes", flush=True)
            return False
        elif self.anchor == None:
            print("Bug: No valid anchor", flush=True)
            return False
        elif self.truefind == "":
            print("Bug: Bad truefind date", flush=True)
            return False
        elif len(self.fixCommits) == 0:
            print("Bug: No fixing commits", flush=True)
            return False
        elif len(self.guiltyCommits) == 0:
            print("Bug: No guilty commits", flush=True)
            return False
        elif self.bisectAttempt == False:
            print("Bug: No bisection attempt", flush=True)
            return False
        return True

    def print_basic(self):
        print("Name:      ", self.name)
        print("Hash:      ", self.hash)
        print("Link:      ", self.link)
    
    def print(self):
        self.print_basic()
        print("32-bit:    ", self.bit32)
        print("TrueFind:  ", self.truefind)
        print("Crashes:   ", len(self.crashes))
        print("Repros:    ", len(self.reproducers()))
        print("Patches:   ", len(self.fixCommits))
        print("Guilties:  ", len(self.guiltyCommits))
        print("Bisection: ", self.bisectAttempt)

    def print_all(self):
        self.print()
        for c in self.crashes:
            print()
            c.print()

spacer = "=========================================================================================="

outfile = "bugs.csv"

syzbotlink = "https://syzkaller.appspot.com"

def sort_date(e):
    return e.date

def fetch_link(l):
    doagain = True
    while doagain:
        doagain = False
        print("Fetching:", l, flush=True)
        time.sleep(1)
        try: rawpage = urlopen(l)
        except URLError as e:
            if (e.code == 429):
                doagain = True
                time.sleep(15)
            else:
                print("Failed to fetch link", flush=True)
                return None
    return html.fromstring(rawpage.read())

# Check that the given arrays are the same length
def check_lengths(arrays):
    for a in arrays[1:]:
        if len(a) != len(arrays[0]):
            return False
        
    return True

def transpose_crash_entries(times, kernels, configs, repros, managers) -> List[CrashEntry]:
    if not check_lengths([times, kernels, configs, repros, managers]):
        print("Warning: Array lengths are mismatched!", len(times), len(kernels), len(configs), len(repros), len(managers))
        return []

    crashes = []
    for i in range(len(kernels)):
        crashes.append(CrashEntry(kernels[i], configs[i], repros[i], managers[i], times[i].split(" ")[0]))

    return crashes

def get_link_arr(link_elements, prepend = "") -> List[str]:
    links = []

    for x in link_elements:
        if len(x.xpath("a/@href")) == 1:
            links.append(prepend + x.xpath("a/@href")[0])
        elif len(x.xpath("a/@href")) > 1:
            print("Warning: Found more than one link in crash entry!")
            links.append(prepend + x.xpath("a/@href")[0])
        else:
            links.append("")
    
    return links

def filter_crashes_manager(crashes : List[CrashEntry]) -> List[CrashEntry]:
    newcrashes = []

    for c in crashes:
        if "arm" not in c.manager and "riscv" not in c.manager and "386" not in c.manager:
            newcrashes.append(c)
    
    return newcrashes

def filter_crashes_kernel(crashes : List[CrashEntry]) -> List[CrashEntry]:
    newcrashes = []

    for c in crashes:
        if "https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/" in c.kernel:
            newcrashes.append(c)
    
    return newcrashes

def filter_crashes_repro(crashes : List[CrashEntry]) -> List[CrashEntry]:
    newcrashes = []

    for c in crashes:
        if c.repro != "":
            newcrashes.append(c)
    
    return crashes

def parse_kernel_commit(html, link):
    date = html.xpath("//table[@class='commit-info']/tr/th[contains(text(), 'committer')]/following-sibling::td[@class='right']/text()")
    if len(date) != 1:
        print("Warning: Failed to parse kernel commit date")
        print("Link:", link)
        print("Date:", date)
        return None

    return Commit(link.split("=")[-1], link, date[0].split(" ")[0])

def fetch_kcommit_date(link : str) -> date:
    kcommit_html = fetch_link(link)
    kcommit_dates = kcommit_html.xpath("//body/div[@id='cgit']/div[@class='content']/table[@class='commit-info']/tr/td[@class='right']/text()")
    if (len(kcommit_dates) < 2 or len(kcommit_dates[1]) < 10):
        return date.fromisoformat("1990-01-01") # yeah, I know. key value bad
    kcommit_date = date.fromisoformat(kcommit_dates[1][:10])
    return kcommit_date

def link2hash(link : str):
    hsh = link.split('=')[-1]
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
    bugname1 = bugname.split(' ')[0]
    if (bugname.split(' ')[-1][-1] == ')'):
        bugname2 = bugname.split(' ')[-2]
    else:
        bugname2 = bugname.split(' ')[-1]
    for line in loglines:
        if (line.startswith("testing commit")):
            cur_hash = line.split(' ')[2]
            continue
        if (line.startswith("run #") or line.startswith("all runs:")):
            if (bugname1 in line and bugname2 in line): # check for function name and sanitizer. Should be good enough
                best_hash = cur_hash

    if (len(best_hash) > 0):
        result.best_hash = best_hash
        best_link = "https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id="+best_hash
        result.best_date = fetch_kcommit_date(best_link).isoformat()
    else:
        print("The bug was never found in the bisect log", flush=True)
    
    return result

def repro_link(bughtml : html) -> str:
    reprolink = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/a[text() ='syz']/@href")
    if len(reprolink) == 0:
        print("Failed to find syz repro from bisection result", flush=True)
        return ""
    return syzbotlink + reprolink[0]

def handle_good_result(line : str, bughtml : html, bug : Bugdata) -> Bisect_Result:
    bug.bisectResult.converge = "good"
    bug.bisectResult.syz_repro = repro_link(bughtml)
    
    kcommit_link = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/span[@class='mono']/a/@href")
    if (len(kcommit_link) == 0):
        print("Failed to find bisect commit link", flush=True)
        bug.bisectResult.err = "parse failure"
        return bug.bisectResult

    kcommit_link = kcommit_link[0]
    bisect_hash = link2hash(kcommit_link)
    bug.bisectResult.hash = bisect_hash
    bisect_date = fetch_kcommit_date(kcommit_link)
    if (bisect_date.isoformat() == "1990-01-01"):
        print("Failed to find bisect commit dates", flush=True)
        bug.bisectResult.err = "parse failure"
        return bug.bisectResult

    bug.bisectResult.date = bisect_date.isoformat()

    bug.bisectResult = bisect_log_best_guess(bug.name, bughtml, bug.bisectResult)

    if (bisect_hash == bug.guiltyCommits[0].hash):
        bug.bisectResult.correct = "correct"
    else:
        bug.bisectResult.correct = "incorrect"

    return bug.bisectResult

def handle_multiple_result(line : str, bughtml : html, bug : Bugdata) -> Bisect_Result:
    bug.bisectResult.converge = "multiple"
    bug.bisectResult.syz_repro = repro_link(bughtml)
    linklist = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/span[@class='mono']/a/@href")
    if (len(linklist) == 0):
        print("Failed to find bisect commit link", flush=True)
        bug.bisectResult.err = "parse failure"
        return bug.bisectResult

    hashes = []
    for link in linklist:
        hashes.append(link2hash(link))
    
    bug.bisectResult = bisect_log_best_guess(bug.name, bughtml, bug.bisectResult)

    if (bug.guiltyCommits[0].hash in hashes):
        bug.bisectResult.date = bug.guiltyCommits[0].date
        bug.bisectResult.correct = "correct"
        bug.bisectResult.hash = bug.guiltyCommits[0].hash
    else:
        bisect_date = fetch_kcommit_date(linklist[0])
        bug.bisectResult.date = bisect_date.isoformat()
        bug.bisectResult.correct = "incorrect"
        bug.bisectResult.hash = link2hash(linklist[0])

    return bug.bisectResult

def handle_error_result(line : str, bughtml : html, bug : Bugdata) -> Bisect_Result:
    bug.bisectResult.converge = "error"
    errlog_link = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/b[2]/a[1]/@href")
    if (len(errlog_link) < 1):
        print("Failed to find bisect error log", flush=True)
        bug.bisectResult.err = "parse failure"
        return bug.bisectResult

    errlog_link = errlog_link[0]
    errlog_html = fetch_link(errlog_link)
    errmsg = errlog_html.xpath("/html/body/p/text()")
    if (len(errmsg) < 1):
        print("Failed to read bisect error log", flush=True)
        bug.bisectResult.err = "parse failure"
        return bug.bisectResult

    errmsg = errmsg[0].split('\n')[0].split(',')[0]
    bug.bisectResult.err = errmsg
    # taking too long, oldest tested release
    if ("taking too long" in errmsg or "oldest tested release" in errmsg):
        bug.bisectResult = bisect_log_best_guess(bug.name, bughtml, bug.bisectResult)

    bug.bisectResult.correct = "incorrect"
    return bug.bisectResult

def handle_oldest_result(line : str, bughtml : html, bug : Bugdata) -> Bisect_Result:
    bug.bisectResult.converge = "error"
    bug.bisectResult.err = "the issue happens on the oldest tested release"
    bug.bisectResult.syz_repro = repro_link(bughtml)
    bug.bisectResult = bisect_log_best_guess(bug.name, bughtml, bug.bisectResult)
    return bug.bisectResult

def fetch_bug(bug : Bugdata) -> Bugdata:
    bughtml = fetch_link(bug.link)

    # fetch name
    bug.name = bughtml.xpath("//body/b/text()")[0]
    if bug.name == "":
        print("Bug: Failed to get name.")
        return None

    # fetch the crash entries
    times = bughtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='time']/text()")
    kernels = bughtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='tag']")[0::2]
    kernels = get_link_arr(kernels)
    configs = bughtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='config']/a/@href")
    repros = bughtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='config']/following-sibling::td[3][contains(@class, 'repro')]")
    repros = get_link_arr(repros, syzbotlink)
    managers = bughtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='manager'][starts-with(text(), 'ci') or starts-with(text(), 'skylake')]/text()")
    bug.crashes = transpose_crash_entries(times, kernels, configs, repros, managers)

    if len(bug.valid_crashes()) == 0:
        bug.print()
        print(bug.name + ": No valid anchor.")
        return None

    bug.crashes.sort(key=sort_date)
    bug.truefind = bug.crashes[0].date.split(" ")[0]
    bug.anchor = bug.anchor_crash()

    if "386" in bug.anchor.manager:
        bug.bit32 = "i386"
    else:
        bug.bit32 = "amd64"

    fixlinks = bughtml.xpath("//body/span[@class='mono']/a/@href")
    guiltyhashes = []
    for l in fixlinks:
        fixhtml = fetch_link(l)
        kcommit = parse_kernel_commit(fixhtml, l)
        if kcommit != None:
            bug.fixCommits.append(kcommit)
        else:
            continue

        fixtext = fixhtml.xpath("//div[@class='commit-msg']/text()")
        fixtext = "\n".join(fixtext).split("\n")
        for line in fixtext:
            if line.startswith("Fixes:"):
                hash = line.split(" ")[1]
                if len(hash) >= 12 and all(c in string.hexdigits for c in hash):
                    guiltyhashes.append(hash)

    bug.fixCommits.sort(reverse=True, key=sort_date)
    if len(bug.fixCommits) == 0:
        bug.print()
        print(bug.name + ": No valid fixes.")
        return None

    for hash in guiltyhashes:
        # I think there is some issue here with too many requests or timeout or something. A problem for a later date.
        searchhtml = fetch_link("https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/log/?qt=range&q=" + hash)
        searchres = searchhtml.xpath("//table[@class='list nowrap']/tr[2]/td[1]/span/text()")
        if len(searchres) <= 0:
            bug.print()
            print(bug.name + ": Guilty Commit Search Failed.")
            return None
        guiltydate = searchres[0]
        guiltylink = "https://git.kernel.org" + searchhtml.xpath("//table[@class='list nowrap']/tr[2]/td[2]/a/@href")[0]
        bug.guiltyCommits.append(Commit(guiltylink.split("=")[-1], guiltylink, guiltydate))
    
    bug.guiltyCommits.sort(key=sort_date)
    if len(bug.guiltyCommits) == 0:
        bug.print()
        print(bug.name + ": No valid guilty commits.")
        return None

    # fetch bisection result
    bisect_result = bughtml.xpath("//body/div/div[@class='bug-bisection-info']/b[1]/text()")
    if len(bisect_result) == 0:
        bug.bisectResult.converge = "none"
        bug.bisectAttempt = False
    else:
        bisect_result = bisect_result[0]
        bug.bisectAttempt = True
        if "Cause bisection:" not in bisect_result:
            print("Failed to parse bisection result: " + bisect_result, flush=True)
            bug.bisectResult.err = "parse failure"
        else:
            if "introduced by" in bisect_result:
                bug.bisectResult = handle_good_result(line, bughtml, bug)

            elif "the cause commit could be any of" in bisect_result:
                bug.bisectResult = handle_multiple_result(line, bughtml, bug)

            elif "failed" in bisect_result:
                bug.bisectResult = handle_error_result(line, bughtml, bug)
            
            elif "oldest tested release" in bisect_result:
                bug.bisectResult = handle_oldest_result(line, bughtml, bug)

            else:
                print("Unknown bisection result:", bisect_result, flush=True)
                bug.bisectResult.err = "unknown result"

    bug.print()
    if bug.validate():
        print()
        print("Anchor: (Finding Commit)")
        bug.anchor_crash().print()
        print()
        print("Patch:")
        bug.fixCommits[0].print()
        print()
        print("Guilty Commit:", flush=True)
        bug.guiltyCommits[0].print()
        print()
        print("Bisection:", flush=True)
        bug.bisectResult.print()
        return bug

    print(bug.name + ": Failed Validation.")
    return None

def links2bugs(filename : str) -> List[Bugdata]:
    file = open(filename, 'r')
    bugs = []
    for line in file:
        bugs.append(Bugdata(line[:-1]))
    file.close()
    return bugs

def bug2csv(bug : Bugdata) -> str:
    # bug.bit32 is left out of this because they are all amd64
    return ",".join([str(bug.number), bug.link, bug.name, bug.truefind, bug.anchor.config, bug.anchor.kernel, bug.anchor.date, bug.fixCommits[0].link, bug.fixCommits[0].date, bug.guiltyCommits[0].link, bug.guiltyCommits[0].date, bug.bisectResult.converge, bug.bisectResult.hash, bug.bisectResult.date, bug.bisectResult.best_hash, bug.bisectResult.best_date, bug.bisectResult.syz_repro, bug.bisectResult.err, " ".join(bug.reproducers())]) + "\n"
    #               $1                $2        $3        $4            $5                 $6                 $7               $8                      $9                      $10                        $11                        $12                        $13                    $14                    $15                         $16                         $17                         $18                   $19

def main():
    # read list of bug links
    bugs = links2bugs("bisect-links.csv")
    print(len(bugs), " bug-links found")

    if (len(bugs) == 0):
        os._exit(os.EX_DATAERR)

    # For loop of fetching bugs might catch a slowdown from the server. Maybe have a delay to play nice.
    file = open(outfile, 'w')
    count = 1
    for b in bugs:
        print()
        print(spacer)
        print()
        bug = fetch_bug(b)
        if bug != None:
            bug.number = count
            count = count + 1
            file.write(bug2csv(bug))
            file.flush()

    file.close()
    os._exit(os.EX_OK)

if __name__ == "__main__":
    main()
