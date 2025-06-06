#ifndef RESHED_TEXT_BUFFER_INCLUDED
#define RESHED_TEXT_BUFFER_INCLUDED

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace reshed
{
    struct [[nodiscard]] edit_point_t final
    {
        size_t byte;
        size_t line;
        size_t column;
    };

    class [[nodiscard]] text_buffer_t final
    {
    public:
        [[nodiscard]] std::pair<size_t, edit_point_t>
        add(size_t line, size_t column, std::string_view content);

        [[nodiscard]] std::pair<size_t, edit_point_t>
        remove(size_t line, size_t column, size_t count);

        [[nodiscard]] std::string_view line(size_t line,
            bool include_newline) const;

        [[nodiscard]] size_t lines() const;

    private:
        std::string buffer_;
    };
} // namespace reshed

#endif
