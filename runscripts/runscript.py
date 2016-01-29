#!/usr/bin/python

import os
import sys

zsim_home = "/home/yw438/zsim"
spec_dir = "/home/yw438/benchmarks"
scriptgen_dir = zsim_home + "/scriptgen"
results_dir = zsim_home + "/results"
stdout_dir = zsim_home + "/stdout"
stderr_dir = zsim_home + "/stderr"

specint = ['perlbench', 'bzip2', 'gcc', 'mcf', 'gobmk', 'hmmer', 'sjeng', 'libquantum', 'h264ref', 'omnetpp', 'astar', 'xalan']

spectfp = ['bwaves', 'gamess', 'milc', 'zeusmp', 'gromacs', 'cactusADM', 'leslie3d', 'namd', 'dealII', 'soplex', 'povray', 'calculix', 'GemsFDTD', 'tonto', 'lbm', 'wrf']

specinvoke = {
    'perlbench'  : spec_dir + "/perlbench -I./lib " + spec_dir + "/checkspam.pl 2500 5 25 11 150 1 1 1 1",
    'bzip2'      : spec_dir + "/bzip2 " + spec_dir + "/input.source 280",
    'gcc'        : spec_dir + "/gcc " + spec_dir + "/200.in -o results/200.s",
    'mcf'        : spec_dir + "/mcf " + spec_dir + "/inp.in",
    'gobmk'      : spec_dir + "/gobmk --quiet --mode gtp < " + spec_dir + "/13x13.tst",
    'hmmer'      : spec_dir + "/hmmer " + spec_dir + "/nph3.hmm " + spec_dir + "/swiss41",
    'sjeng'      : spec_dir + "/sjeng " + spec_dir + "/ref.txt",
    'libquantum' : spec_dir + "/libquantum 1397 8",
    'h264ref'    : spec_dir + "/h264ref -d " + spec_dir + "/foreman_ref_encoder_baseline.cfg",
    'omnetpp'    : spec_dir + "/omnetpp " + spec_dir + "/omnetpp.ini",
    'astar'      : spec_dir + "/astar " + spec_dir + "/BigLakes2048.cfg",
    'xalan'      : spec_dir + "/Xalan -v " + spec_dir + "/t5.xml" + spec_dir + "/xalanc.xsl",
    'bwaves'     : spec_dir + "/bwaves", 
    'gamess'     : spec_dir + "/gamess < " + spec_dir + "/cytosine.2.config",
    'milc'       : spec_dir + "/milc < " + spec_dir + "/su3imp.in",
    'zeusmp'     : spec_dir + "/zeusmp",
    'gromacs'    : spec_dir + "/gromacs -silent -deffnm gromacs -nice 0",
    'cactusADM'  : spec_dir + "/cactusADM " + spec_dir + "/benchADM.par",
    'leslie3d'   : spec_dir + "/leslie3d < " + spec_dir + "/leslie3d.in",
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

if not os.path.exists(scriptgen_dir):
    os.makedirs(scriptgen_dir)

if not os.path.exists(results_dir):
    os.makedirs(results_dir)

if not os.path.exists(stdout_dir):
    os.makedirs(stdout_dir)

if not os.path.exists(stderr_dir):
    os.makedirs(stderr_dir)
    
folder = sys.argv[1]

if not os.path.exists("results/" + folder):
    os.makedirs("results/" + folder)

if not os.path.exists("stdout/" + folder):
    os.makedirs("stdout/" + folder)

if not os.path.exists("stderr/" + folder):
    os.makedirs("stderr/" + folder)

def multiprogs():
    for workload in multiprog:
        p0 = workload[0]
        p1 = workload[1]
        result_folder = "results/" + folder + "/" + p0 + "_" + p1
        stdout_folder = "stdout/" + folder + "/" + p0 + "_" + p1
        stderr_folder = "stderr/" + folder + "/" + p0 + "_" + p1
        if not os.path.exists(result_folder):
            os.makedirs(result_folder)
        if not os.path.exists(stdout_folder):
            os.makedirs(stdout_folder)
        if not os.path.exists(stderr_folder):
            os.makedirs(stderr_folder)
        # create config file
        filename = p0 + "_" + p1 + ".cfg"
        config_file = open(scriptgen_dir + "/" + filename, "w")
        config =  "sim = {\n"
        config += "    phaseLength = 10000;\n"
        config += "    statsPhaseInterval = 1;\n};\n\n"
        config += "sys = {\n"
        config += "    frequency = 2400;\n"
        config += "    lineSize = 64;\n"
        config += "    cores = {\n"
        config += "        nehalem = {\n"
        config += "            type = \"OOO\";\n"
        config += "            cores = 2;\n"
        config += "            icache = \"l1\";\n"
        config += "            dcache = \"l1\";\n"
        config += "        };\n"
        config += "    };\n\n"
        config += "    caches = {\n"
        config += "        l1 = {\n"
        config += "            size = 32768;\n"
        config += "            caches = 4;\n"
        config += "            parent = \"l2\";\n"
        config += "        };\n\n"
        config += "        l2 = {\n"
        config += "            size = 1048576;\n"
        config += "            caches = 1;\n"
        config += "            array = {\n"
        config += "                ways = 16;\n"
        config += "                hash = \"None\";\n"
        config += "            };\n"
        config += "            parent = \"mem\";\n"
        config += "        };\n\n"
        config += "    };\n"
        config += "    mem = {\n"
        config += "        controllers = 2;\n"
        config += "        type = \"DDR\"\n";
        config += "    };\n"
        config += "};\n\n"
        config += "process0 = {\n"
        config += "    command = \"" + specinvoke[p0] + "\";\n"
        config += "    startFastForwarded = True;\n"
        config += "    ffiPoints = \"200000000 100000000\";\n"
        config += "};\n\n"
        config += "process1 = {\n"
        config += "    command = \"" + specinvoke[p1] + "\";\n"
        config += "    startFastForwarded = True;\n"
        config += "    ffiPoints = \"200000000 100000000\";\n"
        config += "};\n\n"
        
        config_file.write("%s\n" % config)
        config_file.close()
        # create run script
        filename = p0 + "_" + p1 + ".sh"
        bash_file = open(scriptgen_dir + "/" + filename, "w")
        command = "#!/bin/bash\n\n"
        command += "build/opt/zsim " + scriptgen_dir + "/" + p0 + "_" + p1 + ".cfg " + result_folder + "\n"
        
        bash_file.write("%s\n" % command)
        bash_file.close()
        # create submit script
        filename = p0 + "_" + p1 + ".sub"
        submit_file = open(scriptgen_dir + "/" + filename, "w")
        env = "Universe = Vanilla\n"
        env += "getenv = True\n"
        env += "Executable = " + scriptgen_dir + "/" + p0 + "_" + p1 + ".sh\n"
        env += "Output = " + stdout_folder + "/" + p0 + "_" + p1 + ".out\n"
        env += "Error = " + stderr_folder + "/" + p0 + "_" + p1 + ".err\n"
        env += "Log = " + stderr_folder + "/" + p0 + "_" + p1 + ".log\n"
        env += "Queue\n"
        
        submit_file.write("%s\n" % env)
        submit_file.close()

multiprogs()
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        