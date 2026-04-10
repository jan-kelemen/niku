#include <cppext_read_file.hpp>

#include <cstddef>
#include <fstream>
#include <stdexcept>

std::vector<char> cppext::read_file(std::filesystem::path const& file)
{
    std::ifstream stream{file, std::ios::ate | std::ios::binary};

    if (!stream.is_open())
    {
        throw std::runtime_error{"failed to open file!"};
    }

    auto const eof{stream.tellg()};

    std::vector<char> buffer(static_cast<size_t>(eof));
    stream.seekg(0);

    stream.read(buffer.data(), eof);

    return buffer;
}
