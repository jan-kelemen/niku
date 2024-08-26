#ifndef CPPEXT_CYCLED_BUFFER_INCLUDED
#define CPPEXT_CYCLED_BUFFER_INCLUDED

#include <cppext_numeric.hpp>

#include <algorithm>
#include <functional>
#include <span>
#include <type_traits>
#include <vector>

namespace cppext
{
    template<typename T, typename Container = std::vector<T>>
    struct [[nodiscard]] cycled_buffer_t final
    {
    public:
        using container_type = Container;
        using value_type = typename Container::value_type;
        using size_type = typename Container::size_type;
        using reference = typename Container::reference;
        using const_reference = typename Container::const_reference;

    public:
        cycled_buffer_t() = default;

        explicit cycled_buffer_t(size_type cycle, size_type size = {});

        cycled_buffer_t(cycled_buffer_t const&) = default;

        cycled_buffer_t(cycled_buffer_t&&) noexcept = default;

    public:
        ~cycled_buffer_t() = default;

    public:
        [[nodiscard]] T& current();

        [[nodiscard]] T const& current() const;

        [[nodiscard]] bool empty() const;

        [[nodiscard]] size_type index() const;

        // cppcheck-suppress functionConst
        void cycle();

        template<typename Action>
        auto cycle(Action&& action) -> std::invoke_result_t<Action, T&, T&>;

        void push(value_type const& value);

        void push(value_type&& value);

        template<class... Args>
        decltype(auto) emplace(Args&&... args);

        void pop();

        void swap(cycled_buffer_t& other) noexcept;

        [[nodiscard]] T* data();

        [[nodiscard]] T const* data() const;

        // cppcheck-suppress [functionConst, returnByReference]
        [[nodiscard]] std::span<T> as_span();

        // cppcheck-suppress returnByReference
        [[nodiscard]] std::span<T const> as_span() const;

    public:
        [[nodiscard]] T& operator*();

        [[nodiscard]] T const& operator*() const;

        [[nodiscard]] T* operator->();

        [[nodiscard]] T const* operator->() const;

        cycled_buffer_t& operator=(cycled_buffer_t const&) = default;

        cycled_buffer_t& operator=(cycled_buffer_t&&) noexcept = default;

    private:
        [[nodiscard]] Container::size_type next_index() const;

    private:
        Container data_;
        Container::size_type cycle_{};
        Container::size_type index_{};
    };

    template<typename T, typename Container>
    cycled_buffer_t<T, Container>::cycled_buffer_t(size_type const cycle,
        size_type const size)
        : cycle_{cycle}
    {
        data_.resize(size);
    }

    template<typename T, typename Container>
    T& cycled_buffer_t<T, Container>::current()
    {
        return data_[index_];
    }

    template<typename T, typename Container>
    T const& cycled_buffer_t<T, Container>::current() const
    {
        return data_[index_];
    }

    template<typename T, typename Container>
    bool cycled_buffer_t<T, Container>::empty() const
    {
        return data_.empty();
    }

    template<typename T, typename Container>
    cycled_buffer_t<T, Container>::size_type
    cycled_buffer_t<T, Container>::index() const
    {
        return index_;
    }

    template<typename T, typename Container>
    void cycled_buffer_t<T, Container>::cycle()
    {
        index_ = next_index();
    }

    template<typename T, typename Container>
    template<typename Action>
    auto cycled_buffer_t<T, Container>::cycle(
        Action&& action) -> std::invoke_result_t<Action, T&, T&>
    {
        auto const next{next_index()};

        if constexpr (!std::is_void_v<std::invoke_result_t<Action, T&, T&>>)
        {
            auto rv{std::invoke(std::forward<Action>(action),
                data_[index_],
                data_[next])};
            index_ = next;
            return rv;
        }
        else
        {
            std::invoke(std::forward<Action>(action),
                data_[index_],
                data_[next]);
            index_ = next;
        }
    }

    template<typename T, typename Container>
    void cycled_buffer_t<T, Container>::push(value_type const& value)
    {
        data_.push_back(value);
    }

    template<typename T, typename Container>
    void cycled_buffer_t<T, Container>::push(value_type&& value)
    {
        data_.push_back(std::move(value));
    }

    template<typename T, typename Container>
    template<class... Args>
    decltype(auto) cycled_buffer_t<T, Container>::emplace(Args&&... args)
    {
        return data_.emplace_back(std::forward<Args>(args)...);
    }

    template<typename T, typename Container>
    void cycled_buffer_t<T, Container>::pop()
    {
        auto const current_it{std::next(begin(data_),
            cppext::narrow<typename Container::difference_type>(index_))};
        std::move(std::next(current_it, 1), end(data_), current_it);
    }

    template<typename T, typename Container>
    void cycled_buffer_t<T, Container>::swap(cycled_buffer_t& other) noexcept
    {
        using std::swap;
        swap(data_, other.data_);
        swap(cycle_, other.cycle_);
        swap(index_, other.index_);
    }

    template<typename T, typename Container>
    T* cycled_buffer_t<T, Container>::data()
    {
        return data_.data();
    }

    template<typename T, typename Container>
    T const* cycled_buffer_t<T, Container>::data() const
    {
        return data_.data();
    }

    template<typename T, typename Container>
    std::span<T> cycled_buffer_t<T, Container>::as_span()
    {
        return data_;
    }

    template<typename T, typename Container>
    std::span<T const> cycled_buffer_t<T, Container>::as_span() const
    {
        return data_;
    }

    template<typename T, typename Container>
    T& cycled_buffer_t<T, Container>::operator*()
    {
        return data_[index_];
    }

    template<typename T, typename Container>
    T const& cycled_buffer_t<T, Container>::operator*() const
    {
        return data_[index_];
    }

    template<typename T, typename Container>
    T* cycled_buffer_t<T, Container>::operator->()
    {
        return &data_[index_];
    }

    template<typename T, typename Container>
    T const* cycled_buffer_t<T, Container>::operator->() const
    {
        return &data_[index_];
    }

    template<typename T, typename Container>
    Container::size_type cycled_buffer_t<T, Container>::next_index() const
    {
        return (index_ + 1) % cycle_;
    }
} // namespace cppext

#endif
