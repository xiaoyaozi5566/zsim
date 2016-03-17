#!/usr/bin/python

import os
import sys
import time

zsim_home = "/home/yw438/zsim"
spec_dir = "/home/yw438/benchmarks"
scriptgen_dir = zsim_home + "/scriptgen"
results_dir = zsim_home + "/results"
stdout_dir = zsim_home + "/stdout"
stderr_dir = zsim_home + "/stderr"

techIni = "/home/yw438/zsim/DRAMSim2/ini/DDR3_micron_64M_8B_x4_sg15.ini"
systemIni_sec = "/home/yw438/zsim/DRAMSim2/system.ini.sec"
systemIni_insec = "/home/yw438/zsim/DRAMSim2/system.ini.baseline"
systemIni_fs = "/home/yw438/zsim/DRAMSim2/system.ini.fs"

# remove astar due to bugs
specint = ['perlbench', 'bzip2', 'gcc', 'mcf', 'gobmk', 'hmmer', 'sjeng', 'libquantum', 'h264ref', 'omnetpp', 'xalan']

specfp = ['bwaves', 'gamess', 'milc', 'zeusmp', 'gromacs', 'cactusADM', 'leslie3d', 'namd', 'dealII', 'soplex', 'povray', 'calculix', 'GemsFDTD', 'tonto', 'lbm', 'wrf']

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
    'xalan'      : spec_dir + "/Xalan -v " + spec_dir + "/t5.xml" + spec_dir + "/xalanc.xsl",
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

bench_name = {
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
    'wrf'   : 'wrf'
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

multiprog = []

for i in range(len(singleprog)-1):
    multiprog.append([singleprog[i], singleprog[i+1]])

multiprog_8 = [
    ['xalan', 'xalan', 'leslie3d', 'libquantum', 'lbm', 'leslie3d', 'leslie3d', 'GemsFDTD'] ,
    ['milc', 'xalan', 'hmmer', 'calculix', 'xalan', 'bzip2', 'GemsFDTD', 'milc'] ,
    ['omnetpp', 'gromacs', 'gamess', 'omnetpp', 'wrf', 'mcf', 'povray', 'mcf'] ,
    ['gobmk', 'cactusADM', 'povray', 'povray', 'gromacs', 'milc', 'cactusADM', 'gamess'] ,
    ['lbm', 'mcf', 'gcc', 'libquantum', 'zeusmp', 'povray', 'leslie3d', 'tonto'] ,
    ['xalan', 'namd', 'mcf', 'soplex', 'tonto', 'xalan', 'mcf', 'lbm'] ,
    ['perlbench', 'GemsFDTD', 'gcc', 'mcf', 'dealII', 'wrf', 'gobmk', 'lbm'] ,
    ['bwaves', 'bzip2', 'sjeng', 'lbm', 'gcc', 'xalan', 'milc', 'tonto'] ,
    ['bwaves', 'perlbench', 'GemsFDTD', 'gobmk', 'perlbench', 'calculix', 'perlbench', 'bwaves'] ,
    ['mcf', 'calculix', 'xalan', 'libquantum', 'perlbench', 'tonto', 'lbm', 'omnetpp']]

workloads = multiprog

if not os.path.exists(scriptgen_dir):
    os.makedirs(scriptgen_dir)

if not os.path.exists(results_dir):
    os.makedirs(results_dir)

if not os.path.exists(stdout_dir):
    os.makedirs(stdout_dir)

if not os.path.exists(stderr_dir):
    os.makedirs(stderr_dir)
    
folder = sys.argv[1]

if not os.path.exists(results_dir + "/" + folder):
    os.makedirs(results_dir + "/" + folder)

if not os.path.exists(stdout_dir + "/" + folder):
    os.makedirs(stdout_dir + "/" + folder)

if not os.path.exists(stderr_dir + "/" + folder):
    os.makedirs(stderr_dir + "/" + folder)

def multiprogs():
    for workload in workloads:
        p0 = workload[0]
        p1 = workload[1]
        result_folder = results_dir + "/" + folder + "/" + p0 + "_" + p1
        stdout_folder = stdout_dir + "/" + folder
        stderr_folder = stderr_dir + "/" + folder
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
        config += "        controllers = 1;\n"
        config += "        type = \"DDR\"\n"
        config += "    };\n"
        config += "};\n\n"
        config += "process0 = {\n"
        config += "    mask = \"0\";\n"
        config += "    command = \"" + specinvoke[p0] + "\";\n"
        if (p0 in redirect_input.keys()):
            config += "    input = \"" + redirect_input[p0] + "\";\n"
        config += "    startFastForwarded = True;\n"
        config += "    ffiPoints = \"2000000 1000000\";\n"
        config += "};\n\n"
        config += "process1 = {\n"
        config += "    mask = \"1\";\n"
        config += "    command = \"" + specinvoke[p1] + "\";\n"
        if (p1 in redirect_input.keys()):
            config += "    input = \"" + redirect_input[p1] + "\";\n"
        config += "    startFastForwarded = True;\n"
        config += "    ffiPoints = \"2000000 1000000\";\n"
        config += "};\n\n"
        
        config_file.write("%s\n" % config)
        config_file.close()
        # create run script
        filename = p0 + "_" + p1 + ".sh"
        bash_file = open(scriptgen_dir + "/" + filename, "w")
        command = "#!/bin/bash\n\n"
        command += zsim_home + "/build/opt/zsim " + scriptgen_dir + "/" + p0 + "_" + p1 + ".cfg " + result_folder + "\n"
        os.system("chmod +x " + scriptgen_dir + "/" + filename)
        
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
        env += "Log = " + stdout_folder + "/" + p0 + "_" + p1 + ".log\n"
        env += "Queue\n"
        
        submit_file.write("%s\n" % env)
        submit_file.close()
        
        os.system("condor_submit " + scriptgen_dir + "/" + filename)

def multiprogs_DRAMSim2_8():
    workloads = multiprog_8
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
        config += "    maxProcEventualDumps = 8;\n"
        config += "    statsPhaseInterval = 1;\n};\n\n"
        config += "sys = {\n"
        config += "    frequency = 2400;\n"
        config += "    lineSize = 64;\n"
        config += "    cores = {\n"
        config += "        nehalem = {\n"
        config += "            type = \"OOO\";\n"
        config += "            cores = 8;\n"
        config += "            icache = \"l1\";\n"
        config += "            dcache = \"l1\";\n"
        config += "        };\n"
        config += "    };\n\n"
        config += "    caches = {\n"
        config += "        l1 = {\n"
        config += "            size = 32768;\n"
        config += "            caches = 16;\n"
        config += "            parent = \"l2\";\n"
        config += "        };\n\n"
        config += "        l2 = {\n"
        config += "            size = 4194304;\n"
        config += "            caches = 1;\n"
        config += "            array = {\n"
        config += "                ways = 64;\n"
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
        config += "        num_pids = 8;\n" 
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
            config += "    ffiPoints = \"2000000 1000000000\";\n"
            config += "    dumpInstrs = 1000000L;\n"
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
        config += "    frequency = 2400;\n"
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
        config += "            size = 524288;\n"
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
        config += "    ffiPoints = \"2000000 1000000\";\n"
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
        
def multiprogs_DRAMSim2():
    for workload in workloads:
        p0 = workload[0]
        p1 = workload[1]
        result_folder = results_dir + "/" + folder + "/" + p0 + "_" + p1
        stdout_folder = stdout_dir + "/" + folder
        stderr_folder = stderr_dir + "/" + folder
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
        config += "    maxProcEventualDumps = 2;\n"
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
        config += "            repl = {\n"
        config += "                type = \"WayPart\";\n"
        config += "            };\n"
        config += "            parent = \"mem\";\n"
        config += "        };\n\n"
        config += "    };\n"
        config += "    mem = {\n"
        config += "        controllers = 1;\n"
        config += "        latency = 10;\n"
        config += "        type = \"DRAMSim\";\n"
        config += "        techIni = \"" + techIni + "\";\n"
        config += "        systemIni = \"" + systemIni + "\";\n"
        config += "        outputDir = \"" + results_dir + "/" + folder + "\";\n"
        config += "        traceName = \"" + p0 + "_" + p1 + "\";\n"
        config += "    };\n"
        config += "};\n\n"
        config += "process0 = {\n"
        config += "    mask = \"0\";\n"
        config += "    command = \"" + specinvoke[p0] + "\";\n"
        if (p0 in redirect_input.keys()):
            config += "    input = \"" + redirect_input[p0] + "\";\n"
        config += "    startFastForwarded = True;\n"
        config += "    ffiPoints = \"2000000 100000000\";\n"
        config += "    dumpInstrs = 1000000L;\n"
        config += "};\n\n"
        config += "process1 = {\n"
        config += "    mask = \"1\";\n"
        config += "    command = \"" + specinvoke[p1] + "\";\n"
        if (p1 in redirect_input.keys()):
            config += "    input = \"" + redirect_input[p1] + "\";\n"
        config += "    startFastForwarded = True;\n"
        config += "    ffiPoints = \"2000000 100000000\";\n"
        config += "    dumpInstrs = 1000000L;\n"
        config += "};\n\n"
        
        config_file.write("%s\n" % config)
        config_file.close()
        # create run script
        filename = p0 + "_" + p1 + ".sh"
        bash_file = open(scriptgen_dir + "/" + filename, "w")
        command = "#!/bin/bash\n\n"
        command += zsim_home + "/build/opt/zsim " + scriptgen_dir + "/" + p0 + "_" + p1 + ".cfg " + result_folder + "\n"
        os.system("chmod +x " + scriptgen_dir + "/" + filename)
        
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
        env += "Log = " + stdout_folder + "/" + p0 + "_" + p1 + ".log\n"
        env += "Queue\n"
        
        submit_file.write("%s\n" % env)
        submit_file.close()
        
        os.system("condor_submit " + scriptgen_dir + "/" + filename)
        time.sleep(5)
        
# call the function        
# multiprogs()
systemIni = systemIni_fs
# multiprogs_DRAMSim2()
multiprogs_DRAMSim2_8()
# singleprogs()