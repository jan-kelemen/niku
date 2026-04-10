#ifndef CPPEXT_READ_FILE_INCLUDED
#define CPPEXT_READ_FILE_INCLUDED

#include <filesystem>
#include <vector>

namespace cppext
{
    [[nodiscard]] std::vector<char> read_file(
        std::filesystem::path const& file);
} // namespace cppext

#endif
