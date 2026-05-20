# Results

These are the raw results used in "Meerkat: Pushing the Limits of Dynamic Bisection with PoC Mutation."
The raw bisection logs are found in each of the directories here based on which tool was run and with which features.

## How The Results Were Gathered

### Meerkat (`mk/`)

These results reflect the fully fledged Meerkat tool.
Meerkat was run with the features: `ff-test,poc-test,poc-all-pocs,stateful-corpus,ff-no-find-backup` (Note: the functionality of some features changed as Meerkat was developed. The exact logs may vary).
Here, `ff-no-find-backup` means that if the bug is not found using mutation at the anchor commit, it should not fall back to using PoCs.
This is because this result is already handled by Mk_nm, so those results can be concatenated.

An example command for running this is as follows:
```
./mk-manager.sh -s 1 -i 1 -b parse/bugs.csv -F ff-test,poc-test,poc-all-pocs,stateful-corpus,ff-no-find-backup
```

### Meerkat 1p and nm (`mk_t_poc/`)

These results cover both Mk_1p and Mk_nm.
This is because the result for Mk_1p is simply the result after the first PoC of Mk_nm, which is observable in the logs.
These cases were wun with the features: `poc-test,poc-all-pocs`.

An example command for running this is as follows:
```
./mk-manager.sh -s 1 -i 1 -b parse/bugs.csv -F poc-test,poc-all-pocs
```

### Meerkat m1p (`mk_m1p`)

These are the results for Meerkat with mutation enabled, but only 1 PoC.
It only contains 111 bugs as the rest only had 1 PoC to begin with, and as such did not need to be rerun.
These bugs were run with the features: `ff-test,stateful-corpus,poc-test,ff-no-find-backup`.
Again, `ff-no-find-backup` could be used since Mk_1p covers the cases where the bug is not found using mutation at the anchor commit.

An example command for running this is as follows:
```
./mk-manager.sh -s 1 -i 1 -b parse/bugs.csv -F ff-test,stateful-corpus,poc-test,ff-no-find-backup
```

### 20-minute Timeout (`20-minute/`)

These are the results from testing whether expanding the timeout to 20 minutes would have any affect on the accuray of Meerkat.
They use the same features as Mk, but with `-m 20` passed as well.

An example command for running this is as follows:
```
./mk-manager.sh -s 1 -i 1 -b parse/bugs.csv -F ff-test,poc-test,poc-all-pocs,stateful-corpus,ff-no-find-backup -m 20
```

### Syz-bisect (`sb/`)

These results were gathered by running Syz-bisect as-is on each of the bugs.

### `corrected-ground-truth.csv`

This file shows the results of our manual effort to correct the BiC of the bugs in our dataset.

### `inter-report-dedup.csv`

This file shows groups of known inter-report duplicate bugs and whether Meerkat is able to identify them as duplicates, as well as whether that would have any effect on our results.
Known duplicates were gathered base on patch.

### `semantic-results.csv`

This file contains the results of each of our tested tools.
The most important part is whether they reached the BiC or not.
This was gathered by comparing the bisected commit of each tool/bug to the corrected BiC.
If the commits did not match, we performed additional manual analysis to determine what changed to block the tool from progressing (i.e. Code Change, Blocking Bug, etc.).

## Results by Figure

We will now go over how the results shown in the paper were extracted from the raw data.
(As the paper is prepared for the final version, this may update.)

### Table 1

Table 1 is a simple reflection of the results in `corrected-ground-truth.csv`, which were obtained through manual analysis.

### Figure 6

Figure 6 shows the number of times each tool reached the BiC, which is gathered from `semantic-results.csv`.

### Figure 7

Figure 7 shows the aggregate runtimes of each tool, which is gathered by aggregating the runtimes shown in `time-plus-semantic.csv`.

### Figure 8

Figure 8 shows the results reached by Meerkat and why the run did not proceed further back in time.
This data is gathered from `semantic-results.csv`.
