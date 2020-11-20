#!/usr/bin/env python
import os

bin_folder = "bin/"
time_duration = "20"   # duration of each run in seconds
num_runs = "5";

# Names for linked lists in set-ll-1k
ll_name_list = [
    "mh-hp",        # Michael-Harris with Hazard Pointers
    "mh-ptb",       # Michael-Harris with Pass The Buck
    "mh-ptp",       # Michael-Harris with Pass The Pointer
    "mh-orc",       # Michael-Harris with OrcGC
    "ho-orc",       # Harris original with OrcGC
    "hsh-orc",      # Herlihy-Shavit-Harris with OrcGC
    "tbkp-orc",     # Timant-Braginsky-Kogan-Petrank with OrcGC
]

# Names for linked lists in set-tree-1m
tree_name_list = [
    "nata-hp",        # Natarajan-Mittal with Hazard Pointers
    "nata-ptb",       # Natarajan-Mittal with Pass The Buck
    "nata-ptp",       # Natarajan-Mittal with Pass The Pointer
    "nata-orc",       # Natarajan-Mittal with OrcGC
]

# Names for skiplists in set-skiplist-1m
skiplist_name_list = [
    "hsskip-orcorig", # Original Herlihy Shavit skiplist with OrcGC
    "hsskip-orc",     # Herlihy Shavit skiplist with poison with OrcGC
]     


print "\n\n+++ Running concurrent microbenchmarks +++\n"

os.system(bin_folder+"q-ll-enq-deq ")

for dsname in ll_name_list:
    os.system(bin_folder+"set-ll-1k "+ dsname +" "+ time_duration +" " +num_runs)

for dsname in tree_name_list:
    os.system(bin_folder+"set-tree-1m "+ dsname +" "+ time_duration +" " +num_runs)

for dsname in skiplist_name_list:
    os.system(bin_folder+"set-skiplist-1m "+ dsname +" "+ time_duration +" " +num_runs)
