#include <cppext_cycled_buffer.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("cycled_buffer", "[cppext][container]")
{
    cppext::cycled_buffer_t<int> buffer(3);

    CHECK(buffer.empty());

    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    CHECK_FALSE(buffer.empty());

    CHECK(buffer.current() == 1);
    CHECK(*buffer == 1);

    buffer.cycle();
    CHECK(buffer.current() == 2);
    CHECK(*buffer == 2);

    buffer.cycle();
    CHECK(buffer.current() == 3);
    CHECK(*buffer == 3);

    buffer.cycle();
    CHECK(buffer.current() == 1);
    CHECK(*buffer == 1);

    buffer.cycle();
    CHECK(buffer.current() == 2);
    CHECK(*buffer == 2);

    buffer.pop();
    CHECK(buffer.current() == 3);
    CHECK(*buffer == 3);
}

TEST_CASE("cycled_buffer with user defined type", "[cppext][container]")
{
    struct user_type_t
    {
        int value;
    };

    cppext::cycled_buffer_t<user_type_t> buffer(3);

    CHECK(buffer.empty());

    buffer.emplace(1);
    buffer.emplace(2);
    buffer.emplace(3);

    CHECK(buffer->value == 1);
    CHECK((*buffer).value == 1);

    buffer.cycle();
    CHECK(buffer->value == 2);
    CHECK((*buffer).value == 2);

    buffer.cycle();
    CHECK(buffer->value == 3);
    CHECK((*buffer).value == 3);
}

TEST_CASE("cycled_buffer with cycling action", "[cppext][container]")
{
    cppext::cycled_buffer_t<int> buffer(3);

    CHECK(buffer.empty());

    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    CHECK(*buffer == 1);
    auto const replace_current_with_next = [](int& current, int const& next)
    { current = next; };
    buffer.cycle(replace_current_with_next); // 2, [2], 3
    CHECK(*buffer == 2);
    buffer.cycle(); // 2, 2, [3]
    CHECK(*buffer == 3);
    buffer.cycle(); // [2], 2, 3
    CHECK(*buffer == 2);

    auto const return_current = [](int const& current, int const&)
    { return current; };
    CHECK(buffer.cycle(return_current) == 2); // 2, [2], 3
    CHECK(buffer.cycle(return_current) == 2); // 2, 2, [3]
    CHECK(buffer.cycle(return_current) == 3); // [2], 2, 3
}
