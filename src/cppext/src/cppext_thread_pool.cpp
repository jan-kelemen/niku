#include <cppext_thread_pool.hpp>

thread_local cppext::detail::work_stealing_queue_t*
    cppext::thread_pool_t::local_work_queue_{};
thread_local unsigned cppext::thread_pool_t::local_index_{};
