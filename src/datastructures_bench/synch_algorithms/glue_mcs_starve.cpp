#include "qd.hpp"

using intlock = mcs_lock;
using locktype = qdlock_impl<intlock, dual_buffer_queue<6144, 16, atomic_instruction_policy_t::use_fetch_and_add>, starvation_policy_t::may_starve>;

extern "C" {
#include "cpplock.h"
#include "cpplock.cpp"
} // extern "C"
