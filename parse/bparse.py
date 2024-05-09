import os
import time
from typing import List
from urllib.request import urlopen
from lxml import html

# Crawl Syzbot for patched bugs that fit criteria

class Commit:
    def __init__(self, h, l, d):
        self.hash = h                       # commit hash
        self.link = l                       # commit link
        self.date = d                       # commit date
    
    def is_upstream(self) -> bool:
        return ("https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/" in self.link)

    def print(self):
        print("Hash:     ", self.hash)
        print("Link:     ", self.link)
        print("Upstream: ", self.is_upstream())
        print("Date:     ", self.date)

class CrashEntry:
    def __init__(self, k, c, r, m, d):
        self.date = d                       # date of crash
        self.kernel = k                     # kernel commit
        self.config = syzbotlink + c        # config link
        self.repro = r                      # reproducer link
        self.manager = m                    # manager name

    def get_commit(self):
        pass

    def print(self):
        print("Date:     ", self.date)
        print("Kernel:   ", self.kernel)
        print("Config:   ", self.config)
        print("Repro:    ", self.repro)
        print("Manager:  ", self.manager)

class Bugdata:
    def __init__(self, n, l, r):
        self.name = n                       # bug name
        self.link = l                       # bug link
        self.hash = l.split("=")[-1]        # bug hash
        self.hasrepro = r                   # has repro?
        self.bit32 = ""                     # is the crash 32-bit
        self.crashes = []                   # list of valid crashes (anything with a repro we can use)
        self.anchor = None                  # from the earliest upstream crash
        self.truefind = ""                  # the date the bug was first found (out of all crashes)
        self.fixCommits = []                # the latest commit that links the guilty commit
        self.guiltyCommits = []             # the earliest guilty commit
    
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
            print("Bug: Bad name")
            return False
        elif self.link == "":
            print("Bug: Bad link")
            return False
        elif self.hash == "":
            print("Bug: Bad hash")
            return False
        elif self.hasrepro == False or len(self.reproducers()) == 0:
            print("Bug: No reproducer")
            return False
        elif self.bit32 == "":
            print("Bug: Bad 32-bit decision")
            return False
        elif len(self.crashes) == 0:
            print("Bug: No crashes")
            return False
        elif self.anchor == None:
            print("Bug: No valid anchor")
            return False
        elif self.truefind == "":
            print("Bug: Bad truefind date")
            return False
        elif len(self.fixCommits) == 0:
            print("Bug: No fixing commits")
            return False
        elif len(self.guiltyCommits) == 0:
            print("Bug: No guilty commits")
            return False
        return True

    def print_basic(self):
        print("Name:     ", self.name)
        print("Hash:     ", self.hash)
        print("Link:     ", self.link)
    
    def print(self):
        self.print_basic()
        print("32-bit:   ", self.bit32)
        print("TrueFind: ", self.truefind)
        print("Crashes:  ", len(self.crashes))
        print("Repros:   ", len(self.reproducers()))
        print("Patches:  ", len(self.fixCommits))
        print("Guilties: ", len(self.guiltyCommits))

    def print_all(self):
        self.print()
        for c in self.crashes:
            print()
            c.print()

    def log(self):
        pass
        # TODO

spacer = "=========================================================================================="

outfile = "bugs-2023.csv"

syzbotlink = "https://syzkaller.appspot.com"
syzbotfixedlink = syzbotlink + "/upstream/fixed"

def sort_date(e):
    return e.date

# Check that the given arrays are the same length
def check_lengths(arrays):
    for a in arrays[1:]:
        if len(a) != len(arrays[0]):
            return False
        
    return True

# Take parallel arrays of bug data and transpose to a single array of structs
def transpose_bug_entries(names, links, repros) -> List[Bugdata]:
    data = []

    if not check_lengths([names, links, repros]):
        print("Warning: Array lengths are mismatched!", len(names), len(links), len(repros))
        return []

    for i in range(len(names)):
        data.append(Bugdata(names[i], syzbotlink + links[i], (repros[i].text != None)))

    return data

def transpose_crash_entries(times, kernels, configs, repros, managers) -> List[CrashEntry]:
    if not check_lengths([times, kernels, configs, repros, managers]):
        print("Warning: Array lengths are mismatched!", len(times), len(kernels), len(configs), len(repros), len(managers))
        return []

    crashes = []
    for i in range(len(kernels)):
        crashes.append(CrashEntry(kernels[i], configs[i], repros[i], managers[i], times[i].split(" ")[0]))

    return crashes

# filter out the bugs that do not have reproducers
def filter_bugs_repro(bugs : List[Bugdata]) -> List[Bugdata]:
    newbugs = []

    for bug in bugs:
        if bug.hasrepro:
            newbugs.append(bug)
    
    return newbugs

def filter_bugs(bugs : List[Bugdata]) -> List[Bugdata]:
    newbugs = bugs

    newbugs = filter_bugs_repro(newbugs)

    return newbugs

def get_repros_arr(repro_elements) -> List[str]:
    repros = []

    for x in repro_elements:
        if len(x.xpath("a/@href")) == 1:
            repros.append(syzbotlink + x.xpath("a/@href")[0])
        elif len(x.xpath("a/@href")) > 1:
            print("Warning: Found more than one reproducer link in crash entry!")
            repros.append(syzbotlink + x.xpath("a/@href")[0])
        else:
            repros.append("")
    
    return repros

def filter_crashes_manager(crashes : List[CrashEntry]) -> List[CrashEntry]:
    newcrashes = []

    for c in crashes:
        if "arm" not in c.manager and "riscv" not in c.manager:
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

def fetch_link(l):
    rawpage = urlopen(l)
    return html.fromstring(rawpage.read())

def parse_kernel_commit(html, link):
    date = html.xpath("//table[@class='commit-info']/tr/th[contains(text(), 'committer')]/following-sibling::td[@class='right']/text()")
    if len(date) != 1:
        print("Warning: Failed to parse kernel commit date")
        print("Link:", link)
        print("Date:", date)
        return None

    return Commit(link.split("=")[-1], link, date[0].split(" ")[0])

def fetch_bug(bug : Bugdata) -> Bugdata:
    bughtml = fetch_link(bug.link)

    times = bughtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='time']/text()")
    kernels = bughtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='tag']/a/@href")[0::2]
    configs = bughtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='config']/a/@href")
    repros = bughtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='config']/following-sibling::td[3][@class='repro']")
    repros = get_repros_arr(repros)
    managers = bughtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='manager'][starts-with(text(), 'ci')]/text()")

    bug.crashes = transpose_crash_entries(times, kernels, configs, repros, managers)

    if len(bug.valid_crashes()) == 0:
        bug.print()
        print("Bug: No valid anchor.")
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
                guiltyhashes.append(line.split(" ")[1])

    bug.fixCommits.sort(reverse=True, key=sort_date)

    for hash in guiltyhashes:
        searchhtml = fetch_link("https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/log/?qt=range&q=" + hash)
        guiltydate = searchhtml.xpath("//table[@class='list nowrap']/tr[2]/td[1]/span/text()")[0]
        guiltylink = "https://git.kernel.org" + searchhtml.xpath("//table[@class='list nowrap']/tr[2]/td[2]/a/@href")[0]
        bug.guiltyCommits.append(Commit(guiltylink.split("=")[-1], guiltylink, guiltydate))
    
    bug.guiltyCommits.sort(key=sort_date)

    bug.print()
    if bug.validate():
        print()
        print("Anchor: (Finding Commit)")
        bug.anchor_crash().print()
        print()
        print("Patch:")
        bug.fixCommits[0].print()
        print()
        print("Guilty Commit:")
        bug.guiltyCommits[0].print()
        return bug

    return None

def main():
    # https://realpython.com/python-web-scraping-practical-introduction/
    rawpage = urlopen(syzbotfixedlink)
    # https://docs.python-guide.org/scenarios/scrape/
    syzbothtml = html.fromstring(rawpage.read())
    # https://scrapfly.io/blog/parsing-html-with-xpath/
    bugnames = syzbothtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='title']/a/text()")
    buglinks = syzbothtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='title']/a/@href")
    bugrepros = syzbothtml.xpath("//body/table[@class='list_table']/tbody/tr/td[@class='title']/following-sibling::td[1][@class='stat']")

    # transpose parallel arrays to structures for ease of use
    bugs = transpose_bug_entries(bugnames, buglinks, bugrepros)
    print(len(bugs), " bugs found")
    bugs = filter_bugs(bugs)
    print(len(bugs), " bugs passed first filter")

    if (len(bugs) == 0):
        os._exit(os.EX_DATAERR)

    # For loop of fetching bugs might catch a slowdown from the server. Maybe have a delay to play nice.
    file = open(outfile, 'w')
    for b in bugs:
        print()
        print(spacer)
        print()
        bug = fetch_bug(b)
        if bug != None:
            file.write(",".join([bug.link, bug.name, bug.truefind, bug.fixCommits[0].link, bug.fixCommits[0].date, bug.anchor.config, bug.anchor.kernel, bug.anchor.date, bug.guiltyCommits[0].link, bug.guiltyCommits[0].date, bug.bit32, " ".join(bug.reproducers())]) + "\n")
        time.sleep(1)

    file.close()
    os._exit(os.EX_OK)

if __name__ == "__main__":
    main()
