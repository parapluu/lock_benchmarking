from collections import OrderedDict
import multiprocessing

import sys
sys.path.append('../src/lock')
sys.path.append('src/lock')
from extract_numa_structure import numa_structure_defines


############
#Definitions
############

Import('mode')

use_cpp_locks = GetOption('cpp_locks')

std_cc_flags = ['-std=gnu99',
                '-Wall',
		'-fPIC',
                '-pthread']
std_cxx_flags = ['-std=c++11',
		 '-Wall',
		 '-Wextra',
		 '-pedantic',
		 '-fPIC',
		 '-pthread']

std_link_flags = ['-pthread']

debug_flags = ['-O1',
               '-g']

debug_flags0 = ['-O0',
               '-g']

optimization_flags = ['-O3']

profile_flags = ['-fno-omit-frame-pointer -O2']

if(mode=='debug'):
    std_cc_flags = std_cc_flags + debug_flags
    std_cxx_flags = std_cxx_flags + debug_flags0
    std_link_flags = std_link_flags + debug_flags
elif(mode=='profile'):
    std_cc_flags = std_cc_flags + profile_flags
    std_cxx_flags = std_cxx_flags + profile_flags
    std_link_flags = std_link_flags + profile_flags
else:
    std_cc_flags = std_cc_flags + optimization_flags
    std_cxx_flags = std_cxx_flags + optimization_flags
    std_link_flags = std_link_flags + optimization_flags

env = Environment(
    CFLAGS = ' '.join(std_cc_flags),
    CXXFLAGS = ' '.join(std_cxx_flags),
    CXX = 'clang++',
    LINKFLAGS = ' '.join(std_link_flags),
    CPPPATH = ['.',
               'src/lock',
               'src/datastructures',
               'src/tests',
               'src/utils',
               'src/benchmark/skiplist',
               'src/benchmark/pairingheap'])

num_of_cores_str=str(multiprocessing.cpu_count())

rg_define = ('NUMBER_OF_READER_GROUPS',num_of_cores_str)

hardware_threads_define = ('NUMBER_OF_HARDWARE_THREADS',num_of_cores_str)

#For partitioned ticket lock
array_size_define = ('ARRAY_SIZE',num_of_cores_str)

object_multi_writers_queue = env.Object("src/datastructures/multi_writers_queue.c")

object_opti_multi_writers_queue = env.Object("src/datastructures/opti_multi_writers_queue.c")

object_dr_multi_writers_queue = env.Object("src/datastructures/dr_multi_writers_queue.c")

object_thread_id = env.Object("src/utils/thread_identifier.c")

object_numa_node_info = env.Object(source = "src/utils/numa_node_info_support.c",
                                   CPPDEFINES = numa_structure_defines())

object_numa_ingress_egress_nzi = env.Object(source = "src/datastructures/numa_ingress_egress_nzi.c",
                                            CPPDEFINES = numa_structure_defines())

#NZI = Non Zero Indicator
nzi_infos = OrderedDict([
        ('rgnzi',      {'defines'     : ['NZI_TYPE_ReaderGroups', rg_define],
                        'deps'        : []}),
        ('nienzi',      {'defines'     : ['NZI_TYPE_NUMAIngressEgressCounter'
                                          ] + numa_structure_defines(),
                        'deps'        : [object_numa_node_info,
                                         object_numa_ingress_egress_nzi]})])

lock_infos = OrderedDict([
        ('sdw',        {'source'      : 'simple_delayed_writers_lock',
                        'defines'     : [rg_define],
                        'exe_defines' : ['LOCK_TYPE_SimpleDelayedWritesLock', 
                                         rg_define],
                        'lock_deps'   : [],
                        'other_deps'  : [object_multi_writers_queue,
                                         object_thread_id],
                        'uses_nzi'     : True}),
        ('aer',        {'source'      : 'all_equal_rdx_lock',
                        'defines'     : [rg_define],
                        'exe_defines' : ['LOCK_TYPE_AllEqualRDXLock',
                                         rg_define],
                        'lock_deps'   : [],
                        'other_deps'  : [object_multi_writers_queue,
                                         object_thread_id],
                        'uses_nzi'    : True}),
        ('ttsa',        {'source'     : 'tts_rdx_lock',
                         'defines'    : [rg_define],
                         'exe_defines': ['LOCK_TYPE_TTSRDXLock',
                                         rg_define],
                         'lock_deps'  : [],
                         'other_deps' : [object_opti_multi_writers_queue,
                                         object_thread_id],
                         'uses_nzi'   : True}),
        ('mcs',        {'source'      : 'mcs_lock',
                        'defines'     : [],
                        'exe_defines' : ['LOCK_TYPE_MCSLock'],
                        'lock_deps'   : [],
                        'other_deps'  : [],
                        'uses_nzi'    : False}),
        ('drmcs',      {'source'      : 'wprw_lock',
                        'defines'     : ['LOCK_TYPE_MCSLock',
                                         'LOCK_TYPE_WPRW_MCSLock',
                                         rg_define],
                        'exe_defines' : ['LOCK_TYPE_WPRWLock',
                                         'LOCK_TYPE_WPRW_MCSLock',
                                         rg_define],
                        'lock_deps'   : ['mcs'],
                        'other_deps'  : [object_thread_id],
                        'uses_nzi'     : True}),
        ('mcsrdx',     {'source'      : 'agnostic_rdx_lock',
                        'defines'     : ['LOCK_TYPE_MCSLock',
                                         'LOCK_TYPE_WPRW_MCSLock',
                                         rg_define],
                        'exe_defines' : ['LOCK_TYPE_AgnosticRDXLock',
                                         'LOCK_TYPE_WPRW_MCSLock',
                                         rg_define],
                        'lock_deps'   : ['mcs'],
                        'other_deps'  : [object_thread_id,
                                         object_opti_multi_writers_queue],
                        'uses_nzi'     : True}),
        ('tatas',      {'source'      : 'tatas_lock',
                        'defines'     : [],
                        'exe_defines' : ['LOCK_TYPE_TATASLock'],
                        'lock_deps'   : [],
                        'other_deps'  : [],
                        'uses_nzi'     : False}),
        ('tatasrdx',   {'source'      : 'agnostic_rdx_lock',
                        'defines'     : ['LOCK_TYPE_TATASLock',
                                         'LOCK_TYPE_WPRW_TATASLock',
                                         rg_define],
                        'exe_defines' : ['LOCK_TYPE_AgnosticRDXLock',
                                         'LOCK_TYPE_WPRW_TATASLock',
                                         rg_define],
                        'lock_deps'   : ['tatas'],
                        'other_deps'  : [object_thread_id,
                                         object_opti_multi_writers_queue],
                        'uses_nzi'     : True}),
        ('ticket',     {'source'      : 'ticket_lock',
                        'defines'     : [],
                        'exe_defines' : ['LOCK_TYPE_TicketLock'],
                        'lock_deps'   : [],
                        'other_deps'  : [],
                        'uses_nzi'     : False}),
        ('aticket',    {'source'      : 'aticket_lock',
                        'defines'     : [array_size_define],
                        'exe_defines' : ['LOCK_TYPE_ATicketLock',
                                         array_size_define],
                        'lock_deps'   : [],
                        'other_deps'  : [],
                        'uses_nzi'     : False}),
        ('cohort',     {'source'      : 'cohort_lock',
                        'defines'     : [array_size_define] + numa_structure_defines(),
                        'exe_defines' : ['LOCK_TYPE_CohortLock', 
                                         array_size_define] + numa_structure_defines(),
                        'lock_deps'   : ['ticket','aticket'],
                        'other_deps'  : [object_numa_node_info],
                        'uses_nzi'     : False}),
        ('wprwcohort', {'source'      : 'wprw_lock',
                        'defines'     : ['LOCK_TYPE_CohortLock', 
                                         'LOCK_TYPE_WPRW_CohortLock', 
                                         array_size_define, 
                                         rg_define] + numa_structure_defines(),
                        'exe_defines' : ['LOCK_TYPE_WPRWLock', 
                                         'LOCK_TYPE_WPRW_CohortLock', 
                                         array_size_define, 
                                         rg_define]  + numa_structure_defines(),
                        'lock_deps'   : ['cohort'],
                        'other_deps'  : [object_numa_node_info,
                                         object_thread_id],
                        'uses_nzi'    : True}),
        ('cohortrdx',  {'source'      : 'agnostic_rdx_lock',
                        'defines'     : ['LOCK_TYPE_CohortLock',
                                         'LOCK_TYPE_WPRW_CohortLock',
                                         array_size_define,
                                         rg_define] + numa_structure_defines(),
                        'exe_defines' : ['LOCK_TYPE_AgnosticRDXLock',
                                         'LOCK_TYPE_WPRW_CohortLock',
                                         array_size_define,
                                         rg_define]+ numa_structure_defines(),
                        'lock_deps'   : ['cohort'],
                        'other_deps'  : [object_thread_id,
                                         object_opti_multi_writers_queue,
                                         object_numa_node_info],
                        'uses_nzi'     : True}),
        ('fcrdx',        {'source'      : 'flat_comb_rdx_lock',
                          'defines'     : [rg_define],
                          'exe_defines' : ['LOCK_TYPE_FlatCombRDXLock',
                                           rg_define],
                          'lock_deps'   : [],
                          'other_deps'  : [object_thread_id],
                          'uses_nzi'     : True}),
        ('tatasdx',   {'source'      : 'agnostic_dx_lock',
                       'defines'     : ['LOCK_TYPE_TATASLock',
                                        'LOCK_TYPE_WPRW_TATASLock'],
                       'exe_defines' : ['LOCK_TYPE_AgnosticDXLock',
                                        'LOCK_TYPE_WPRW_TATASLock'],
                       'lock_deps'   : ['tatas'],
                       'other_deps'  : [object_thread_id,
                                        object_dr_multi_writers_queue],
                       'uses_nzi'     : False}),
        ('tatasfdx',   {'source'      : 'agnostic_fdx_lock',
                        'defines'     : ['LOCK_TYPE_TATASLock',
                                         'LOCK_TYPE_WPRW_TATASLock'],
                        'exe_defines' : ['LOCK_TYPE_AgnosticFDXLock',
                                         'LOCK_TYPE_WPRW_TATASLock'],
                        'lock_deps'   : ['tatas'],
                        'other_deps'  : [object_thread_id,
                                         object_dr_multi_writers_queue],
                        'uses_nzi'     : False})])

cpp_lock_infos = [('cpprdx',     {'source'      : 'cpprdx',
                                  'defines'     : [],
                                  'exe_defines' : ['LOCK_TYPE_CPPRDX'],
                                  'lock_deps'   : [],
                                  'other_deps'  : [],
                                  'uses_nzi'    : False})]

if use_cpp_locks:
    lock_infos.update(cpp_lock_infos)

lock_specific_object_defs = OrderedDict([
        ('test',             {'source'      : 'src/tests/test_rdx_lock.c',
                              'defines'     : [hardware_threads_define]}),
        ('rw_bench_clone',   {'source'      : 'src/benchmark/rw_bench_clone.c',
                             'defines'     : ['RW_BENCH_CLONE']}),
        #('rw_bench_memtrans',{'source'      : 'src/benchmark/rw_bench_clone.c',
        #                     'defines'     : ['RW_BENCH_MEM_TRANSFER']}),
        ('priority_queue_bench',   {'source'      : 'src/benchmark/priority_queue_bench.c',
                                    'defines'     : ['PAIRING_HEAP']})])


#Located in src/benchmark/
benchmarks_scripts = ['compare_benchmarks.py',
                      'benchmark_lock.py',
                      'run_benchmarks_on_intel_i7.py',
                      'run_benchmarks_on_sandy.py',
                      'run_benchmarks_on_amd_fx_6100.py']

#Located in src/profile/
profile_scripts = ['profile_perf.py']

#################
#Generate objects
#################

def create_lock_specific_object(lock_id, lock_specific_object_def_id, variant):
    definition = lock_specific_object_defs[lock_specific_object_def_id]
    return env.Object(
        target = lock_specific_object_def_id + '_' + variant['id'] + '.o',
        source = definition['source'],
        CPPDEFINES = lock_infos[lock_id]['exe_defines'] + definition['defines'] + variant['defines'])

for lock_id in lock_infos:
    lock_info = lock_infos[lock_id]
    other_deps = lock_info['other_deps']
    for lock_dep in lock_info['lock_deps']:
        other_deps.append(lock_infos[lock_dep]['variants'][0]['obj'])#Lock deps only alowed to have one variant
        other_deps.append(lock_infos[lock_dep]['other_deps'])
    ext = '.c'
    if lock_info['source'] == 'cpprdx':
        ext = '.cpp'
    if lock_info['uses_nzi']:
        lock_info['variants'] = []
        for nzi_id in nzi_infos:
            nzi_info = nzi_infos[nzi_id]
            variant_id = lock_id + '_' + nzi_id
            lock_info['variants'].append(
                {'id': variant_id,
                 'obj':env.Object(
                        target = lock_info['source'] + variant_id + '.o',
                        source = 'src/lock/' + lock_info['source'] + ext,
                        CPPDEFINES=lock_info['defines'] + nzi_info['defines']),
                 'deps': nzi_info['deps'],
                 'defines': nzi_info['defines']})
    else:
        lock_info['variants'] = [
            {'id': lock_id,
             'obj': env.Object(
                    target = lock_info['source'] + lock_id + '.o',
                    source = 'src/lock/' + lock_info['source'] + ext,
                    CPPDEFINES=lock_info['defines']),
             'deps': [],
             'defines': []}]
    for lock_specific_object_def_id in lock_specific_object_defs:
        for variant in lock_info['variants']:
            variant[lock_specific_object_def_id] = ( 
                create_lock_specific_object(lock_id,
                                            lock_specific_object_def_id,
                                            variant))


##############
#Link programs
##############

def create_lock_specific_program(lock_id, lock_specific_object_def_id):
    lock_info = lock_infos[lock_id]
    for variant in lock_info['variants']:
        env.Program(
            target = lock_specific_object_def_id + '_' + variant['id'],
            source = ([variant[lock_specific_object_def_id], 
                       variant['obj']] + 
                      lock_info['other_deps'] +
                      variant['deps']))

for lock_id in lock_infos:
    for lock_specific_object_def_id in lock_specific_object_defs:
        create_lock_specific_program(lock_id,
                                     lock_specific_object_def_id)


env.Program(
    target = 'test_multi_writers_queue',
    source = ['src/tests/test_multi_writers_queue.c',
              object_multi_writers_queue])

env.Program(
    target = 'test_pairingheap',
    source = ['src/benchmark/pairingheap/test_pairingheap.c'])

#############
#Copy scripts
#############

for benchmarks_script in benchmarks_scripts:
    Command(benchmarks_script, 
            'src/benchmark/' + benchmarks_script, 
            Copy("$TARGET", "$SOURCE"))


if(mode=='profile'):
    for profile_script in profile_scripts:
        Command(profile_script, 
                'src/profile/' + profile_script, 
                Copy("$TARGET", "$SOURCE"))
