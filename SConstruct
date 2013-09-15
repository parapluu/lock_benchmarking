
#Locks and lock benchmarks

AddOption('--cpp_locks',
          action='store_true',
          dest='cpp_locks',
          default=False)

AddOption('--llvm',
          action='store_true',
          dest='use_llvm',
          default=False)

AddOption('--use_cas_fetch_and_add',
          action='store_true',
          dest='use_cas_fetch_and_add',
          default=False)

AddOption('--use_pinning',
          action='store_true',
          dest='use_pinning',
          default=False)

AddOption('--use_queue_stats',
          action='store_true',
          dest='use_queue_stats',
          default=False)

AddOption('--use_print_thread_queue_stats',
          action='store_true',
          dest='use_print_thread_queue_stats',
          default=False)

mode = 'release'

SConscript('SConscript.py', variant_dir='bin', duplicate=0, exports='mode')

mode = 'debug'

SConscript('SConscript.py', variant_dir='bin_debug', duplicate=0, exports='mode')

mode = 'profile'

SConscript('SConscript.py', variant_dir='bin_profile', duplicate=0, exports='mode')
