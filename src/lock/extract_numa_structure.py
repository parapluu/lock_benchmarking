#!/usr/bin/python

import sys
from os import listdir
from os.path import join
from os import mkdir
from subprocess import Popen
from subprocess import PIPE
import re 
import socket

def detectCPUs():
 """
 Detects the number of CPUs on a system.
 """
 # Linux, Unix and MacOS:
 if hasattr(os, "sysconf"):
     if os.sysconf_names.has_key("SC_NPROCESSORS_ONLN"):
         # Linux & Unix:
         ncpus = os.sysconf("SC_NPROCESSORS_ONLN")
         if isinstance(ncpus, int) and ncpus > 0:
             return ncpus
     else: # OSX:
         return int(os.popen2("sysctl -n hw.ncpu")[1].read())
 # Windows:
 if os.environ.has_key("NUMBER_OF_PROCESSORS"):
         ncpus = int(os.environ["NUMBER_OF_PROCESSORS"]);
         if ncpus > 0:
             return ncpus
 return 1 # Default


def numa_structure():
    lscpu_pipe = Popen("lscpu",stdout = PIPE).stdout
    number_of_numa_nodes = 0
    cpus_per_node = 0
    not_ready = True
    outputString = ""
    coresNodeList = []
    while not_ready:
        line = lscpu_pipe.readline()
        if line:
            matchObject = re.search("NUMA node\d CPU\(s\):(.*)", line, re.M)
            if matchObject:
                number_of_numa_nodes = number_of_numa_nodes + 1
                cpusString = matchObject.group(1).strip()
                rangeMatchObject = re.search("(\d+)-(\d+)", cpusString, re.M)
                coreListString = ""
                if rangeMatchObject:
                    start = int(rangeMatchObject.group(1).strip())
                    end = int(rangeMatchObject.group(2).strip())
                    cpus_per_node = end + 1
                    coreList = []
                    for i in range(start, end+1):
                        coreList.append(str(i))
                    coreListString = ",".join(coreList)
                else:
                    cpus_per_node = len(cpusString.split(","))
                    coreListString = re.sub(r' ', "", cpusString)
                coresNodeList.append(coreListString)
        else:
            not_ready = False

    #Hack because of bug in cpuinfo for bulldozer
    if socket.gethostname()=="bulldozer":
        newCoresNodeList = [] 
        for i in range(0, number_of_numa_nodes, 2):
            newCoresNodeList.append(coresNodeList[i] + "," + coresNodeList[i+1])
        coresNodeList = newCoresNodeList
        number_of_numa_nodes = number_of_numa_nodes / 2
        cpus_per_node = cpus_per_node * 2
    lscpu_pipe.close()
    coresNodeList = ["{" + x + "}" for x in coresNodeList]
    numaStructure = "{" + ",".join(coresNodeList) + "}"
    return(number_of_numa_nodes, cpus_per_node, numaStructure)

def numa_structure_defines():
    number_of_numa_nodes, cpus_per_node, numaStructure = numa_structure()
    return [('NUMBER_OF_NUMA_NODES', str(number_of_numa_nodes)),
            ('NUMBER_OF_CPUS_PER_NODE', str(cpus_per_node)),
            ('NUMA_STRUCTURE', numaStructure)]

number_of_numa_nodes, cpus_per_node, numaStructure = numa_structure()

print "-DNUMBER_OF_NUMA_NODES="+str(number_of_numa_nodes)+" "
print "-DNUMBER_OF_CPUS_PER_NODE="+str(cpus_per_node)+" "
print ("-DNUMA_STRUCTURE={" + numaStructure + "}")
    
