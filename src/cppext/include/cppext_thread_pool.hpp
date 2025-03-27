#ifndef CPPEXT_THREAD_POOL_INCLUDED
#define CPPEXT_THREAD_POOL_INCLUDED

#include <algorithm>
#include <atomic>
#include <concepts>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppext::detail
{
    template<typename ThreadContainer>
    class [[nodiscard]] join_threads_t final
    {
    public:
        explicit join_threads_t(ThreadContainer& threads);

        join_threads_t(join_threads_t const&) = delete;

        join_threads_t(join_threads_t&& other) noexcept;

    public:
        ~join_threads_t();

    public:
        join_threads_t& operator=(join_threads_t const&) = delete;

        join_threads_t& operator=(join_threads_t&& other) noexcept;

    private:
        ThreadContainer* threads_;
    };

    template<typename ThreadContainer>
    join_threads_t<ThreadContainer>::join_threads_t(ThreadContainer& threads)
        : threads_{&threads}
    {
    }

    template<typename ThreadContainer>
    join_threads_t<ThreadContainer>::join_threads_t(
        join_threads_t&& other) noexcept
        : threads_{std::exchange(other.threads_, nullptr)}
    {
    }

    template<typename ThreadContainer>
    join_threads_t<ThreadContainer>::~join_threads_t()
    {
        std::ranges::for_each(*threads_, std::mem_fn(&std::thread::join));
    }

    template<typename ThreadContainer>
    // cppcheck-suppress operatorEqVarError
    join_threads_t<ThreadContainer>& join_threads_t<ThreadContainer>::operator=(
        join_threads_t&& other) noexcept
    {
        std::swap(threads_, other.threads_);

        return *this;
    }

    template<typename T>
    class [[nodiscard]] threadsafe_queue_t final
    {
    public:
        threadsafe_queue_t() = default;

        threadsafe_queue_t(threadsafe_queue_t const&) = delete;

        threadsafe_queue_t(threadsafe_queue_t&&) noexcept = delete;

    public:
        ~threadsafe_queue_t() = default;

        [[nodiscard]] std::shared_ptr<T> try_pop();

        [[nodiscard]] bool try_pop(T& value);

        [[nodiscard]] std::shared_ptr<T> wait_and_pop();

        void wait_and_pop(T& value);

        void push(T new_value);

        [[nodiscard]] bool empty();

    public:
        threadsafe_queue_t& operator=(threadsafe_queue_t const&) = delete;

        threadsafe_queue_t& operator=(threadsafe_queue_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] node_t final
        {
            std::shared_ptr<T> data;
            std::unique_ptr<node_t> next;
        };

    private:
        [[nodiscard]] node_t* get_tail();

        [[nodiscard]] std::unique_ptr<node_t> pop_head();

        [[nodiscard]] std::unique_lock<std::mutex> wait_for_data();

        [[nodiscard]] std::unique_ptr<node_t> wait_pop_head();

        [[nodiscard]] std::unique_ptr<node_t> wait_pop_head(T& value);

        [[nodiscard]] std::unique_ptr<node_t> try_pop_head();

        [[nodiscard]] std::unique_ptr<node_t> try_pop_head(T& value);

    private:
        std::mutex head_mutex_;
        std::unique_ptr<node_t> head_{std::make_unique<node_t>()};

        std::mutex tail_mutex_;
        node_t* tail_{head_.get()};

        std::condition_variable data_cond_;
    };

    template<typename T>
    std::shared_ptr<T> threadsafe_queue_t<T>::try_pop()
    {
        std::unique_ptr old_head{try_pop_head()};
        return old_head ? old_head->data : nullptr;
    }

    template<typename T>
    bool threadsafe_queue_t<T>::try_pop(T& value)
    {
        std::unique_ptr const old_head{try_pop_head(value)};
        return static_cast<bool>(old_head);
    }

    template<typename T>
    std::shared_ptr<T> threadsafe_queue_t<T>::wait_and_pop()
    {
        std::unique_ptr old_head{wait_pop_head()};
        return old_head->data;
    }

    template<typename T>
    void threadsafe_queue_t<T>::wait_and_pop(T& value)
    {
        [[maybe_unused]] std::unique_ptr old_head{wait_pop_head(value)};
    }

    template<typename T>
    void threadsafe_queue_t<T>::push(T new_value)
    {
        auto new_data{std::make_shared<T>(std::move(new_value))};
        auto new_node{std::make_unique<node_t>()};
        {
            std::lock_guard const tail_lock{tail_mutex_};
            tail_->data = std::move(new_data);
            node_t* const new_tail{new_node.get()};
            tail_->next = std::move(new_node);
            tail_ = new_tail;
        }
        data_cond_.notify_one();
    }

    template<typename T>
    bool threadsafe_queue_t<T>::empty()
    {
        std::lock_guard head_lock{head_mutex_};
        return head_.get() == get_tail();
    }

    template<typename T>
    threadsafe_queue_t<T>::node_t* threadsafe_queue_t<T>::get_tail()
    {
        std::lock_guard const tail_lock{tail_mutex_};
        return tail_;
    }

    template<typename T>
    std::unique_ptr<typename threadsafe_queue_t<T>::node_t>
    threadsafe_queue_t<T>::pop_head()
    {
        std::unique_ptr old_head{std::move(head_)};
        head_ = std::move(old_head->next);
        return old_head;
    }

    template<typename T>
    std::unique_lock<std::mutex> threadsafe_queue_t<T>::wait_for_data()
    {
        std::unique_lock head_lock{head_mutex_};
        data_cond_.wait(head_lock, [&]() { return head_.get() != get_tail(); });
        return head_lock;
    }

    template<typename T>
    std::unique_ptr<typename threadsafe_queue_t<T>::node_t>
    threadsafe_queue_t<T>::wait_pop_head()
    {
        std::unique_lock const head_lock{wait_for_data()};
        return pop_head();
    }

    template<typename T>
    std::unique_ptr<typename threadsafe_queue_t<T>::node_t>
    threadsafe_queue_t<T>::wait_pop_head(T& value)
    {
        std::unique_lock const head_lock{wait_for_data()};
        value = std::move(*(head_->data));
        return pop_head();
    }

    template<typename T>
    std::unique_ptr<typename threadsafe_queue_t<T>::node_t>
    threadsafe_queue_t<T>::try_pop_head()
    {
        std::lock_guard const head_lock{head_mutex_};
        if (head_.get() == get_tail())
        {
            return nullptr;
        }
        return pop_head();
    }

    template<typename T>
    std::unique_ptr<typename threadsafe_queue_t<T>::node_t>
    threadsafe_queue_t<T>::try_pop_head(T& value)
    {
        std::lock_guard const head_lock{head_mutex_};
        if (head_.get() == get_tail())
        {
            return nullptr;
        }
        value = std::move(*(head_->data));
        return pop_head();
    }

    class [[nodiscard]] function_wrapper_t final
    {
    public:
        function_wrapper_t() = default;

        function_wrapper_t(function_wrapper_t const&) = delete;

        template<typename F>
        // cppcheck-suppress noExplicitConstructor
        function_wrapper_t(F&& f)
        requires(!std::same_as<F, function_wrapper_t>);

        function_wrapper_t(function_wrapper_t&& other) noexcept = default;

    public:
        ~function_wrapper_t() = default;

    public:
        void operator()();

        function_wrapper_t& operator=(function_wrapper_t const&) = delete;

        function_wrapper_t& operator=(
            function_wrapper_t&& other) noexcept = default;

    private:
        // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
        struct [[nodiscard]] impl_base_t
        {
            virtual void call() = 0;

            virtual ~impl_base_t() = default;
        };

        template<std::invocable Invocable>
        struct [[nodiscard]] impl_type_t final : impl_base_t
        {
            Invocable functor_;

            // cppcheck-suppress noExplicitConstructor
            impl_type_t(Invocable&& functor);

            void call() override;
        };

    private:
        std::unique_ptr<impl_base_t> impl_;
    };

    template<typename F>
    function_wrapper_t::function_wrapper_t(F&& f)
    requires(!std::same_as<F, function_wrapper_t>)
        : impl_{new impl_type_t<F>(std::forward<F>(f))}
    {
    }

    template<std::invocable Invocable>
    function_wrapper_t::impl_type_t<Invocable>::impl_type_t(Invocable&& functor)
        : functor_{std::move(functor)}
    {
    }

    template<std::invocable Invocable>
    void function_wrapper_t::impl_type_t<Invocable>::call()
    {
        std::invoke(functor_);
    }

    class [[nodiscard]] work_stealing_queue_t final
    {
    public:
        work_stealing_queue_t() = default;

        work_stealing_queue_t(work_stealing_queue_t const&) = delete;

        work_stealing_queue_t(work_stealing_queue_t&&) = delete;

    public:
        ~work_stealing_queue_t() = default;

    public:
        void push(function_wrapper_t data);

        [[nodiscard]] bool empty() const;

        [[nodiscard]] bool try_pop(function_wrapper_t& res);

        [[nodiscard]] bool try_steal(function_wrapper_t& res);

    public:
        work_stealing_queue_t& operator=(work_stealing_queue_t const&) = delete;

        work_stealing_queue_t& operator=(work_stealing_queue_t&&) = delete;

    private:
        mutable std::mutex mutex_;
        std::deque<function_wrapper_t> queue_;
    };

} // namespace cppext::detail

namespace cppext
{
    class [[nodiscard]] thread_pool_t final
    {
    public:
        explicit thread_pool_t(
            unsigned thread_count = std::thread::hardware_concurrency());

        thread_pool_t(thread_pool_t const&) = delete;

        thread_pool_t(thread_pool_t&&) = delete;

    public:
        ~thread_pool_t();

    public:
        [[nodiscard]] unsigned thread_count() const noexcept;

        template<typename FunctionType>
        std::future<std::invoke_result_t<FunctionType>> submit(FunctionType f);

        template<typename FunctionType>
        void set_thread_exit_function(FunctionType f);

    public:
        thread_pool_t& operator=(thread_pool_t const&) = delete;

        thread_pool_t& operator=(thread_pool_t&&) = delete;

    private:
        void worker_thread(unsigned index);

        [[nodiscard]] bool pop_task_from_local_queue(
            detail::function_wrapper_t& task);

        [[nodiscard]] bool pop_task_from_pool_queue(
            detail::function_wrapper_t& task);

        [[nodiscard]] bool pop_task_from_other_thread_queue(
            detail::function_wrapper_t& task);

        void run_pending_task();

    private:
        std::atomic_bool done_{false};

        detail::threadsafe_queue_t<detail::function_wrapper_t> pool_work_queue_;
        std::vector<std::unique_ptr<detail::work_stealing_queue_t>> queues_;

        detail::function_wrapper_t thread_exit_function_;

        std::vector<std::thread> threads_;
        detail::join_threads_t<std::vector<std::thread>> joiner_{threads_};

        static thread_local detail::work_stealing_queue_t* local_work_queue_;
        static thread_local unsigned local_index_;
    };

    template<typename FunctionType>
    std::future<std::invoke_result_t<FunctionType>> thread_pool_t::submit(
        FunctionType f)
    {
        using result_type = std::invoke_result_t<FunctionType>;

        std::packaged_task<result_type()> task{f};
        std::future<result_type> res{task.get_future()};
        if (local_work_queue_)
        {
            local_work_queue_->push(std::move(task));
        }
        else
        {
            pool_work_queue_.push(std::move(task));
        }
        return res;
    }

    template<typename FunctionType>
    void thread_pool_t::set_thread_exit_function(FunctionType f)
    {
        thread_exit_function_ = std::move(f);
    }
} // namespace cppext
#endif
