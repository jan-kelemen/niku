#include <cppext_hash.hpp>

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/md5.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>

// IWYU pragma: no_include <boost/hash2/digest.hpp>
// IWYU pragma: no_include <boost/hash2/flavor.hpp>

namespace
{
    [[nodiscard]] constexpr auto hash(auto const& v)
    {
        boost::hash2::md5_128 h1;
        boost::hash2::hash_append(h1, {}, v);
        return h1.result();
    }
} // namespace

TEST_CASE("std::optional is hashable", "[cppext][hash]")
{
    SECTION("std::optional<int>")
    {
        CHECK(hash(std::optional<int>{std::nullopt}) ==
            hash(std::optional<int>{std::nullopt}));
        CHECK(hash(std::optional{1}) == hash(std::optional{1}));

        CHECK(hash(std::optional{1}) != hash(std::optional{2}));
        CHECK(hash(std::optional{1}) != hash(std::optional<int>{std::nullopt}));
    }

    SECTION("std::optional<float>")
    {
        CHECK(hash(std::optional{0.0f}) == hash(std::optional{0.0f}));
        CHECK(hash(std::optional{1.0f}) == hash(std::optional{1.0f}));

        CHECK(hash(std::optional{0.0f}) != hash(std::optional{1.0f}));
    }
}
