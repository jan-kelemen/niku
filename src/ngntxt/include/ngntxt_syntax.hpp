#ifndef NGNTXT_SYNTAX_INCLUDED
#define NGNTXT_SYNTAX_INCLUDED

#include <cppext_memory.hpp>

#include <tree_sitter/api.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

namespace ngntxt
{
    using parser_handle_t =
        cppext::unique_ptr_with_static_deleter_t<TSParser, ts_parser_delete>;

    using language_handle_t =
        cppext::unique_ptr_with_static_deleter_t<TSLanguage const,
            ts_language_delete>;

    using tree_handle_t =
        cppext::unique_ptr_with_static_deleter_t<TSTree, ts_tree_delete>;

    using query_handle_t =
        cppext::unique_ptr_with_static_deleter_t<TSQuery, ts_query_delete>;

    using query_cursor_handle_t =
        cppext::unique_ptr_with_static_deleter_t<TSQueryCursor,
            ts_query_cursor_delete>;

    [[nodiscard]] parser_handle_t create_parser();

    [[nodiscard]] query_handle_t create_query(language_handle_t const& language,
        std::string_view pattern);

    [[nodiscard]] query_cursor_handle_t
    execute_query(query_handle_t const& query, TSNode node);

    struct [[nodiscard]] query_match_t final
    {
        uint32_t id;
        uint16_t pattern_index;
        std::span<TSQueryCapture const> captures;
    };

    [[nodiscard]] std::optional<query_match_t> next_match(
        query_cursor_handle_t& handle);

    [[nodiscard]] bool set_language(parser_handle_t& parser,
        language_handle_t const& language);

    [[nodiscard]] tree_handle_t parse(parser_handle_t& parser,
        tree_handle_t const& old_tree,
        std::function<std::string_view(size_t, size_t, size_t)> read);

    struct [[nodiscard]] edit_point_t final
    {
        size_t byte;
        size_t row;
        size_t column;
    };

    struct [[nodiscard]] input_edit_t final
    {
        edit_point_t start;
        edit_point_t old_end;
        edit_point_t new_end;
    };

    [[nodiscard]] tree_handle_t edit(parser_handle_t& parser,
        tree_handle_t& tree,
        input_edit_t const& input_edit,
        std::function<std::string_view(size_t, size_t, size_t)> read);

    [[nodiscard]] std::vector<std::string_view> capture_names(
        query_handle_t& query);
} // namespace ngntxt
#endif
