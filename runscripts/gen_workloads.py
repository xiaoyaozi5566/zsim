#!/usr/bin/python

import os
import sys
import time
import random

# remove astar due to bugs
specint = ['perlbench', 'bzip2', 'gcc', 'mcf', 'gobmk', 'hmmer', 'sjeng', 'libquantum', 'h264ref', 'omnetpp', 'xalan']
# remove calculix, povray, wrf due to bugs
specfp = ['bwaves', 'gamess', 'milc', 'zeusmp', 'gromacs', 'cactusADM', 'leslie3d', 'namd', 'dealII', 'soplex', 'GemsFDTD', 'tonto', 'lbm']

singleprog = specint + specfp

num_progs = int(sys.argv[1])

workloads = []

for i in range(10):
    workload = []
    for i in range(num_progs):
        rand_num = random.randint(0, len(singleprog)-1)
        workload.append(singleprog[rand_num])
    print workload, ","