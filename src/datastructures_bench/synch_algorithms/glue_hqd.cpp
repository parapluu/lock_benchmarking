#include "qd.hpp"

using intlock = mcs_lock;
using locktype = hqdlock_impl<intlock, intlock, dual_buffer_queue<6144, 16, atomic_instruction_policy_t::use_fetch_and_add>, pinning_policy_t::pinned_threads, starvation_policy_t::starvation_free>;

extern "C" {
#include "cpplock.h"
#include "cpplock.cpp"
} // extern "C"
