#include "qd.hpp"

using intlock = mcs_lock;
//using locktype = qdlock_impl<intlock, buffer_queue<262139>>;
//using locktype = qdlock_impl<intlock, buffer_queue<4096*40>>;
//using locktype = qdlock_impl<intlock, dual_buffer_queue<12288, 8>>;
//using locktype = qdlock_impl<intlock, dual_buffer_queue<2048, 64>>;

//using locktype = qdlock_impl<intlock, dual_buffer_queue<4096, 32>>;
using locktype = qdlock_impl<intlock, dual_buffer_queue<6144, 16, atomic_instruction_policy_t::use_fetch_and_add>, starvation_policy_t::starvation_free>;


//using locktype = qdlock_impl<intlock, dual_buffer_queue<24576, 4>>;
//using locktype = qdlock_impl<intlock, entry_queue<4096,96>>;

extern "C" {
#include "cpplock.h"

#include "cpplock.cpp"

} // extern "C"
