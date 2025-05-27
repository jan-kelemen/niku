#include <ngntxt_syntax.hpp>

#include <cppext_numeric.hpp>

#include <spdlog/spdlog.h>

ngntxt::parser_handle_t ngntxt::create_parser()
{
    ngntxt::parser_handle_t rv{ts_parser_new()};

    return rv;
}

ngntxt::query_handle_t ngntxt::create_query(language_handle_t const& language,
    std::string_view pattern)
{
    uint32_t error_offset;
    TSQueryError error_type;
    ngntxt::query_handle_t rv{ts_query_new(language.get(),
        pattern.data(),
        cppext::narrow<uint32_t>(pattern.size()),
        &error_offset,
        &error_type)};
    if (!rv)
    {
        spdlog::error("Query error of type {} on offset {}",
            std::to_underlying(error_type),
            error_offset);
    }

    return rv;
}

ngntxt::query_cursor_handle_t ngntxt::execute_query(query_handle_t const& query,
    TSNode node)
{
    ngntxt::query_cursor_handle_t rv{ts_query_cursor_new()};

    ts_query_cursor_exec(rv.get(), query.get(), node);

    return rv;
}

std::optional<ngntxt::query_match_t> ngntxt::next_match(
    query_cursor_handle_t& handle)
{
    TSQueryMatch match;
    if (!ts_query_cursor_next_match(handle.get(), &match))
    {
        return std::nullopt;
    }

    return std::make_optional<ngntxt::query_match_t>(match.id,
        match.pattern_index,
        std::span{match.captures, match.capture_count});
}

bool ngntxt::set_language(parser_handle_t& parser,
    language_handle_t const& language)
{
    return ts_parser_set_language(parser.get(), language.get());
}

ngntxt::tree_handle_t ngntxt::parse(parser_handle_t& parser,
    tree_handle_t const& old_tree,
    std::function<std::string_view(size_t, size_t, size_t)> read)
{
    return ngntxt::tree_handle_t{ts_parser_parse(parser.get(),
        old_tree.get(),
        TSInput{
            .payload = &read,
            .read =
                [](void* const payload,
                    uint32_t const byte_index,
                    TSPoint const position,
                    uint32_t* const bytes_read)
            {
                auto* const cb{static_cast<
                    std::function<std::string_view(size_t, size_t, size_t)>*>(
                    payload)};

                std::string_view const buffer{
                    (*cb)(byte_index, position.row, position.column)};

                *bytes_read = cppext::narrow<uint32_t>(buffer.size());
                return buffer.data();
            },
            .encoding = TSInputEncodingUTF8,
            .decode = nullptr,
        })};
}
