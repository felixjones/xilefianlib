/*
===============================================================================

 Copyright (C) 2024 Felix Jones
 For conditions of distribution and use, see copyright notice in LICENSE

===============================================================================
*/

#pragma once

#include <algorithm>
#include <functional>

namespace xilefian {

    template <typename T, class Compare = std::less<T>, class Container = std::vector<T>>
    class bheap {
    public:
        using container_type = Container;
        using value_type = typename Container::value_type;
        using size_type = typename Container::size_type;
        using reference = typename Container::reference;
        using const_reference = typename Container::const_reference;
        using iterator = typename Container::iterator;

        [[nodiscard]]
        constexpr bool empty() const noexcept {
            return m_heap.empty();
        }

        [[nodiscard]]
        constexpr size_type size() const noexcept {
            return m_heap.size();
        }

        constexpr reference front() noexcept {
            return m_heap.front();
        }

        constexpr const_reference front() const noexcept {
            return m_heap.front();
        }

        template <typename... Args>
        constexpr iterator emplace(Args&&... args) noexcept {
            m_heap.emplace_back(std::forward<Args>(args)...);
            return fix_heap();
        }

        constexpr iterator push(const value_type& value) noexcept {
            m_heap.push_back(value);
            return fix_heap();
        }

        constexpr iterator push(value_type&& value) noexcept {
            m_heap.push_back(value);
            return fix_heap();
        }

        constexpr void pop() noexcept {
            std::iter_swap(m_heap.begin(), std::prev(m_heap.end())); // Move end to front
            m_heap.pop_back();
            fix_heap_recursive(m_heap.begin());
        }

        constexpr void swap(bheap& other) noexcept {
            std::swap(m_heap, other.m_heap);
        }

        constexpr bool operator==(const bheap& rhs) const noexcept {
            return m_heap == rhs.m_heap;
        }

        constexpr bool operator<=(const bheap& rhs) const noexcept {
            return m_heap <= rhs.m_heap;
        }

        constexpr bool operator>=(const bheap& rhs) const noexcept {
            return m_heap >= rhs.m_heap;
        }

        constexpr bool operator<(const bheap& rhs) const noexcept {
            return m_heap < rhs.m_heap;
        }

        constexpr bool operator>(const bheap& rhs) const noexcept {
            return m_heap > rhs.m_heap;
        }
    private:
        constexpr iterator fix_heap() noexcept {
            auto it = std::prev(m_heap.end());
            auto parent = decltype(it){};
            while (it != m_heap.begin()) {
                parent = iterator_parent(it);
                if (!m_comparator(*parent, *it)) {
                    break;
                }
                std::iter_swap(parent, it);
                it = parent;
            }
            return it;
        }

        constexpr void fix_heap_recursive(iterator it) noexcept {
            auto positive = iterator_positive(it);
            auto negative = iterator_negative(it);
            auto largest = it;

            if (positive != m_heap.end() && m_comparator(*largest, *positive)) {
                largest = positive;
            }

            if (negative != m_heap.end() && m_comparator(*largest, *negative)) {
                largest = negative;
            }

            if (largest != it) {
                std::iter_swap(it, largest);
                fix_heap_recursive(largest);
            }
        }

        constexpr iterator iterator_parent(iterator it) noexcept {
            const auto idx = (std::distance(m_heap.begin(), it) - 1) / 2;
            if (idx >= m_heap.size()) {
                return m_heap.end();
            }
            return std::next(m_heap.begin(), idx);
        }

        constexpr iterator iterator_positive(iterator it) noexcept {
            const auto idx = std::distance(m_heap.begin(), it) * 2 + 1;
            if (idx >= m_heap.size()) {
                return m_heap.end();
            }
            return std::next(m_heap.begin(), idx);
        }

        constexpr iterator iterator_negative(iterator it) noexcept {
            const auto idx = std::distance(m_heap.begin(), it) * 2 + 2;
            if (idx >= m_heap.size()) {
                return m_heap.end();
            }
            return std::next(m_heap.begin(), idx);
        }

        container_type m_heap;
        Compare m_comparator;
    };

}

namespace std {

    template <typename T, class Compare, class Container>
    constexpr void swap(xilefian::bheap<T, Compare, Container>& lhs, xilefian::bheap<T, Compare, Container>& rhs) noexcept {
    lhs.swap(rhs);
}

}
