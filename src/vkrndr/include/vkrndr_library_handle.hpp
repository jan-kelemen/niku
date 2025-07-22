#ifndef VKRNDR_LIBRARY_HANDLE_INCLUDED
#define VKRNDR_LIBRARY_HANDLE_INCLUDED

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include <expected>
#include <system_error>

namespace vkrndr
{
    struct [[nodiscard]] library_handle_t final
        : boost::intrusive_ref_counter<library_handle_t>
    {
    public:
        library_handle_t() = default;

        library_handle_t(library_handle_t const&) = delete;

        library_handle_t(library_handle_t&&) noexcept = delete;

    public:
        ~library_handle_t();

    public:
        library_handle_t& operator=(library_handle_t const&) = delete;

        library_handle_t& operator=(library_handle_t&&) noexcept = delete;
    };

    using library_handle_ptr_t = boost::intrusive_ptr<library_handle_t>;

    [[nodiscard]] std::expected<library_handle_ptr_t, std::error_code>
    initialize();
} // namespace vkrndr

#endif
