#include <cppext_thread_pool.hpp>

#include <iterator>

void cppext::detail::function_wrapper_t::operator()()
{
    if (impl_)
    {
        impl_->call();
    }
}

void cppext::detail::work_stealing_queue_t::push(function_wrapper_t data)
{
    std::scoped_lock const lock{mutex_};
    queue_.push_front(std::move(data));
}

bool cppext::detail::work_stealing_queue_t::empty() const
{
    std::scoped_lock const lock{mutex_};
    return queue_.empty();
}

bool cppext::detail::work_stealing_queue_t::try_pop(function_wrapper_t& res)
{
    std::scoped_lock const lock{mutex_};
    if (queue_.empty())
    {
        return false;
    }

    res = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

bool cppext::detail::work_stealing_queue_t::try_steal(function_wrapper_t& res)
{
    std::scoped_lock const lock{mutex_};
    if (queue_.empty())
    {
        return false;
    }
    res = std::move(queue_.back());
    queue_.pop_back();
    return true;
}

cppext::thread_pool_t::thread_pool_t(unsigned const thread_count)
{
    auto const tc{std::max(1u, thread_count)};

    try
    {
        std::generate_n(std::back_inserter(queues_),
            tc,
            std::make_unique<detail::work_stealing_queue_t>);

        for (unsigned i{}; i != tc; ++i)
        {
            threads_.emplace_back(&thread_pool_t::worker_thread, this, i);
        }
    }
    catch (...)
    {
        done_ = true;
        throw;
    }
}

cppext::thread_pool_t::~thread_pool_t() { done_ = true; }

unsigned cppext::thread_pool_t::thread_count() const noexcept
{
    return static_cast<unsigned>(threads_.size());
}

void cppext::thread_pool_t::worker_thread(unsigned index)
{
    local_index_ = index;
    local_work_queue_ = queues_[local_index_].get();
    while (!done_)
    {
        run_pending_task();
    }

    thread_exit_function_();
}

bool cppext::thread_pool_t::pop_task_from_local_queue(
    detail::function_wrapper_t& task)
{
    return static_cast<bool>(local_work_queue_) &&
        local_work_queue_->try_pop(task);
}

bool cppext::thread_pool_t::pop_task_from_pool_queue(
    detail::function_wrapper_t& task)
{
    return pool_work_queue_.try_pop(task);
}

bool cppext::thread_pool_t::pop_task_from_other_thread_queue(
    detail::function_wrapper_t& task)
{
    for (unsigned i{}; i != queues_.size(); ++i)
    {
        auto const index{(local_index_ + i + 1) % queues_.size()};
        if (queues_[index]->try_steal(task))
        {
            return true;
        }
    }
    return false;
}

void cppext::thread_pool_t::run_pending_task()
{
    detail::function_wrapper_t task;
    if (pop_task_from_local_queue(task) ||
        pop_task_from_pool_queue(task) ||
        pop_task_from_other_thread_queue(task))
    {
        task();
    }
    else
    {
        std::this_thread::yield();
    }
}

thread_local cppext::detail::work_stealing_queue_t*
    cppext::thread_pool_t::local_work_queue_{};
thread_local unsigned cppext::thread_pool_t::local_index_{};
