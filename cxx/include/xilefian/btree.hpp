/*
===============================================================================

 Copyright (C) 2023 Felix Jones
 For conditions of distribution and use, see copyright notice in LICENSE

===============================================================================
*/

#pragma once

#include <iterator>
#include <memory>

#include "bvec.hpp"

namespace xilefian {

    template <typename T, class Compare = std::less<T>, class Allocator = std::allocator<T>>
    class btree {
    public:
        using value_type = T;
    private:
        using value_allocator_traits = std::allocator_traits<Allocator>;
        using bool_allocator = value_allocator_traits::template rebind_alloc<bool>;
        using bvec_type = bvec<bool_allocator>;

        struct bnode {
            bnode* parent;
            bnode* positive;
            bnode* negative;
            value_type& value;

            template <bool Forward>
            [[nodiscard]]
            constexpr auto*& next() noexcept {
                if constexpr (Forward) {
                    return negative;
                } else {
                    return positive;
                }
            }

            template <bool Forward>
            [[nodiscard]]
            constexpr auto*& prev() noexcept {
                if constexpr (Forward) {
                    return positive;
                } else {
                    return negative;
                }
            }

            [[nodiscard]]
            constexpr auto*& child(bool which) noexcept {
                return which ? positive : negative;
            }
        };

        constexpr void recursive_destroy(bnode* node) noexcept {
            if (node->positive) {
                recursive_destroy(node->positive);
            }
            if (node->negative) {
                recursive_destroy(node->negative);
            }
            auto* valuePtr = &node->value;
            node_allocator_traits::destroy(m_nodeAllocator, node);
            m_nodeAllocator.deallocate(node, 1);
            value_allocator_traits::destroy(m_valueAllocator, valuePtr);
            m_valueAllocator.deallocate(valuePtr, 1);
        }
    public:
        constexpr explicit btree(const Allocator allocator = Allocator()) noexcept : m_valueAllocator{allocator}, m_nodeAllocator{allocator}, m_boolAllocator{allocator} {}

        constexpr ~btree() noexcept {
            if (m_root) {
                recursive_destroy(m_root);
            }
        }

        [[nodiscard]]
        constexpr auto empty() const noexcept {
            return m_root == nullptr;
        }

        class iterator {
        public:
            constexpr auto& operator*() noexcept {
                return m_node->value;
            }

            constexpr auto& operator++() noexcept {
                advance<true>();
                return *this;
            }

            constexpr auto operator++(int) noexcept -> iterator {
                auto copy = *this;
                advance<true>();
                return copy;
            }

            constexpr auto& operator--() noexcept {
                advance<false>();
                return *this;
            }

            constexpr auto operator--(int) noexcept -> iterator {
                auto copy = *this;
                advance<false>();
                return copy;
            }

            constexpr bool operator==(const iterator& rhs) const noexcept {
                if (m_isEnd || rhs.m_isEnd) {
                    return m_isEnd == rhs.m_isEnd;
                } else {
                    return m_node == rhs.m_node;
                }
            }

            constexpr bool operator!=(const iterator& rhs) const noexcept {
                if (m_isEnd || rhs.m_isEnd) {
                    return m_isEnd != rhs.m_isEnd;
                } else {
                    return m_node != rhs.m_node;
                }
            }
        private:
            friend class btree;

            constexpr iterator(bnode* node, bvec_type&& code) noexcept : m_node{node}, m_code{code}, m_isEnd{} {}
            constexpr iterator(bnode* node, bvec_type&& code, std::nullptr_t) noexcept : m_node{node}, m_code{code}, m_isEnd{true} {}

            template <bool Forward>
            constexpr void advance() noexcept {
                if (m_isEnd) {
                    if constexpr (!Forward) {
                        if (m_node->parent) {
                            m_node = m_node->parent;
                        } else {
                            m_isEnd = false;
                        }
                    }
                    return;
                }

                if (m_node->template next<Forward>()) {
                    m_node = m_node->template next<Forward>();
                    m_code.push_back(!Forward);

                    while (m_node->template prev<Forward>()) {
                        m_node = m_node->template prev<Forward>();
                        m_code.push_back(Forward);
                    }
                } else if (m_node->parent) {
                    // Go back up to last positive path
                    auto* next = m_node;
                    auto iter = m_code.rbegin();
                    while (next->parent && *iter != Forward) {
                        next = next->parent;
                        ++iter;
                    }

                    if (next->parent) {
                        m_code.erase(iter.base(), m_code.end());
                        m_node = next->parent;
                    } else {
                        m_isEnd = true;
                    }
                } else {
                    m_isEnd = true;
                }
            }

            bnode* m_node;
            bvec_type m_code;
            bool m_isEnd;
        };

        template <typename... Args>
        constexpr auto emplace(Args&&... args) noexcept -> iterator {
            auto* value = m_valueAllocator.allocate(1);
            value_allocator_traits::construct(m_valueAllocator, value, std::forward<Args>(args)...);

            bvec_type code{m_boolAllocator};
            bnode* parent = nullptr;
            auto** node = &m_root;
            while (*node) {
                parent = *node;
                if (m_comparator(*value, (*node)->value)) {
                    node = &(*node)->positive;
                    code.push_back(true);
                } else {
                    node = &(*node)->negative;
                    code.push_back(false);
                }
            }

            *node = m_nodeAllocator.allocate(1);
            node_allocator_traits::construct(m_nodeAllocator, *node, parent, nullptr, nullptr, *value);
            return iterator{*node, std::move(code)};
        }

        template <typename... Args>
        constexpr auto emplace_hint(iterator hint, Args&&... args) noexcept -> iterator {
            auto* value = m_valueAllocator.allocate(1);
            value_allocator_traits::construct(m_valueAllocator, value, std::forward<Args>(args)...);

            auto* parent = hint.m_node;
            auto code = hint.m_code;
            while (parent->parent && code.back() != m_comparator(*value, parent->parent->value)) {
                code.pop_back();
                parent = parent->parent;
            }

            auto** node = &parent->child(code.emplace_back(m_comparator(*value, parent->value)));
            while (*node) {
                parent = *node;
                if (m_comparator(*value, (*node)->value)) {
                    node = &(*node)->positive;
                    code.push_back(true);
                } else {
                    node = &(*node)->negative;
                    code.push_back(false);
                }
            }

            *node = m_nodeAllocator.allocate(1);
            node_allocator_traits::construct(m_nodeAllocator, *node, parent, nullptr, nullptr, *value);
            return iterator{*node, std::move(code)};
        }

        constexpr auto insert(const value_type& value) noexcept {
            return emplace(value); // Calls copy constructor
        }

        constexpr auto insert(value_type&& value) noexcept {
            return emplace(value); // Calls move constructor
        }

        constexpr auto insert_hint(iterator hint, const value_type& value) noexcept {
            return emplace_hint(hint, value); // Calls copy constructor
        }

        constexpr auto insert_hint(iterator hint, value_type&& value) noexcept {
            return emplace_hint(hint, value); // Calls move constructor
        }

        constexpr auto begin() noexcept -> iterator {
            bvec_type code{m_boolAllocator};
            auto* node = m_root;
            while (node->positive) {
                node = node->positive;
                code.push_back(true);
            }
            return iterator{node, std::move(code)};
        }

        constexpr auto end() noexcept -> iterator {
            bvec_type code{m_boolAllocator};
            auto* node = m_root;
            while (node->negative) {
                node = node->negative;
                code.push_back(false);
            }
            return iterator{node, std::move(code), nullptr};
        }
    private:
        using node_allocator = value_allocator_traits::template rebind_alloc<bnode>;
        using node_allocator_traits = std::allocator_traits<node_allocator>;

        Allocator m_valueAllocator{};
        node_allocator m_nodeAllocator{};
        bool_allocator m_boolAllocator{}; // For iterator codes
        Compare m_comparator;
        bnode* m_root{};
    };

}
