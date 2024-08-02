def main():
    file1 = "bugs.csv"
    file2 = "bugs-other.csv"
    file3 = "bugs-unique.csv"

    print("Finding all bugs in", file1, "that are not in", file2)

    newfile = open(file1, 'r')
    newlines = newfile.readlines()
    newfile.close()

    oldfile = open(file2, 'r')
    oldlines = oldfile.readlines()
    oldfile.close()

    bugnames = []
    for l in oldlines:
        if l != "":
            bugnames.append(l.split(",")[1])

    uniquelines = []
    for l in newlines:
        if l != "" and l.split(",")[1] not in bugnames:
            uniquelines.append(l)
    
    uniquefile = open(file3, 'w')
    uniquefile.writelines(uniquelines)
    uniquefile.close()
    print(len(uniquelines), "bugs written to", file3)

if __name__ == "__main__":
    main()
