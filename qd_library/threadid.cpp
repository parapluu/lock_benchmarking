#include "threadid.hpp"

unsigned long thread_id_store::max_id = 0;
std::set<unsigned long> thread_id_store::orphans;
std::mutex thread_id_store::mutex;

thread_local thread_id_t thread_id;
