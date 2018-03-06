#include "qd.hpp"
#include "threadid.cpp"

//using intlock = extended_lock<tatas_lock>;
using intlock = mcs_futex_lock;
//using locktype = mrqdlock_impl<intlock, dual_buffer_queue<6144, 16, atomic_instruction_policy_t::use_fetch_and_add>, reader_groups<64>, 65536, starvation_policy_t::starvation_free>;
//using locktype = mrqdlock_impl<intlock, dual_buffer_queue<4096, 24, atomic_instruction_policy_t::use_fetch_and_add>, reader_groups<64>, 65536, starvation_policy_t::may_starve>;
using locktype = mrqdlock_impl<intlock, dual_buffer_queue<4096, 24, atomic_instruction_policy_t::use_fetch_and_add>, reader_groups<64>, 65536, starvation_policy_t::starvation_free>;

extern "C" {
#include "rcpp_lock.h"
#include "rcpp_lock.cpp"
} // extern "C"
