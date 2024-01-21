/*
===============================================================================

 Copyright (C) 2023 Felix Jones
 For conditions of distribution and use, see copyright notice in LICENSE

===============================================================================
*/

#pragma once

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>

namespace xilefian {

    template <class Allocator = std::allocator<bool>>
    class bvec {
    public:
        using size_type = std::size_t;
        using allocator_type = Allocator;
    private:
        using block_type = std::conditional_t<__SIZEOF_POINTER__ == 8, std::uint64_t, std::uint32_t>;
        static constexpr auto block_digits = static_cast<size_type>(std::numeric_limits<block_type>::digits);
        static constexpr auto block_mask = static_cast<block_type>(~0);

        static constexpr auto block_round(auto x) noexcept {
            return (x + (block_digits - 1)) / block_digits;
        }
    public:
        constexpr explicit bvec(const Allocator& alloc = Allocator()) noexcept : m_data{}, m_wordAllocator{alloc} {}

        constexpr bvec(size_type count, bool value, const Allocator& alloc = Allocator()) noexcept : m_data{}, m_wordAllocator{alloc} {
            resize(count, value);
        }

        explicit constexpr bvec(size_type count, const Allocator& alloc = Allocator()) noexcept : m_data{}, m_wordAllocator{alloc} {
            resize(count);
        }

        template <class InputIt>
        explicit constexpr bvec(InputIt first, InputIt last, const Allocator& alloc = Allocator()) noexcept : m_data{}, m_wordAllocator{alloc} {
            assign(first, last);
        }

        constexpr bvec(const bvec& other) noexcept : m_data{}, m_wordAllocator{other.m_wordAllocator} {
            assign(other.cbegin(), other.cend());
        }

        constexpr bvec(const bvec& other, const Allocator& alloc) noexcept : m_data{}, m_wordAllocator{alloc} {
            assign(other.cbegin(), other.cend());
        }

        constexpr bvec(bvec&& other) noexcept : m_data{other.m_data}, m_wordAllocator{other.m_wordAllocator} {
            other.m_data.stack = {};
        }

        constexpr bvec(bvec&& other, const Allocator& alloc) noexcept : m_data{}, m_wordAllocator{alloc} {
            if (m_wordAllocator == other.m_wordAllocator) {
                m_data = other.m_data;
                other.m_data.stack = {};
            } else {
                assign(other.cbegin(), other.cend());
            }
        }

        constexpr bvec(std::initializer_list<bool> init, const Allocator& alloc = Allocator()) noexcept : m_data{}, m_wordAllocator{alloc} {
            assign(init.begin(), init.end());
        }

        constexpr ~bvec() noexcept {
            if (m_data.is_heap()) {
                m_wordAllocator.deallocate(m_data.heap.pointer, m_data.heap.capacity);
            }
        }

        constexpr bvec& operator=(const bvec& other) noexcept {
            assign(other.cbegin(), other.cend());
            return *this;
        }

        constexpr bvec& operator=(bvec&& other) noexcept {
            if constexpr (std::allocator_traits<word_allocator>::propagate_on_container_copy_assignment::value) {
                if (m_data.is_heap()) {
                    m_wordAllocator.deallocate(m_data.heap.pointer, m_data.heap.capacity);
                }
                m_wordAllocator = other.m_wordAllocator;
                m_data = other.m_data;
                other.m_data.stack = {};
            } else if (m_wordAllocator == other.m_wordAllocator) {
                if (m_data.is_heap()) {
                    m_wordAllocator.deallocate(m_data.heap.pointer, m_data.heap.capacity);
                }
                m_data = other.m_data;
                other.m_data.stack = {};
            } else {
                assign(other.cbegin(), other.cend());
            }
            return *this;
        }

        constexpr bvec& operator=(std::initializer_list<bool> init) noexcept {
            assign(init.begin(), init.end());
            return *this;
        }

        constexpr void assign(size_type count, bool value) noexcept {
            if (m_data.is_heap()) {
                m_wordAllocator.deallocate(m_data.heap.pointer, m_data.heap.capacity);
            }
            m_data.stack = {};
            resize(count, value);
        }

        template <class InputIt>
        constexpr void assign(InputIt first, InputIt last) noexcept {
            if constexpr (std::same_as<InputIt, bvec::iterator> || std::same_as<InputIt, bvec::const_iterator>) {
                const auto& other = first.m_owner;

                if (m_data.is_heap()) {
                    m_wordAllocator.deallocate(m_data.heap.pointer, m_data.heap.capacity);
                }

                if (other.m_data.is_heap()) {
                    const auto begin = static_cast<size_type>(first.m_pos);
                    const auto end = static_cast<size_type>(last.m_pos);
                    const auto beginWord = begin / block_digits;
                    const auto beginBit = begin % block_digits;
                    const auto endWord = block_round(end);
                    const auto endBit = end % block_digits;
                    const auto words = endWord - beginWord;

                    // Copy entire words
                    m_data.heap = {
                            .is_heap = true,
                            .size = ((words * block_digits) - endBit) & heap_size_mask,
                            .capacity = words & heap_capacity_mask,
                            .pointer = m_wordAllocator.allocate(words)
                    };
                    __builtin_memcpy(m_data.heap.pointer, &other.m_data.heap.pointer[beginWord], words * sizeof(block_type));

                    erase(this->begin(), this->begin() + static_cast<int>(beginBit)); // Erase leading bits
                } else {
                    m_data.stack = {
                            .is_heap = false,
                            .size = static_cast<size_type>(last.m_pos - first.m_pos) & stack_size_mask,
                            .data = other.m_data.stack.data >> first.m_pos
                    };
                }
            } else {
                clear();
                while (first != last) {
                    push_back(*first++);
                }
            }
        }

        constexpr void assign(std::initializer_list<bool> init) noexcept {
            assign(init.begin(), init.end());
        }

        [[nodiscard]]
        constexpr allocator_type get_allocator() const noexcept {
            return m_wordAllocator;
        }
    private:
        using word_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<block_type>;

#if __SIZEOF_POINTER__ == 8
        struct stack_type {
            bool is_heap : 1;
            size_type size : 7;
            __uint128_t data : (128 - 8);
        };
        using stack_data_type = __uint128_t;
        static constexpr auto stack_capacity = static_cast<stack_data_type>(128 - 8);
        static constexpr auto stack_size_mask = (static_cast<size_type>(1) << 7) - 1;
        static constexpr auto stack_data_mask = (static_cast<stack_data_type>(1) << stack_capacity) - 1;

        struct heap_type {
            bool is_heap : 1;
            size_type size : 35;
            size_type capacity : 28; // In block
            block_type* pointer;
        };
        static constexpr auto heap_size_mask = (static_cast<size_type>(1) << 35) - 1;
        static constexpr auto heap_capacity_max = static_cast<size_type>(1) << 28;
        static constexpr auto heap_capacity_mask = heap_capacity_max - 1;
#else
        struct stack_type {
            bool is_heap : 1;
            size_type size : 6;
            std::uint64_t data : (64 - 7);
        };
        using stack_data_type = std::uint64_t;
        static constexpr auto stack_capacity = static_cast<stack_data_type>(64 - 7);
        static constexpr auto stack_size_mask = (static_cast<size_type>(1) << 6) - 1;
        static constexpr auto stack_data_mask = (static_cast<stack_data_type>(1) << stack_capacity) - 1;

        struct heap_type {
            bool is_heap : 1;
            size_type size : 18;
            size_type capacity : 13; // In block
            block_type* pointer;
        };
        static constexpr auto heap_size_mask = (static_cast<size_type>(1) << 18) - 1;
        static constexpr auto heap_capacity_max = static_cast<size_type>(1) << 13;
        static constexpr auto heap_capacity_mask = heap_capacity_max - 1;
#endif

        union {
            stack_type stack;
            heap_type heap;

            [[nodiscard]]
            constexpr auto is_heap() const noexcept {
                return stack.is_heap;
            }

            [[nodiscard]]
            constexpr auto size() const noexcept {
                return stack.is_heap ? heap.size : stack.size;
            }
        } m_data;

        word_allocator m_wordAllocator;
    public:
        [[nodiscard]]
        constexpr auto capacity() const noexcept -> size_type {
            return m_data.is_heap() ? m_data.heap.capacity * block_digits : stack_capacity;
        }

        [[nodiscard]]
        constexpr auto size() const noexcept -> size_type {
            return m_data.is_heap() ? m_data.heap.size : m_data.stack.size;
        }

        [[nodiscard]]
        constexpr auto max_size() const noexcept -> size_type {
            return heap_capacity_max * block_digits;
        }

        [[nodiscard]]
        constexpr auto empty() const noexcept {
            return size() == 0;
        }

        constexpr void clear() noexcept {
            if (m_data.is_heap()) {
                m_data.heap.size = 0;
            } else {
                m_data.stack.size = 0;
            }
        }

        constexpr void reserve(size_type newCapacity) noexcept {
            if (newCapacity <= capacity()) {
                return;
            }

            if (m_data.is_heap()) {
                reserve_heap(newCapacity);
            } else {
                move_to_heap(newCapacity);
            }
        }

        constexpr void resize(size_type count, bool value = false) noexcept {
            if (m_data.is_heap()) {
                resize_heap(count, value);
            } else if (count > stack_capacity) {
                grow_to_heap(count, value);
            } else {
                resize_stack(count, value);
            }
        }

        constexpr void swap(bvec& other) noexcept {
            if constexpr (std::allocator_traits<word_allocator>::propagate_on_container_swap::value) {
                std::swap(m_wordAllocator, other.m_wordAllocator);
                std::swap(m_data, other.m_data);
            } else if (m_wordAllocator == other.m_wordAllocator) {
                std::swap(m_data, other.m_data);
            } else {
                const auto heapQualify = m_data.is_heap() | (other.m_data.is_heap() << 1);

                if (heapQualify == 0x0) {
                    // Both on stack
                    std::swap(m_data, other.m_data);
                    return;
                }

                if (heapQualify == 0x1) {
                    // this on heap, other on stack
                    if (m_data.heap.size <= stack_capacity) {
                        const auto otherData = other.m_data.stack.data;
                        std::uint64_t thisData;
                        __builtin_memcpy(&thisData, m_data.heap.pointer, sizeof(thisData));

                        other.m_data.stack.data = thisData & stack_data_mask;
                        __builtin_memcpy(m_data.heap.pointer, &otherData, sizeof(otherData));

                        const auto thisSize = m_data.heap.size;
                        m_data.heap.size = other.m_data.stack.size;
                        other.m_data.stack.size = thisSize & stack_size_mask;
                        return;
                    }

                    other.reserve(m_data.heap.size); // other now on heap
                } else if (heapQualify == 0x2) {
                    // this on stack, other on heap
                    if (other.m_data.heap.size <= stack_capacity) {
                        const auto thisData = m_data.stack.data;
                        std::uint64_t otherData;
                        __builtin_memcpy(&otherData, other.m_data.heap.pointer, sizeof(otherData));

                        m_data.stack.data = otherData & stack_data_mask;
                        __builtin_memcpy(other.m_data.heap.pointer, &thisData, sizeof(thisData));

                        const auto otherSize = other.m_data.heap.size;
                        other.m_data.heap.size = m_data.stack.size;
                        m_data.stack.size = otherSize & stack_size_mask;
                        return;
                    }

                    reserve(other.m_data.heap.size); // this now on heap
                }

                // Both on heap
                const auto thisWord = block_round(size());
                const auto otherWord = block_round(other.size());
                const auto commonWords = std::min(thisWord, otherWord);

                for (auto ii = 0u; ii < commonWords; ++ii) {
                    std::swap(m_data.heap.pointer[ii], other.m_data.heap.pointer[ii]);
                }

                if (thisWord > otherWord) {
                    other.reserve_heap(m_data.heap.size);
                    __builtin_memcpy(&other.m_data.heap.pointer[otherWord], &m_data.heap.pointer[otherWord], (thisWord - otherWord) * sizeof(block_type));
                } else if (thisWord < otherWord) {
                    reserve_heap(other.m_data.heap.size);
                    __builtin_memcpy(&m_data.heap.pointer[thisWord], &other.m_data.heap.pointer[thisWord], (otherWord - thisWord) * sizeof(block_type));
                }

                const auto thisSize = m_data.heap.size;
                m_data.heap.size = other.m_data.heap.size;
                other.m_data.heap.size = thisSize & heap_size_mask;
            }
        }

        constexpr void push_back(bool value) noexcept {
            resize(size() + 1, value);
        }

        constexpr void pop_back() noexcept {
            if (m_data.is_heap()) {
                --m_data.heap.size;
            } else {
                --m_data.stack.size;
            }
        }

        constexpr auto emplace_back(bool value) noexcept {
            const auto endPos = size();
            auto ref = at(endPos);
            resize(endPos + 1, value);
            return ref;
        }

        constexpr void flip() noexcept {
            if (m_data.is_heap()) {
                const auto words = block_round(m_data.heap.size);
                for (auto ii = 0u; ii < words; ++ii) {
                    const auto prev = m_data.heap.pointer[ii];
                    m_data.heap.pointer[ii] = ~prev;
                }
            } else {
                m_data.stack.data = ~m_data.stack.data;
            }
        }
    private:
        constexpr void reserve_heap(size_type newCapacity) noexcept {
            const auto words = block_round(newCapacity);
            if (words == m_data.heap.capacity) {
                return;
            }

            // Reallocate
            auto *pointer = m_wordAllocator.allocate(words);
            __builtin_memcpy(pointer, m_data.heap.pointer, m_data.heap.capacity * sizeof(*pointer));
            m_wordAllocator.deallocate(m_data.heap.pointer, m_data.heap.capacity);

            m_data.heap.capacity = words & heap_capacity_mask;
            m_data.heap.pointer = pointer;
        }

        constexpr void move_to_heap(size_type newCapacity) noexcept {
            const auto words = block_round(newCapacity);

            // Move to heap
            auto *pointer = m_wordAllocator.allocate(words);

            pointer[0] = m_data.stack.data & block_mask;
            pointer[1] = (m_data.stack.data >> block_digits) & block_mask;

            m_data.heap = {
                    .is_heap = true,
                    .size = m_data.stack.size,
                    .capacity = words & heap_capacity_mask,
                    .pointer = pointer
            };
        }

        constexpr void resize_heap(size_type count, bool value) noexcept {
            const auto words = block_round(count);
            if (words > m_data.heap.capacity) {
                // Reallocate
                auto *pointer = m_wordAllocator.allocate(words);
                __builtin_memcpy(pointer, m_data.heap.pointer, m_data.heap.capacity * sizeof(*pointer));
                m_wordAllocator.deallocate(m_data.heap.pointer, m_data.heap.capacity);

                // Align count into next word (filling current)
                auto tailWord = m_data.heap.size / block_digits;
                const auto tailSize = m_data.heap.size % block_digits;

                m_data.heap.size = count & heap_size_mask;
                m_data.heap.capacity = words & heap_capacity_mask;
                m_data.heap.pointer = pointer;

                if (tailSize) {
                    if (value) {
                        m_data.heap.pointer[tailWord] |= (block_mask << tailSize);
                    } else {
                        m_data.heap.pointer[tailWord] &= ~(block_mask << tailSize);
                    }
                    ++tailWord;
                }

                // Fill words
                const auto fill = value ? block_mask : 0;
                for (auto ii = tailWord; ii < words; ++ii) {
                    m_data.heap.pointer[ii] = fill;
                }
            } else {
                if (count > m_data.heap.size) {
                    // Grow within the tail word
                    const auto tailWord = m_data.heap.size / block_digits;
                    const auto tailSize = m_data.heap.size % block_digits;

                    if (value) {
                        m_data.heap.pointer[tailWord] |= (block_mask << tailSize);
                    } else {
                        m_data.heap.pointer[tailWord] &= ~(block_mask << tailSize);
                    }
                }

                m_data.heap.size = count & heap_size_mask;
            }
        }

        constexpr void grow_to_heap(size_type count, bool value) noexcept {
            const auto words = block_round(count);

            // Align count into next word (filling current)
            auto tailWord = m_data.stack.size / block_digits;
            const auto tailSize = m_data.stack.size % block_digits;

            // Move to heap
            auto *pointer = m_wordAllocator.allocate(words);

            if (tailWord) {
                pointer[0] = m_data.stack.data & block_mask;
            }

            if (tailSize) {
                if (value) {
                    pointer[tailWord] = (block_mask << tailSize) | ((m_data.stack.data >> block_digits) & block_mask);
                } else {
                    pointer[tailWord] = (m_data.stack.data >> block_digits) & block_mask;
                }
                ++tailWord;
            }

            // Fill words
            const auto fill = value ? block_mask : 0;
            for (auto ii = tailWord; ii < words; ++ii) {
                pointer[ii] = fill;
            }

            m_data.heap = {
                    .is_heap = true,
                    .size = count & heap_size_mask,
                    .capacity = words & heap_capacity_mask,
                    .pointer = pointer
            };
        }

        constexpr void resize_stack(size_type count, bool value) noexcept {
            if (count > m_data.stack.size) {
                auto data = m_data.stack.data;
                if (value) {
                    data |= stack_data_mask << m_data.stack.size;
                } else {
                    data &= ~(stack_data_mask << m_data.stack.size);
                }
                m_data.stack.data = data & stack_data_mask;
            }
            m_data.stack.size = count & stack_size_mask;
        }

        constexpr void set_at(size_type pos, bool value) noexcept {
            if (m_data.is_heap()) {
                const auto word = pos / block_digits;
                const auto offset = pos % block_digits;

                if (value) {
                    m_data.heap.pointer[word] |= static_cast<block_type>(1) << offset;
                } else {
                    m_data.heap.pointer[word] &= ~(static_cast<block_type>(1) << offset);
                }
            } else {
                if (value) {
                    m_data.stack.data |= (static_cast<stack_data_type>(1) << pos);
                } else {
                    m_data.stack.data &= ~(static_cast<stack_data_type>(1) << pos);
                }
            }
        }

        [[nodiscard]]
        constexpr bool get_at(size_type pos) const noexcept {
            if (m_data.is_heap()) {
                const auto word = pos / block_digits;
                const auto offset = pos % block_digits;

                return m_data.heap.pointer[word] & (static_cast<block_type>(1) << offset);
            } else {
                return m_data.stack.data & (static_cast<stack_data_type>(1) << pos);
            }
        }

        constexpr void flip_at(size_type pos) noexcept {
            if (m_data.is_heap()) {
                const auto word = pos / block_digits;
                const auto offset = pos % block_digits;

                const auto prev = m_data.heap.pointer[word];
                m_data.heap.pointer[word] = prev ^ (static_cast<block_type>(1) << offset);
            } else {
                const auto prev = m_data.stack.data;
                m_data.stack.data = prev ^ (static_cast<stack_data_type>(1) << pos);
            }
        }

        template <class Constness>
        struct reference_type {
            constexpr reference_type& operator=(bool value) noexcept requires (!std::is_const_v<Constness>) {
                m_owner.set_at(m_pos, value);
                return *this;
            }

            constexpr operator bool() const noexcept {
                return m_owner.get_at(m_pos);
            }

            constexpr void flip() noexcept requires (!std::is_const_v<Constness>) {
                m_owner.flip_at(m_pos);
            }
        private:
            friend bvec;
            constexpr reference_type(Constness& owner, size_type pos) noexcept : m_owner{owner}, m_pos{pos} {}

            Constness& m_owner;
            const size_type m_pos;
        };
    public:
        using reference = reference_type<bvec>;
        using const_reference = reference_type<const bvec>;

        constexpr reference at(size_type pos) noexcept {
            return {*this, pos};
        }

        [[nodiscard]]
        constexpr const_reference at(size_type pos) const noexcept {
            return {*this, pos};
        }

        constexpr reference operator[](size_type pos) noexcept {
            return {*this, pos};
        }

        [[nodiscard]]
        constexpr const_reference operator[](size_type pos) const noexcept {
            return {*this, pos};
        }

        constexpr reference front() noexcept {
            return {*this, 0};
        }

        [[nodiscard]]
        constexpr const_reference front() const noexcept {
            return {*this, 0};
        }

        constexpr reference back() noexcept {
            return {*this, size() - 1};
        }

        [[nodiscard]]
        constexpr const_reference back() const noexcept {
            return {*this, size() - 1};
        }

        static constexpr void swap(reference x, reference y) noexcept {
            if (x.m_owner.get_at(x.m_pos) == y.m_owner.get_at(y.m_pos)) {
                return;
            }

            x.m_owner.flip_at(x.m_pos);
            y.m_owner.flip_at(y.m_pos);
        }
    private:
        friend struct bvec_cast_helper;

        template <class Constness, int Direction>
        struct iterator_type {
            using difference_type = std::make_signed_t<size_type>;
            using value_type = bool;

            constexpr iterator_type(const iterator_type<std::remove_const_t<Constness>, Direction>& other) noexcept requires std::is_const_v<Constness> : m_owner{other.m_owner}, m_pos{other.m_pos} {}
            constexpr iterator_type(const iterator_type& other) noexcept : m_owner{other.m_owner}, m_pos{other.m_pos} {}

            constexpr iterator_type operator++(int) noexcept {
                auto prev = *this;
                m_pos += Direction;
                return prev;
            }

            constexpr iterator_type& operator++() noexcept {
                m_pos += Direction;
                return *this;
            }

            constexpr iterator_type operator--(int) noexcept {
                auto prev = *this;
                m_pos -= Direction;
                return prev;
            }

            constexpr iterator_type& operator--() noexcept {
                m_pos -= Direction;
                return *this;
            }

            constexpr iterator_type& operator+=(difference_type n) noexcept {
                m_pos += n;
                return *this;
            }

            constexpr iterator_type& operator-=(difference_type n) noexcept {
                m_pos -= n;
                return *this;
            }

            constexpr iterator_type operator+(difference_type n) const noexcept {
                return iterator_type{m_owner, m_pos + n};
            }

            constexpr iterator_type operator-(difference_type n) const noexcept {
                return iterator_type{m_owner, m_pos - n};
            }

            constexpr bool operator!=(const iterator_type& rhs) const noexcept {
                return &m_owner != &rhs.m_owner || m_pos != rhs.m_pos;
            }

            constexpr bool operator==(const iterator_type& rhs) const noexcept {
                return &m_owner == &rhs.m_owner && m_pos == rhs.m_pos;
            }

            constexpr bool operator<=(const iterator_type& rhs) const noexcept {
                return &m_owner == &rhs.m_owner && m_pos <= rhs.m_pos;
            }

            constexpr bool operator>=(const iterator_type& rhs) const noexcept {
                return &m_owner == &rhs.m_owner && m_pos >= rhs.m_pos;
            }

            constexpr bool operator<(const iterator_type& rhs) const noexcept {
                return &m_owner == &rhs.m_owner && m_pos < rhs.m_pos;
            }

            constexpr bool operator>(const iterator_type& rhs) const noexcept {
                return &m_owner == &rhs.m_owner && m_pos > rhs.m_pos;
            }

            constexpr reference_type<Constness> operator*() const noexcept {
                return {m_owner, static_cast<size_type>(m_pos)};
            }

            [[nodiscard]]
            constexpr iterator_type<Constness, 1> base() const noexcept requires (Direction < 0) {
                return {m_owner, m_pos};
            }
        private:
            friend bvec;
            constexpr iterator_type(Constness& owner, difference_type pos) noexcept : m_owner{owner}, m_pos{pos} {}

            Constness& m_owner;
            difference_type m_pos;
        };
    public:
        using iterator = iterator_type<bvec, 1>;
        using const_iterator = iterator_type<const bvec, 1>;
        using reverse_iterator = iterator_type<bvec, -1>;
        using const_reverse_iterator = iterator_type<const bvec, -1>;

        constexpr iterator begin() noexcept {
            return {*this, 0};
        }

        [[nodiscard]]
        constexpr const_iterator cbegin() const noexcept {
            return {*this, 0};
        }

        [[nodiscard]]
        constexpr auto begin() const noexcept {
            return cbegin();
        }

        constexpr iterator end() noexcept {
            return {*this, static_cast<int>(size())};
        }

        [[nodiscard]]
        constexpr const_iterator cend() const noexcept {
            return {*this, static_cast<int>(size())};
        }

        [[nodiscard]]
        constexpr auto end() const noexcept {
            return cend();
        }

        constexpr reverse_iterator rbegin() noexcept {
            return {*this, static_cast<int>(size()) - 1};
        }

        [[nodiscard]]
        constexpr const_reverse_iterator crbegin() const noexcept {
            return {*this, static_cast<int>(size()) - 1};
        }

        [[nodiscard]]
        constexpr auto rbegin() const noexcept {
            return crbegin();
        }

        constexpr reverse_iterator rend() noexcept {
            return {*this, -1};
        }

        [[nodiscard]]
        constexpr const_reverse_iterator crend() const noexcept {
            return {*this, -1};
        }

        [[nodiscard]]
        constexpr auto rend() const noexcept {
            return crend();
        }

        constexpr iterator erase(const_iterator pos) noexcept {
            constexpr auto carry_bit = (static_cast<block_type>(1) << (block_digits - 1));

            if (m_data.is_heap()) {
                const auto posWord = static_cast<size_type>(pos.m_pos) / block_digits;
                const auto posWordSize = static_cast<size_type>(pos.m_pos) % block_digits;

                bool carry = false;
                auto lastWord = m_data.heap.size / block_digits;
                while (lastWord > posWord) {
                    const auto carryBit = carry ? carry_bit : 0;
                    carry = m_data.heap.pointer[lastWord] & 1;
                    m_data.heap.pointer[lastWord] >>= 1;
                    m_data.heap.pointer[lastWord] |= carryBit;
                    --lastWord;
                }

                const auto upper = m_data.heap.pointer[posWord] >> (posWordSize + 1);
                const auto lower = m_data.heap.pointer[posWord] & ((static_cast<block_type>(1) << posWordSize) - 1);
                m_data.heap.pointer[posWord] = (carry ? carry_bit : 0) | (upper << posWordSize) | lower;
            } else {
                const auto upper = m_data.stack.data >> (pos.m_pos + 1);
                const auto lower = m_data.stack.data & ((static_cast<stack_data_type>(1) << pos.m_pos) - 1);
                m_data.stack.data = ((upper << pos.m_pos) | lower) & stack_data_mask;
                --m_data.stack.size;
            }
            return iterator{*this, pos.m_pos};
        }

        constexpr iterator erase(const_iterator first, const_iterator last) noexcept {
            if (m_data.is_heap()) {
                if (last.m_pos > m_data.heap.size) {
                    m_data.heap.size = static_cast<size_type>(first.m_pos) & heap_size_mask;
                } else {
                    auto firstBlock = static_cast<size_type>(first.m_pos) / block_digits;
                    auto firstBit = static_cast<size_type>(first.m_pos) % block_digits;

                    auto lastBlock = static_cast<size_type>(last.m_pos) / block_digits;
                    auto lastBit = static_cast<size_type>(last.m_pos) % block_digits;

                    // Handle first block
                    if (firstBlock == lastBlock) {
                        const auto upperMask = block_mask << lastBit;
                        const auto lowerMask = (static_cast<block_type>(1) << firstBit) - 1;

                        const auto endBlock = block_round(m_data.heap.size);
                        auto& data = m_data.heap.pointer;

                        data[firstBlock] = (data[firstBlock] & lowerMask) |
                                             ((data[firstBlock] & upperMask) >> (lastBit - firstBit));

                        ++lastBlock;
                        if (lastBlock < endBlock) {
                            data[firstBlock] |= data[lastBlock] << (block_digits - lastBit - firstBit);
                        }
                        ++firstBlock;

                        lastBit = (lastBit + block_digits - firstBit) % block_digits;
                        firstBit = 0;
                    } else if (firstBit) {
                        const auto lowerMask = (static_cast<block_type>(1) << firstBit) - 1;

                        const auto endBlock = block_round(m_data.heap.size);
                        auto& data = m_data.heap.pointer;

                        data[firstBlock] = (data[firstBlock] & lowerMask) |
                                             ((data[lastBlock] >> lastBit) << firstBit);

                        ++lastBlock;
                        if (firstBit < lastBit && lastBlock < endBlock) {
                            data[firstBlock] |= data[lastBlock] << (firstBit + block_digits - lastBit);
                        }
                        ++firstBlock;

                        lastBit = (lastBit + block_digits - firstBit) % block_digits;
                        firstBit = 0;
                    }

                    const auto firstByteAlign = firstBit % 8;
                    const auto lastByteAlign = lastBit % 8;

                    // Handle remaining blocks
                    if (firstByteAlign == 0 && lastByteAlign == 0) {
                        const auto firstByte = (firstBlock * sizeof(block_type)) + (firstBit / 8);
                        const auto lastByte = (lastBlock * sizeof(block_type)) + (lastBit / 8);

                        const auto endByte = (m_data.heap.size + 7) / 8;
                        const auto numBytes = endByte - lastByte;

                        auto* data8 = reinterpret_cast<std::uint8_t*>(m_data.heap.pointer);
                        __builtin_memcpy(&data8[firstByte], &data8[lastByte], numBytes);
                    } else if (lastBlock > firstBlock) {
                        const auto blockSpan = lastBlock - firstBlock;

                        auto& data = m_data.heap.pointer;

                        for (auto ii = 0u; ii < blockSpan; ++ii) {
                            data[firstBlock + ii] = (data[lastBlock + ii] >> lastBit) |
                                                      (data[lastBlock + ii + 1] << (block_digits - lastBit));
                        }

                        // Shift last block
                        data[firstBlock + blockSpan] = data[lastBlock + blockSpan] >> lastBit;
                    }

                    m_data.heap.size = (m_data.heap.size - static_cast<size_type>(last.m_pos - first.m_pos)) & heap_size_mask;
                }
            } else {
                if (last.m_pos > m_data.stack.size) {
                    m_data.stack.size = static_cast<size_type>(first.m_pos) & stack_size_mask;
                } else {
                    const auto start = static_cast<size_type>(first.m_pos);
                    const auto end = static_cast<size_type>(last.m_pos);

                    const auto lower = m_data.stack.data & ((static_cast<stack_data_type>(1) << start) - 1);
                    const auto upper = m_data.stack.data >> end;

                    m_data.stack.data = ((upper << start) | lower) & stack_data_mask;
                    m_data.stack.size = (m_data.stack.size - (end - start)) & stack_size_mask;
                }
            }

            return iterator{*this, first.m_pos};
        }
    };

    template <class Alloc>
    constexpr bool operator==(const bvec<Alloc>& lhs, const bvec<Alloc>& rhs) noexcept {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        for (auto ii = 0u; ii < lhs.size(); ++ii) {
            if (lhs[ii] != rhs[ii]) {
                return false;
            }
        }

        return true;
    }

    template <class Alloc>
    constexpr auto operator<=>(const bvec<Alloc>& lhs, const bvec<Alloc>& rhs) noexcept {
        auto lhsSize = lhs.size();
        auto rhsSize = rhs.size();

        while (lhsSize > rhsSize) {
            if (lhs[--lhsSize]) {
                return std::weak_ordering::greater;
            }
        }

        while (rhsSize > lhsSize) {
            if (rhs[--rhsSize]) {
                return std::weak_ordering::less;
            }
        }

        // Size is same, compare MSB
        while (lhsSize--) {
            if (lhs[lhsSize] > rhs[lhsSize]) {
                return std::weak_ordering::greater;
            }
            if (lhs[lhsSize] < rhs[lhsSize]) {
                return std::weak_ordering::less;
            }
        }

        return std::weak_ordering::equivalent;
    }

    struct bvec_cast_helper {
        template <typename BVec>
        static constexpr auto block_digits = BVec::block_digits;

        template <std::integral T, class Alloc>
        static constexpr auto cast(const bvec<Alloc>& c, typename bvec<Alloc>::size_type pos) noexcept -> T {
            if constexpr (std::same_as<T, bool>) {
                return c.get_at(pos);
            } else {
                constexpr auto digits = std::numeric_limits<std::make_unsigned_t<T>>::digits;

                const auto lastWord = c.size() / digits;

                auto mask = bvec<Alloc>::block_mask;
                if (pos == lastWord) {
                    const auto bits = c.size() % digits;
                    mask >>= (std::numeric_limits<decltype(mask)>::digits - bits);
                }

                if (c.m_data.is_heap()) {
                    return static_cast<T>(reinterpret_cast<T*>(c.m_data.heap.pointer)[pos] & mask);
                } else {
                    const auto shift = pos * digits;
                    return static_cast<T>((c.m_data.stack.data >> shift) & mask);
                }
            }
        }
    };

    template <std::integral T, class Alloc>
    constexpr auto bvec_cast(const bvec<Alloc>& c, typename bvec<Alloc>::size_type pos = 0) noexcept -> T {
        return bvec_cast_helper::cast<T, Alloc>(c, pos);
    }

    template <typename BVec>
    inline constexpr auto bvec_block_digits = bvec_cast_helper::block_digits<BVec>;

}

namespace std {

    template <class Alloc>
    constexpr void swap(xilefian::bvec<Alloc>& lhs, xilefian::bvec<Alloc>& rhs) noexcept {
        lhs.swap(rhs);
    }

    template <class Alloc>
    constexpr auto erase(xilefian::bvec<Alloc>& c, bool value) noexcept -> xilefian::bvec<Alloc>::size_type {
        constexpr auto digits = std::numeric_limits<std::make_unsigned_t<std::size_t>>::digits;
        const auto lastWord = (c.size() + digits - 1) / digits;

        auto popCount = 0;
        for (auto ii = 0u; ii < lastWord; ++ii) {
            popCount += __builtin_popcount(xilefian::bvec_cast<unsigned int>(c, ii));
        }

        if (value) {
            c.assign(c.size() - popCount, false);
            return popCount;
        }

        const auto removed = c.size() - popCount;
        c.assign(popCount, true);
        return removed;
    }

    template <class Alloc, class Pred>
    constexpr auto erase_if(xilefian::bvec<Alloc>& c, Pred pred) noexcept -> xilefian::bvec<Alloc>::size_type {
        const auto startSize = c.size();

        auto iter = c.begin();
        while (iter != c.end()) {
            if (pred(*iter)) {
                c.erase(iter);
            } else {
                ++iter;
            }
        }

        return startSize - c.size();
    }

    template <class Alloc>
    struct hash<xilefian::bvec<Alloc>> {
        constexpr auto operator()(const xilefian::bvec<Alloc>& key) const noexcept -> std::size_t {
            constexpr auto digits = std::numeric_limits<std::make_unsigned_t<std::size_t>>::digits;
            const auto lastWord = (key.size() + digits - 1) / digits;

            std::size_t result = key.size();
            for (auto ii = 0u; ii < lastWord; ++ii) {
                result = result * xilefian::bvec_block_digits<xilefian::bvec<Alloc>> + xilefian::bvec_cast<std::size_t>(key, ii);
            }
            return result;
	    }
	};

}
