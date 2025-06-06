#include <text_buffer.hpp>

#include <cppext_numeric.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <ranges>
#include <utility>

std::pair<size_t, reshed::edit_point_t>
reshed::text_buffer_t::add(size_t line, size_t column, std::string_view content)
{
    auto line_range{std::views::split(buffer_, '\n') | std::views::drop(line)};

    using diff_t = std::iter_difference_t<decltype(line_range.front().begin())>;

    size_t edit_start{};

    if (line_range.empty())
    {
        edit_start = buffer_.size();
        buffer_ += content;
    }
    else
    {
        auto const it{std::next(line_range.front().begin(),
            cppext::narrow<diff_t>(column))};
        edit_start = cppext::narrow<size_t>(std::distance(buffer_.begin(), it));

        buffer_.insert(it, content.cbegin(), content.cend());
    }

    auto const newlines_in_content{
        static_cast<size_t>(std::ranges::count(content, '\n'))};
    auto edit_end_column{column + content.size()};
    if (newlines_in_content != 0)
    {
        edit_end_column = content.rfind('\n');
    }

    return std::make_pair(edit_start,
        reshed::edit_point_t{.byte = edit_start + content.size(),
            .line = line + newlines_in_content,
            .column = edit_end_column});
}

std::pair<size_t, reshed::edit_point_t>
reshed::text_buffer_t::remove(size_t line, size_t column, size_t count)
{
    auto line_range{std::views::split(buffer_, '\n') | std::views::drop(line)};
    using diff_t = std::iter_difference_t<decltype(line_range.front().begin())>;

    auto end_it{
        std::next(line_range.front().begin(), cppext::narrow<diff_t>(column))};

    auto const edit_start{
        cppext::narrow<size_t>(std::distance(buffer_.begin(), end_it))};

    std::string_view const erased{end_it - cppext::narrow<diff_t>(count),
        end_it};
    auto const newlines_in_content{
        static_cast<size_t>(std::ranges::count(erased, '\n'))};
    assert(!newlines_in_content);

    buffer_.erase(end_it - cppext::narrow<diff_t>(count), end_it);

    return std::make_pair(edit_start,
        reshed::edit_point_t{.byte = edit_start - count,
            .line = line - newlines_in_content,
            .column = column - 1});
}

std::string_view reshed::text_buffer_t::line(size_t line,
    bool include_newline) const
{
    auto line_range{std::views::split(buffer_, '\n') | std::views::drop(line)};

    auto end_it{line_range.front().end()};
    if (include_newline && end_it != buffer_.end())
    {
        std::advance(end_it, 1);
    }

    return {line_range.front().begin(), end_it};
}

size_t reshed::text_buffer_t::lines() const
{
    return static_cast<size_t>(std::ranges::count(buffer_, '\n')) + 1;
}
