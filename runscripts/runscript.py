#!/usr/bin/python

import os
import sys
import time

zsim_home = "/home/yw438/zsim"
spec_dir = "/home/yw438/benchmarks"
parsec_dir = "/home/yw438/parsec/parsec-2.1/pkgs/apps"
parsec_kernel = "/home/yw438/parsec/parsec-2.1/pkgs/kernels"
scriptgen_dir = zsim_home + "/scriptgen"
results_dir = zsim_home + "/results"
stdout_dir = zsim_home + "/stdout"
stderr_dir = zsim_home + "/stderr"

techIni = "/home/yw438/zsim/DRAMSim2/ini/DDR3_micron_64M_8B_x4_sg15.ini"
systemIni_sec = "/home/yw438/zsim/DRAMSim2/system.ini.sec"
systemIni_sec2banks = "/home/yw438/zsim/DRAMSim2/system.ini.sec2banks"
systemIni_secrand = "/home/yw438/zsim/DRAMSim2/system.ini.rand"
systemIni_insec = "/home/yw438/zsim/DRAMSim2/system.ini.baseline"
systemIni_fs = "/home/yw438/zsim/DRAMSim2/system.ini.fs"
systemIni_bankpar = "/home/yw438/zsim/DRAMSim2/system.ini.bankpar"
systemIni_rankpar = "/home/yw438/zsim/DRAMSim2/system.ini.rankpar"
systemIni_smart = "/home/yw438/zsim/DRAMSim2/system.ini.smart"
systemIni_relax = "/home/yw438/zsim/DRAMSim2/system.ini.relax"
systemIni_mix = "/home/yw438/zsim/DRAMSim2/system.ini.mix"
systemIni_prob = "/home/yw438/zsim/DRAMSim2/system.ini.prob"
systemIni_probmix = "/home/yw438/zsim/DRAMSim2/system.ini.probmix"
systemIni_limit = "/home/yw438/zsim/DRAMSim2/system.ini.limit"
systemIni_dynamic="/home/yw438/zsim/DRAMSim2/system.ini.dynamic"

# remove astar due to bugs
specint = ['perlbench', 'bzip2', 'gcc', 'mcf', 'gobmk', 'hmmer', 'sjeng', 'libquantum', 'h264ref', 'omnetpp', 'xalan']

# remove calculix, povray, wrf due to bugs
specfp = ['bwaves', 'gamess', 'milc', 'zeusmp', 'gromacs', 'cactusADM', 'leslie3d', 'namd', 'dealII', 'soplex', 'GemsFDTD', 'tonto', 'lbm']

# parsec benchmarks
parsec = ['blackscholes', 'bodytrack', 'facesim', 'ferret', 'fluidanimate', 'freqmine', 'swaptions', 'vips']

specinvoke = {
    'perlbench'  : spec_dir + "/perlbench -I" + spec_dir + "/lib " + spec_dir + "/checkspam.pl 2500 5 25 11 150 1 1 1 1",
    'bzip2'      : spec_dir + "/bzip2 " + spec_dir + "/input.source 280",
    'gcc'        : spec_dir + "/gcc " + spec_dir + "/200.in -o results/200.s",
    'mcf'        : spec_dir + "/mcf " + spec_dir + "/inp.in",
    'gobmk'      : spec_dir + "/gobmk --quiet --mode gtp",
    'hmmer'      : spec_dir + "/hmmer " + spec_dir + "/nph3.hmm " + spec_dir + "/swiss41",
    'sjeng'      : spec_dir + "/sjeng " + spec_dir + "/ref.txt",
    'libquantum' : spec_dir + "/libquantum 1397 8",
    'h264ref'    : spec_dir + "/h264ref -d " + spec_dir + "/foreman_ref_encoder_baseline.cfg",
    'omnetpp'    : spec_dir + "/omnetpp " + spec_dir + "/omnetpp.ini",
    # 'astar'      : spec_dir + "/astar " + spec_dir + "/BigLakes2048.cfg",
    'xalan'      : spec_dir + "/Xalan -v " + spec_dir + "/t5.xml " + spec_dir + "/xalanc.xsl",
    'bwaves'     : spec_dir + "/bwaves", 
    'gamess'     : spec_dir + "/gamess",
    'milc'       : spec_dir + "/milc",
    'zeusmp'     : spec_dir + "/zeusmp",
    'gromacs'    : spec_dir + "/gromacs -silent -deffnm gromacs -nice 0",
    'cactusADM'  : spec_dir + "/cactusADM " + spec_dir + "/benchADM.par",
    'leslie3d'   : spec_dir + "/leslie3d",
    'namd'       : spec_dir + "/namd --input " + spec_dir + "/namd.input --iterations 38 --output results/namd.out",
    'dealII'     : spec_dir + "/dealII 23",
    'soplex'     : spec_dir + "/soplex -sl -e -m45000 " + spec_dir + "/pds-50.mps",
    'povray'     : spec_dir + "/povray " + spec_dir + "/SPEC-benchmark-ref.ini",
    'calculix'   : spec_dir + "/calculix -i " + spec_dir + "/hyperviscoplastic",
    'GemsFDTD'   : spec_dir + "/GemsFDTD",
    'tonto'      : spec_dir + "/tonto",
    'lbm'        : spec_dir + "/lbm 3000 results/reference.dat 0 0" + spec_dir + "/100_100_130_ldc.of",
    'wrf'        : spec_dir + "/wrf",
    # 'sphinx3'    : spec_dir + "/sphinx_livepretend ctlfile . args.an4",
}

redirect_input = {
    'gobmk'      : spec_dir + "/13x13.tst",
    'gamess'     : spec_dir + "/cytosine.2.config",
    'milc'       : spec_dir + "/su3imp.in",
    'leslie3d'   : spec_dir + "/leslie3d.in",
}

# # parsec simlarge
# parsecinvoke = {
#     'blackscholes' : parsec_dir + "/blackscholes/inst/amd64-linux.gcc/bin/blackscholes 4 " + parsec_dir + "/blackscholes/inputs/in_64K.txt " + parsec_dir + "/blackscholes/run/prices.txt",
#     'bodytrack'    : parsec_dir + "/bodytrack/inst/amd64-linux.gcc/bin/bodytrack " + parsec_dir + "/bodytrack/inputs/sequenceB_4 4 4 4000 5 0 4",
#     'facesim'      : parsec_dir + "/facesim/inst/amd64-linux.gcc/bin/facesim -timing -threads 4",
#     'ferret'       : parsec_dir + "/ferret/inst/amd64-linux.gcc/bin/ferret " + parsec_dir + "/ferret/inputs/corel lsh " + parsec_dir + "/ferret/inputs/queries 10 20 4 " + parsec_dir + "/ferret/run/output.txt",
#     'fluidanimate' : parsec_dir + "/fluidanimate/inst/amd64-linux.gcc/bin/fluidanimate 4 5 " + parsec_dir + "/fluidanimate/inputs/in_300K.fluid " + parsec_dir + "/fluidanimate/run/out.fluid",
#     'freqmine'     : parsec_dir + "/freqmine/inst/amd64-linux.gcc/bin/freqmine " + parsec_dir + "/freqmine/inputs/kosarak_990k.dat 790",
#     'swaptions'    : parsec_dir + "/swaptions/inst/amd64-linux.gcc/bin/swaptions -ns 64 -sm 20000 -nt 4",
#     'vips'         : parsec_dir + "/vips/inst/amd64-linux.gcc/bin/vips im_benchmark " + parsec_dir + "/vips/inputs/bigben_2662x5500.v " + parsec_dir + "/vips/run/output.v",
# }

# parsec simsmall
parsecinvoke = {
    'blackscholes' : parsec_dir + "/blackscholes/inst/amd64-linux.gcc/bin/blackscholes 4 " + parsec_dir + "/blackscholes/inputs/in_4K.txt " + parsec_dir + "/blackscholes/run/prices.txt",
    'bodytrack'    : parsec_dir + "/bodytrack/inst/amd64-linux.gcc/bin/bodytrack " + parsec_dir + "/bodytrack/inputs/sequenceB_1 4 1 1000 5 0 4",
    'canneal'      : parsec_kernel + "/canneal/inst/amd64-linux.gcc/bin/canneal 4 10000 2000 " + parsec_kernel + "/canneal/inputs/100000.nets 32",
    'facesim'      : parsec_dir + "/facesim/inst/amd64-linux.gcc/bin/facesim -timing -threads 4",
    'ferret'       : parsec_dir + "/ferret/inst/amd64-linux.gcc/bin/ferret " + parsec_dir + "/ferret/inputs/corel lsh " + parsec_dir + "/ferret/inputs/queries 10 20 4 " + parsec_dir + "/ferret/run/output.txt",
    'fluidanimate' : parsec_dir + "/fluidanimate/inst/amd64-linux.gcc/bin/fluidanimate 4 5 " + parsec_dir + "/fluidanimate/inputs/in_35K.fluid " + parsec_dir + "/fluidanimate/run/out.fluid",
    'freqmine'     : parsec_dir + "/freqmine/inst/amd64-linux.gcc/bin/freqmine " + parsec_dir + "/freqmine/inputs/kosarak_250k.dat 220",
    'swaptions'    : parsec_dir + "/swaptions/inst/amd64-linux.gcc/bin/swaptions -ns 16 -sm 5000 -nt 4",
    'streamcluster': parsec_kernel + "/streamcluster/inst/amd64-linux.gcc/bin/streamcluster 10 20 32 4096 4096 1000 none " + parsec_kernel + "/streamcluster/run/output.txt 4",
    'vips'         : parsec_dir + "/vips/inst/amd64-linux.gcc/bin/vips im_benchmark " + parsec_dir + "/vips/inputs/pomegranate_1600x1200.v " + parsec_dir + "/vips/run/output.v",
}

bench_name = {
    # spec
    'perlbench': 'per',
    'astar' : 'ast',
    'bzip2' : 'bzi',
    'gcc'   : 'gcc',
    'gobmk' : 'gob',
    'h264ref' : 'h264',
    'omnetpp': 'omn',
    'hmmer' : 'hmm',
    'libquantum' : 'lib',
    'mcf'   : 'mcf',
    'sjeng' : 'sje',
    'xalan' : 'xal',
    'bwaves': 'bwa',
    'gamess': 'gam',
    'milc'  : 'mil',
    'zeusmp': 'zeu',
    'gromacs': 'gro',
    'cactusADM': 'cac',
    'leslie3d': 'les',
    'namd'  : 'nam',
    'dealII': 'dea',
    'soplex': 'sop',
    'povray': 'pov',
    'calculix': 'cal',
    'GemsFDTD': 'gem',
    'tonto' : 'ton',
    'lbm'   : 'lbm',
    'wrf'   : 'wrf',
    # parsec
    'blackscholes' : 'bla',
    'bodytrack' : 'bod',
    'facesim' : 'fac',
    'ferret' : 'fer',
    'fluidanimate' : 'flu',
    'freqmine' : 'fre',
    'swaptions' : 'swa',
    'vips' : 'vip'
}

multiprog = [['astar', 'bzip2'],
             ['bzip2', 'astar'],
             ['astar', 'mcf'],
             ['mcf', 'astar'],
             ['libquantum', 'h264ref'],
             ['h264ref', 'libquantum'],
             ['libquantum', 'xalan'],
             ['xalan', 'libquantum'],
             ['astar', 'gobmk'],
             ['gobmk', 'astar'],
             ['libquantum', 'hmmer'],
             ['hmmer', 'libquantum'],
             ['bzip2', 'gcc'],
             ['gcc', 'bzip2'],
             ['h264ref', 'gobmk'],
             ['gobmk', 'h264ref'],
             ['mcf', 'hmmer'],
             ['hmmer', 'mcf'],
             ['xalan', 'sjeng'],
             ['sjeng', 'xalan'],
            ]

test = [['bzip2', 'gcc'],]

singleprog = specint + specfp

singleprog_parsec = parsec

multiprog = []

for i in range(len(singleprog)-1):
    multiprog.append([singleprog[i], singleprog[i+1]])

multiprog_8 = [
    ['gamess', 'libquantum', 'gcc', 'dealII', 'xalan', 'gromacs', 'zeusmp', 'bzip2'] ,
    ['tonto', 'leslie3d', 'gamess', 'gromacs', 'gamess', 'h264ref', 'mcf', 'bwaves'] ,
    ['cactusADM', 'xalan', 'dealII', 'cactusADM', 'mcf', 'perlbench', 'libquantum', 'namd'] ,
    ['gcc', 'cactusADM', 'h264ref', 'zeusmp', 'gamess', 'cactusADM', 'gamess', 'tonto'] ,
    ['omnetpp', 'GemsFDTD', 'sjeng', 'tonto', 'h264ref', 'bzip2', 'gromacs', 'hmmer'] ,
    ['sjeng', 'perlbench', 'zeusmp', 'gamess', 'leslie3d', 'leslie3d', 'gcc', 'gcc'] ,
    ['zeusmp', 'dealII', 'sjeng', 'zeusmp', 'mcf', 'perlbench', 'zeusmp', 'zeusmp'] ,
    ['xalan', 'gromacs', 'gcc', 'bzip2', 'dealII', 'gobmk', 'tonto', 'hmmer'] ,
    ['zeusmp', 'xalan', 'bwaves', 'libquantum', 'libquantum', 'leslie3d', 'zeusmp', 'hmmer'] ,
    ['namd', 'zeusmp', 'gamess', 'perlbench', 'omnetpp', 'gcc', 'tonto', 'zeusmp'] ,
    ['milc', 'gcc', 'gromacs', 'xalan', 'lbm', 'bwaves', 'gcc', 'h264ref'] ,
    ['perlbench', 'xalan', 'cactusADM', 'dealII', 'gobmk', 'leslie3d', 'cactusADM', 'gobmk'] ,
    ['libquantum', 'libquantum', 'hmmer', 'GemsFDTD', 'libquantum', 'leslie3d', 'hmmer', 'omnetpp'] ,
    ['perlbench', 'gcc', 'cactusADM', 'mcf', 'zeusmp', 'perlbench', 'omnetpp', 'libquantum'] ,
    ['zeusmp', 'gromacs', 'h264ref', 'mcf', 'milc', 'h264ref', 'hmmer', 'bzip2'] ,
    ['soplex', 'gamess', 'milc', 'gromacs', 'hmmer', 'sjeng', 'leslie3d', 'libquantum'] ,
    ['omnetpp', 'bzip2', 'libquantum', 'gobmk', 'bwaves', 'tonto', 'bwaves', 'sjeng'] ,
    ['tonto', 'xalan', 'omnetpp', 'gcc', 'tonto', 'h264ref', 'h264ref', 'hmmer'] ,
    ['GemsFDTD', 'lbm', 'perlbench', 'bwaves', 'tonto', 'dealII', 'gromacs', 'perlbench'] ,
    ['zeusmp', 'hmmer', 'lbm', 'leslie3d', 'mcf', 'mcf', 'mcf', 'bwaves'] ,
]

# multiprog_8 = [
#     ['xalan', 'xalan', 'soplex', 'soplex', 'mcf', 'mcf', 'omnetpp', 'omnetpp'],
#     ['milc', 'milc', 'lbm', 'lbm', 'xalan', 'xalan', 'zeusmp', 'zeusmp'],
#     ['lbm', 'lbm', 'lbm', 'lbm', 'lbm', 'lbm', 'lbm', 'lbm'],
#     ['libquantum', 'libquantum', 'libquantum', 'libquantum', 'libquantum', 'libquantum', 'libquantum', 'libquantum'],
#     ['mcf', 'mcf', 'mcf', 'mcf', 'mcf', 'mcf', 'mcf', 'mcf'],
#     ['milc', 'milc', 'milc', 'milc', 'milc', 'milc', 'milc', 'milc'],
#     ['zeusmp', 'zeusmp', 'zeusmp', 'zeusmp', 'zeusmp', 'zeusmp', 'zeusmp', 'zeusmp'],
#     ['GemsFDTD', 'GemsFDTD', 'GemsFDTD', 'GemsFDTD', 'GemsFDTD', 'GemsFDTD', 'GemsFDTD', 'GemsFDTD'],
#     ['xalan', 'xalan', 'xalan', 'xalan', 'xalan', 'xalan', 'xalan', 'xalan']
# ]
#
# multiprog_8 = [
#     ['perlbench', 'perlbench', 'perlbench', 'perlbench', 'perlbench', 'perlbench', 'perlbench', 'perlbench'] ,
#     ['bzip2', 'bzip2', 'bzip2', 'bzip2', 'bzip2', 'bzip2', 'bzip2', 'bzip2'] ,
#     ['gcc', 'gcc', 'gcc', 'gcc', 'gcc', 'gcc', 'gcc', 'gcc'] ,
#     ['mcf', 'mcf', 'mcf', 'mcf', 'mcf', 'mcf', 'mcf', 'mcf'] ,
#     ['gobmk', 'gobmk', 'gobmk', 'gobmk', 'gobmk', 'gobmk', 'gobmk', 'gobmk'] ,
#     ['hmmer', 'hmmer', 'hmmer', 'hmmer', 'hmmer', 'hmmer', 'hmmer', 'hmmer'] ,
#     ['sjeng', 'sjeng', 'sjeng', 'sjeng', 'sjeng', 'sjeng', 'sjeng', 'sjeng'] ,
#     ['libquantum', 'libquantum', 'libquantum', 'libquantum', 'libquantum', 'libquantum', 'libquantum', 'libquantum'] ,
#     ['h264ref', 'h264ref', 'h264ref', 'h264ref', 'h264ref', 'h264ref', 'h264ref', 'h264ref'] ,
#     ['omnetpp', 'omnetpp', 'omnetpp', 'omnetpp', 'omnetpp', 'omnetpp', 'omnetpp', 'omnetpp'] ,
#     ['xalan', 'xalan', 'xalan', 'xalan', 'xalan', 'xalan', 'xalan', 'xalan'] ,
#     ['bwaves', 'bwaves', 'bwaves', 'bwaves', 'bwaves', 'bwaves', 'bwaves', 'bwaves'] ,
#     ['gamess', 'gamess', 'gamess', 'gamess', 'gamess', 'gamess', 'gamess', 'gamess'] ,
#     ['milc', 'milc', 'milc', 'milc', 'milc', 'milc', 'milc', 'milc'] ,
#     ['zeusmp', 'zeusmp', 'zeusmp', 'zeusmp', 'zeusmp', 'zeusmp', 'zeusmp', 'zeusmp'] ,
#     ['gromacs', 'gromacs', 'gromacs', 'gromacs', 'gromacs', 'gromacs', 'gromacs', 'gromacs'] ,
#     ['cactusADM', 'cactusADM', 'cactusADM', 'cactusADM', 'cactusADM', 'cactusADM', 'cactusADM', 'cactusADM'] ,
#     ['leslie3d', 'leslie3d', 'leslie3d', 'leslie3d', 'leslie3d', 'leslie3d', 'leslie3d', 'leslie3d'] ,
#     ['namd', 'namd', 'namd', 'namd', 'namd', 'namd', 'namd', 'namd'] ,
#     ['dealII', 'dealII', 'dealII', 'dealII', 'dealII', 'dealII', 'dealII', 'dealII'] ,
#     ['soplex', 'soplex', 'soplex', 'soplex', 'soplex', 'soplex', 'soplex', 'soplex'] ,
#     ['GemsFDTD', 'GemsFDTD', 'GemsFDTD', 'GemsFDTD', 'GemsFDTD', 'GemsFDTD', 'GemsFDTD', 'GemsFDTD'] ,
#     ['tonto', 'tonto', 'tonto', 'tonto', 'tonto', 'tonto', 'tonto', 'tonto'] ,
#     ['lbm', 'lbm', 'lbm', 'lbm', 'lbm', 'lbm', 'lbm', 'lbm']
# ]

multiprog_4 = [
    ['perlbench', 'perlbench', 'perlbench', 'perlbench'] ,
    ['bzip2', 'bzip2', 'bzip2', 'bzip2'] ,
    ['gcc', 'gcc', 'gcc', 'gcc'] ,
    ['mcf', 'mcf', 'mcf', 'mcf'] ,
    ['gobmk', 'gobmk', 'gobmk', 'gobmk'] ,
    ['hmmer', 'hmmer', 'hmmer', 'hmmer'] ,
    ['sjeng', 'sjeng', 'sjeng', 'sjeng'] ,
    ['libquantum', 'libquantum', 'libquantum', 'libquantum'] ,
    ['h264ref', 'h264ref', 'h264ref', 'h264ref'] ,
    ['omnetpp', 'omnetpp', 'omnetpp', 'omnetpp'] ,
    ['xalan', 'xalan', 'xalan', 'xalan'] ,
    ['bwaves', 'bwaves', 'bwaves', 'bwaves'] ,
    ['gamess', 'gamess', 'gamess', 'gamess'] ,
    ['milc', 'milc', 'milc', 'milc'] ,
    ['zeusmp', 'zeusmp', 'zeusmp', 'zeusmp'] ,
    ['gromacs', 'gromacs', 'gromacs', 'gromacs'] ,
    ['cactusADM', 'cactusADM', 'cactusADM', 'cactusADM'] ,
    ['leslie3d', 'leslie3d', 'leslie3d', 'leslie3d', 'leslie3d'] ,
    ['namd', 'namd', 'namd', 'namd'] ,
    ['dealII', 'dealII', 'dealII', 'dealII'] ,
    ['soplex', 'soplex', 'soplex', 'soplex'] ,
    ['GemsFDTD', 'GemsFDTD', 'GemsFDTD', 'GemsFDTD'] ,
    ['tonto', 'tonto', 'tonto', 'tonto'] ,
    ['lbm', 'lbm', 'lbm', 'lbm'] 
]

multiprog_2 = [
    ['perlbench', 'perlbench'] ,
    ['bzip2', 'bzip2'] ,
    ['gcc', 'gcc'] ,
    ['mcf', 'mcf'] ,
    ['gobmk', 'gobmk'] ,
    ['hmmer', 'hmmer'] ,
    ['sjeng', 'sjeng'] ,
    ['libquantum', 'libquantum'] ,
    ['h264ref', 'h264ref'] ,
    ['omnetpp', 'omnetpp'] ,
    ['xalan', 'xalan'] ,
    ['bwaves', 'bwaves'] ,
    ['gamess', 'gamess'] ,
    ['milc', 'milc'] ,
    ['zeusmp', 'zeusmp'] ,
    ['gromacs', 'gromacs'] ,
    ['cactusADM', 'cactusADM'] ,
    ['leslie3d', 'leslie3d'] ,
    ['namd', 'namd'] ,
    ['dealII', 'dealII'] ,
    ['soplex', 'soplex'] ,
    ['GemsFDTD', 'GemsFDTD'] ,
    ['tonto', 'tonto'] ,
    ['lbm', 'lbm']
]

parsec_8 = [
    ['blackscholes', 'blackscholes', 'blackscholes', 'blackscholes', 'blackscholes', 'blackscholes', 'blackscholes', 'blackscholes'],
    ['bodytrack', 'bodytrack', 'bodytrack', 'bodytrack', 'bodytrack', 'bodytrack', 'bodytrack', 'bodytrack'],
    ['canneal', 'canneal', 'canneal', 'canneal', 'canneal', 'canneal', 'canneal', 'canneal'],
    ['facesim', 'facesim', 'facesim', 'facesim', 'facesim', 'facesim', 'facesim', 'facesim'],
    ['ferret', 'ferret', 'ferret', 'ferret', 'ferret', 'ferret', 'ferret', 'ferret'],
    ['fluidanimate', 'fluidanimate', 'fluidanimate', 'fluidanimate', 'fluidanimate', 'fluidanimate', 'fluidanimate', 'fluidanimate'],
    ['freqmine', 'freqmine', 'freqmine', 'freqmine', 'freqmine', 'freqmine', 'freqmine', 'freqmine'],
    ['swaptions', 'swaptions', 'swaptions', 'swaptions', 'swaptions', 'swaptions', 'swaptions', 'swaptions'],
    ['streamcluster', 'streamcluster', 'streamcluster', 'streamcluster', 'streamcluster', 'streamcluster', 'streamcluster', 'streamcluster'],
    ['vips', 'vips', 'vips', 'vips', 'vips', 'vips', 'vips', 'vips']
]

if not os.path.exists(scriptgen_dir):
    os.makedirs(scriptgen_dir)

if not os.path.exists(results_dir):
    os.makedirs(results_dir)

if not os.path.exists(stdout_dir):
    os.makedirs(stdout_dir)

if not os.path.exists(stderr_dir):
    os.makedirs(stderr_dir)

def multiprogs_DRAMSim2():
    for workload in workloads:
        name = ""
        for bench in workload:
            name += bench + "_"
        name = name[:-1]
        result_folder = results_dir + "/" + folder + "/" + name
        stdout_folder = stdout_dir + "/" + folder
        stderr_folder = stderr_dir + "/" + folder
        if not os.path.exists(result_folder):
            os.makedirs(result_folder)
        if not os.path.exists(stdout_folder):
            os.makedirs(stdout_folder)
        if not os.path.exists(stderr_folder):
            os.makedirs(stderr_folder)
        # create config file
        filename = name + ".cfg"
        config_file = open(scriptgen_dir + "/" + filename, "w")
        config =  "sim = {\n"
        config += "    phaseLength = 10000;\n"
        config += "    maxProcEventualDumps = " + str(len(workload)) + ";\n"
        config += "    statsPhaseInterval = 10000;\n};\n\n"
        config += "sys = {\n"
        config += "    frequency = 1200;\n"
        config += "    lineSize = 64;\n"
        config += "    cores = {\n"
        config += "        nehalem = {\n"
        config += "            type = \"OOO\";\n"
        config += "            cores = " + str(len(workload)) + ";\n"
        config += "            icache = \"l1\";\n"
        config += "            dcache = \"l1\";\n"
        config += "        };\n"
        config += "    };\n\n"
        config += "    caches = {\n"
        config += "        l1 = {\n"
        config += "            size = 32768;\n"
        config += "            caches = " + str(len(workload)*2) + ";\n"
        config += "            parent = \"l2\";\n"
        config += "        };\n\n"
        config += "        l2 = {\n"
        config += "            size = " + str(len(workload)*1048576) + ";\n"
        config += "            caches = 1;\n"
        config += "            array = {\n"
        config += "                ways = " + str(len(workload)*8) + ";\n"
        config += "                hash = \"None\";\n"
        config += "            };\n"
        config += "            repl = {\n"
        config += "                type = \"WayPart\";\n"
        config += "            };\n"
        config += "            parent = \"mem\";\n"
        config += "        };\n\n"
        config += "    };\n"
        config += "    mem = {\n"
        config += "        controllers = 1;\n"
        config += "        latency = 10;\n"
        config += "        capacityMB = 32768\n"
        config += "        num_pids = " + str(len(workload)) + ";\n" 
        config += "        type = \"DRAMSim\";\n"
        config += "        techIni = \"" + techIni + "\";\n"
        config += "        systemIni = \"" + systemIni + "\";\n"
        config += "        outputDir = \"" + results_dir + "/" + folder + "\";\n"
        config += "        traceName = \"" + name + "\";\n"
        config += "    };\n"
        config += "};\n\n"
        for i in range(len(workload)):
            config += "process" + str(i) + " = {\n"
            config += "    mask = \"" + str(i) + "\";\n"
            config += "    command = \"" + specinvoke[workload[i]] + "\";\n"
            if (workload[i] in redirect_input.keys()):
                config += "    input = \"" + redirect_input[workload[i]] + "\";\n"
            config += "    startFastForwarded = True;\n"
            config += "    ffiPoints = \"1000000000 100000000000\";\n"
            config += "    dumpInstrs = 100000000L;\n"
            config += "};\n\n"
        
        config_file.write("%s\n" % config)
        config_file.close()
        # create run script
        filename = name + ".sh"
        bash_file = open(scriptgen_dir + "/" + filename, "w")
        command = "#!/bin/bash\n\n"
        command += zsim_home + "/build/opt/zsim " + scriptgen_dir + "/" + name + ".cfg " + result_folder + "\n"
        os.system("chmod +x " + scriptgen_dir + "/" + filename)
        
        bash_file.write("%s\n" % command)
        bash_file.close()
        # create submit script
        filename = name + ".sub"
        submit_file = open(scriptgen_dir + "/" + filename, "w")
        env = "Universe = Vanilla\n"
        env += "getenv = True\n"
        env += "Executable = " + scriptgen_dir + "/" + name + ".sh\n"
        env += "Output = " + stdout_folder + "/" + name + ".out\n"
        env += "Error = " + stderr_folder + "/" + name + ".err\n"
        env += "Log = " + stdout_folder + "/" + name + ".log\n"
        env += "Queue\n"
        
        submit_file.write("%s\n" % env)
        submit_file.close()
        
        os.system("condor_submit " + scriptgen_dir + "/" + filename)
        time.sleep(2)

def multiprogs_parsec():
    for workload in workloads:
        name = ""
        for bench in workload:
            name += bench + "_"
        name = name[:-1]
        result_folder = results_dir + "/" + folder + "/" + name
        stdout_folder = stdout_dir + "/" + folder
        stderr_folder = stderr_dir + "/" + folder
        if not os.path.exists(result_folder):
            os.makedirs(result_folder)
        if not os.path.exists(stdout_folder):
            os.makedirs(stdout_folder)
        if not os.path.exists(stderr_folder):
            os.makedirs(stderr_folder)
        # create config file
        filename = name + ".cfg"
        config_file = open(scriptgen_dir + "/" + filename, "w")
        config =  "sim = {\n"
        config += "    phaseLength = 10000;\n"
        # config += "    maxProcEventualDumps = " + str(len(workload)) + ";\n"
        config += "    statsPhaseInterval = 10000;\n};\n\n"
        config += "sys = {\n"
        config += "    frequency = 1200;\n"
        config += "    lineSize = 64;\n"
        config += "    cores = {\n"
        config += "        nehalem = {\n"
        config += "            type = \"OOO\";\n"
        config += "            cores = " + str(len(workload)) + ";\n"
        config += "            icache = \"l1\";\n"
        config += "            dcache = \"l1\";\n"
        config += "        };\n"
        config += "    };\n\n"
        config += "    caches = {\n"
        config += "        l1 = {\n"
        config += "            size = 32768;\n"
        config += "            caches = " + str(len(workload)*2) + ";\n"
        config += "            parent = \"l2\";\n"
        config += "        };\n\n"
        config += "        l2 = {\n"
        config += "            size = " + str(len(workload)/4*1048576) + ";\n"
        config += "            caches = 1;\n"
        config += "            array = {\n"
        config += "                ways = " + str(len(workload)/4*8) + ";\n"
        config += "                hash = \"None\";\n"
        config += "            };\n"
        # config += "            repl = {\n"
        # config += "                type = \"WayPart\";\n"
        # config += "            };\n"
        config += "            parent = \"mem\";\n"
        config += "        };\n\n"
        config += "    };\n"
        config += "    mem = {\n"
        config += "        controllers = 1;\n"
        config += "        latency = 10;\n"
        config += "        capacityMB = 32768\n"
        config += "        num_pids = " + str(len(workload)/4) + ";\n" 
        config += "        multithread = True;\n"
        config += "        type = \"DRAMSim\";\n"
        config += "        techIni = \"" + techIni + "\";\n"
        config += "        systemIni = \"" + systemIni + "\";\n"
        config += "        outputDir = \"" + results_dir + "/" + folder + "\";\n"
        config += "        traceName = \"" + name + "\";\n"
        config += "    };\n"
        config += "};\n\n"
        for i in range(len(workload)/4):
            j = i*4
            config += "process" + str(i) + " = {\n"
            config += "    mask = \"" + str(j) + " " + str(j+1) + " " + str(j+2) + " " + str(j+3) + " " + "\";\n"
            config += "    command = \"" + parsecinvoke[workload[i]] + "\";\n"
            if (workload[i] in redirect_input.keys()):
                config += "    input = \"" + redirect_input[workload[i]] + "\";\n"
            config += "    startFastForwarded = True;\n"
            config += "};\n\n"
        
        config_file.write("%s\n" % config)
        config_file.close()
        # create run script
        filename = name + ".sh"
        bash_file = open(scriptgen_dir + "/" + filename, "w")
        command = "#!/bin/bash\n\n"
        command += zsim_home + "/build/opt/zsim " + scriptgen_dir + "/" + name + ".cfg " + result_folder + "\n"
        os.system("chmod +x " + scriptgen_dir + "/" + filename)
        
        bash_file.write("%s\n" % command)
        bash_file.close()
        # create submit script
        filename = name + ".sub"
        submit_file = open(scriptgen_dir + "/" + filename, "w")
        env = "Universe = Vanilla\n"
        env += "getenv = True\n"
        env += "Executable = " + scriptgen_dir + "/" + name + ".sh\n"
        env += "Output = " + stdout_folder + "/" + name + ".out\n"
        env += "Error = " + stderr_folder + "/" + name + ".err\n"
        env += "Log = " + stdout_folder + "/" + name + ".log\n"
        env += "Queue\n"
        
        submit_file.write("%s\n" % env)
        submit_file.close()
        
        os.system("condor_submit " + scriptgen_dir + "/" + filename)
        time.sleep(5)
        
def singleprogs():
    for workload in singleprog:
        result_folder = results_dir + "/" + folder + "/" + workload
        stdout_folder = stdout_dir + "/" + folder
        stderr_folder = stderr_dir + "/" + folder
        if not os.path.exists(result_folder):
            os.makedirs(result_folder)
        if not os.path.exists(stdout_folder):
            os.makedirs(stdout_folder)
        if not os.path.exists(stderr_folder):
            os.makedirs(stderr_folder)
        # create config file
        filename = workload + ".cfg"
        config_file = open(scriptgen_dir + "/" + filename, "w")
        config =  "sim = {\n"
        config += "    phaseLength = 10000;\n};\n\n"
        config += "sys = {\n"
        config += "    frequency = 1200;\n"
        config += "    lineSize = 64;\n"
        config += "    cores = {\n"
        config += "        OutOfOrder = {\n"
        config += "            type = \"OOO\";\n"
        config += "            icache = \"l1d\";\n"
        config += "            dcache = \"l1i\";\n"
        config += "        };\n"
        config += "    };\n\n"
        config += "    caches = {\n"
        config += "        l1d = {\n"
        config += "            size = 32768;\n"
        config += "            parent = \"l2\";\n"
        config += "        };\n\n"
        config += "        l1i = {\n"
        config += "            size = 32768;\n"
        config += "            parent = \"l2\";\n"
        config += "        };\n\n"        
        config += "        l2 = {\n"
        config += "            size = 1048576;\n"
        config += "            caches = 1;\n"
        config += "            array = {\n"
        config += "                ways = 8;\n"
        config += "                hash = \"None\";\n"
        config += "            };\n"
        config += "            parent = \"mem\";\n"
        config += "        };\n\n"
        config += "    };\n"
        config += "    mem = {\n"
        config += "        controllers = 1;\n"
        config += "        latency = 10;\n"
        config += "        capacityMB = 32768\n"
        config += "        type = \"DRAMSim\";\n"
        config += "        techIni = \"" + techIni + "\";\n"
        config += "        systemIni = \"" + systemIni + "\";\n"
        config += "        outputDir = \"" + results_dir + "/" + folder + "\";\n"
        config += "        traceName = \"" + workload + "\";\n"
        config += "    };\n"
        config += "};\n\n"
        config += "process0 = {\n"
        config += "    command = \"" + specinvoke[workload] + "\";\n"
        if (workload in redirect_input.keys()):
            config += "    input = \"" + redirect_input[workload] + "\";\n"
        config += "    startFastForwarded = True;\n"
        config += "    ffiPoints = \"1000000000 100000000\";\n"
        config += "};\n\n"
        
        config_file.write("%s\n" % config)
        config_file.close()
        # create run script
        filename = workload + ".sh"
        bash_file = open(scriptgen_dir + "/" + filename, "w")
        command = "#!/bin/bash\n\n"
        command += zsim_home + "/build/opt/zsim " + scriptgen_dir + "/" + workload + ".cfg " + result_folder + "\n"
        os.system("chmod +x " + scriptgen_dir + "/" + filename)
        
        bash_file.write("%s\n" % command)
        bash_file.close()
        # create submit script
        filename = workload + ".sub"
        submit_file = open(scriptgen_dir + "/" + filename, "w")
        env = "Universe = Vanilla\n"
        env += "getenv = True\n"
        env += "Executable = " + scriptgen_dir + "/" + workload + ".sh\n"
        env += "Output = " + stdout_folder + "/" + workload + ".out\n"
        env += "Error = " + stderr_folder + "/" + workload + ".err\n"
        env += "Log = " + stdout_folder + "/" + workload + ".log\n"
        env += "Queue\n"
        
        submit_file.write("%s\n" % env)
        submit_file.close()
        
        os.system("condor_submit " + scriptgen_dir + "/" + filename)

def singleprogs_parsec():
    for workload in singleprog_parsec:
        result_folder = results_dir + "/" + folder + "/" + workload
        stdout_folder = stdout_dir + "/" + folder
        stderr_folder = stderr_dir + "/" + folder
        if not os.path.exists(result_folder):
            os.makedirs(result_folder)
        if not os.path.exists(stdout_folder):
            os.makedirs(stdout_folder)
        if not os.path.exists(stderr_folder):
            os.makedirs(stderr_folder)
        # create config file
        filename = workload + ".cfg"
        config_file = open(scriptgen_dir + "/" + filename, "w")
        config =  "sim = {\n"
        config += "    phaseLength = 10000;\n};\n\n"
        config += "sys = {\n"
        config += "    frequency = 1200;\n"
        config += "    lineSize = 64;\n"
        config += "    cores = {\n"
        config += "        OutOfOrder = {\n"
        config += "            type = \"OOO\";\n"
        config += "            icache = \"l1d\";\n"
        config += "            dcache = \"l1i\";\n"
        config += "        };\n"
        config += "    };\n\n"
        config += "    caches = {\n"
        config += "        l1d = {\n"
        config += "            size = 32768;\n"
        config += "            parent = \"l2\";\n"
        config += "        };\n\n"
        config += "        l1i = {\n"
        config += "            size = 32768;\n"
        config += "            parent = \"l2\";\n"
        config += "        };\n\n"        
        config += "        l2 = {\n"
        config += "            size = 1048576;\n"
        config += "            caches = 1;\n"
        config += "            array = {\n"
        config += "                ways = 8;\n"
        config += "                hash = \"None\";\n"
        config += "            };\n"
        config += "            parent = \"mem\";\n"
        config += "        };\n\n"
        config += "    };\n"
        config += "    mem = {\n"
        config += "        controllers = 1;\n"
        config += "        latency = 10;\n"
        config += "        capacityMB = 32768\n"
        config += "        type = \"DRAMSim\";\n"
        config += "        techIni = \"" + techIni + "\";\n"
        config += "        systemIni = \"" + systemIni + "\";\n"
        config += "        outputDir = \"" + results_dir + "/" + folder + "\";\n"
        config += "        traceName = \"" + workload + "\";\n"
        config += "    };\n"
        config += "};\n\n"
        config += "process0 = {\n"
        config += "    command = \"" + parsecinvoke[workload] + "\";\n"
        if (workload in redirect_input.keys()):
            config += "    input = \"" + redirect_input[workload] + "\";\n"
        config += "    startFastForwarded = True;\n"
        config += "};\n\n"
        
        config_file.write("%s\n" % config)
        config_file.close()
        # create run script
        filename = workload + ".sh"
        bash_file = open(scriptgen_dir + "/" + filename, "w")
        command = "#!/bin/bash\n\n"
        command += zsim_home + "/build/opt/zsim " + scriptgen_dir + "/" + workload + ".cfg " + result_folder + "\n"
        os.system("chmod +x " + scriptgen_dir + "/" + filename)
        
        bash_file.write("%s\n" % command)
        bash_file.close()
        # create submit script
        filename = workload + ".sub"
        submit_file = open(scriptgen_dir + "/" + filename, "w")
        env = "Universe = Vanilla\n"
        env += "getenv = True\n"
        env += "Executable = " + scriptgen_dir + "/" + workload + ".sh\n"
        env += "Output = " + stdout_folder + "/" + workload + ".out\n"
        env += "Error = " + stderr_folder + "/" + workload + ".err\n"
        env += "Log = " + stdout_folder + "/" + workload + ".log\n"
        env += "Queue\n"
        
        submit_file.write("%s\n" % env)
        submit_file.close()
        
        os.system("condor_submit " + scriptgen_dir + "/" + filename)
                
# call the function        
# folder = "single_prog_parsec"
# systemIni = systemIni_insec
# singleprogs_parsec()
#
# # Two programs
# workloads = multiprog_2
# # baseline scheme
# folder = "insec_2"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_insec
# multiprogs_DRAMSim2()
#
# # SecMem
# folder = "sec_2"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_sec2banks
# multiprogs_DRAMSim2()
#
# # FS
# folder = "fs_2"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_fs
# multiprogs_DRAMSim2()
#
# # smart scheduling
# folder = "smart_2"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_smart
# multiprogs_DRAMSim2()
#
# # Four programs
# workloads = multiprog_4
# # baseline scheme
# folder = "insec_4"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_insec
# multiprogs_DRAMSim2()
#
# # SecMem
# folder = "sec_4"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_sec2banks
# multiprogs_DRAMSim2()
#
# # FS
# folder = "fs_4"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_fs
# multiprogs_DRAMSim2()
#
# # smart scheduling
# folder = "smart_4"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_smart
# multiprogs_DRAMSim2()
#
# # Eight programs
# workloads = multiprog_8
# # baseline scheme
# folder = "insec_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_insec
# multiprogs_DRAMSim2()
#
# # SecMem
# folder = "sec_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_sec2banks
# multiprogs_DRAMSim2()
#
# # FS
# folder = "fs_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_fs
# multiprogs_DRAMSim2()
#
# # smart scheduling
# folder = "smart_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_smart
# multiprogs_DRAMSim2()
#
# # Random address mapping
# folder = "secrand_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_secrand
# multiprogs_DRAMSim2()
#
# # Interleaving three banks in one turn
# folder = "sec3banks_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_sec
# multiprogs_DRAMSim2()
#
# # bank partitioning
# folder = "bankpar_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_bankpar
# multiprogs_DRAMSim2()
#
# # rank partitioning
# folder = "rankpar_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_rankpar
# multiprogs_DRAMSim2()
#
# # side channel protection
# folder = "relax_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_relax
# multiprogs_DRAMSim2()
#
# # side channel protection with stats
# workloads = multiprog_8
# folder = "conflict_stats"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_relax
# multiprogs_DRAMSim2()
#
# # mixed policy
# workloads = multiprog_8
# folder = "mix_100K"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_mix
# multiprogs_DRAMSim2()

# # probability based scheduling
# workloads = multiprog_8
# folder = "prob"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_prob
# multiprogs_DRAMSim2()

# # probability based scheduling
# workloads = multiprog_8
# folder = "probmix_1M"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_probmix
# multiprogs_DRAMSim2()

# # probability based scheduling
# workloads = multiprog_8
# folder = "limit_100000"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_limit
# multiprogs_DRAMSim2()

# Eight programs
workloads = multiprog_8
# # baseline scheme
# folder = "insec_diffbench_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_insec
# multiprogs_DRAMSim2()

# # SecMem
# folder = "sec_diffbench_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_sec2banks
# multiprogs_DRAMSim2()

# # FS
# folder = "fs_diffbench_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_fs
# multiprogs_DRAMSim2()

# # bank partitioning
# folder = "bankpar_diffbench_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_bankpar
# multiprogs_DRAMSim2()
#
# # rank partitioning
# folder = "rankpar_diffbench_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_rankpar
# multiprogs_DRAMSim2()

# Eight programs
# workloads = parsec_8
# # baseline scheme
# folder = "parsec_insec_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_insec
# multiprogs_parsec()

# # SecMem
# folder = "parsec_sec_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_sec2banks
# multiprogs_parsec()
#
# # FS
# folder = "parsec_fs_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_fs
# multiprogs_parsec()
#
# # Random address mapping
# folder = "parsec_secrand_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_secrand
# multiprogs_parsec()
#
# # Interleaving three banks in one turn
# folder = "parsec_sec3banks_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_sec
# multiprogs_parsec()

# # bank partitioning
# folder = "parsec_bankpar_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_bankpar
# multiprogs_parsec()
#
# # rank partitioning
# folder = "parsec_rankpar_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_rankpar
# multiprogs_parsec()

# # side channel protection
# folder = "parsec_relax_8"
#
# if not os.path.exists(results_dir + "/" + folder):
#     os.makedirs(results_dir + "/" + folder)
#
# if not os.path.exists(stdout_dir + "/" + folder):
#     os.makedirs(stdout_dir + "/" + folder)
#
# if not os.path.exists(stderr_dir + "/" + folder):
#     os.makedirs(stderr_dir + "/" + folder)
#
# systemIni = systemIni_relax
# multiprogs_parsec()

# Dynamic scheduling
folder = "dynamic_rank_3_64"

if not os.path.exists(results_dir + "/" + folder):
    os.makedirs(results_dir + "/" + folder)

if not os.path.exists(stdout_dir + "/" + folder):
    os.makedirs(stdout_dir + "/" + folder)

if not os.path.exists(stderr_dir + "/" + folder):
    os.makedirs(stderr_dir + "/" + folder)

systemIni = systemIni_dynamic
multiprogs_DRAMSim2()
