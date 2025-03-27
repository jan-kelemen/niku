#include <cppext_thread_pool.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

TEST_CASE("thread_pool", "[cppext][mt]")
{
    unsigned const thread_count{10};

    std::atomic_uint8_t count{};
    std::atomic_uint8_t exit_count{};

    {
        cppext::thread_pool_t pool{thread_count};
        CHECK(pool.thread_count() == thread_count);

        pool.set_thread_exit_function([&exit_count]() { ++exit_count; });

        std::vector<std::future<void>> results;
        for (unsigned i{0}; i != thread_count - 1; ++i)
        {
            results.push_back(pool.submit(
                [&count, &pool]()
                {
                    pool.submit([&count]() { ++count; });

                    std::this_thread::sleep_for(std::chrono::seconds{1});

                    ++count;
                }));
        }

        for (unsigned i{0}; i != thread_count; ++i)
        {
            results.push_back(pool.submit([&count]() { ++count; }));
        }

        for (auto&& r : results)
        {
            r.get();
        }
    }

    CHECK(count == 3 * thread_count - 2);
    CHECK(exit_count == thread_count);
}
