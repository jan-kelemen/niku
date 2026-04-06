#include <cppext_hash_adapter.hpp>

#include <cppext_hash.hpp>

#include <boost/hash2/md5.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <unordered_set>

// IWYU pragma: no_include <boost/hash2/digest.hpp>
// IWYU pragma: no_include <boost/hash2/flavor.hpp>
// IWYU pragma: no_include <utility>
// IWYU pragma: no_include <__hash_table>

namespace
{
    using hasher_t =
        cppext::hash_adapter_t<std::optional<int>, boost::hash2::md5_128>;
} // namespace

TEST_CASE("hash_adapter can be used for unordered container hashes",
    "[cppext][hash_adapter]")
{
    std::unordered_set<std::optional<int>, hasher_t> container;

    {
        auto const& [it, inserted] = container.emplace(1);
        REQUIRE(inserted);
        CHECK(*it == 1);
    }

    {
        auto const& [it, inserted] = container.emplace(1);
        REQUIRE_FALSE(inserted);
        CHECK(*it == 1);
    }

    {
        auto const& [it, inserted] = container.emplace(2);
        REQUIRE(inserted);
        CHECK(*it == 2);
    }

    {
        auto const& [it, inserted] = container.emplace(std::nullopt);
        REQUIRE(inserted);
        CHECK_FALSE(*it);
    }

    CHECK(container.size() == 3);
}
