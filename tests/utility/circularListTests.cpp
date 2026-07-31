////////////////////////////////////////////////////////////////////////////////
///
/// \file circularListTests.cpp
/// ---------------------------
///
///   CircularListAlgorithms, which replaced boost::intrusive's. The module
/// chain is the only caller and its failure mode is not a crash: a link written
/// in the wrong order leaves a chain that still walks, still counts, and has two
/// modules in the wrong places -- or a reference count that never reaches zero,
/// because the traits' setters assign an IntrusivePtr.
///
///   So the operations are checked here against plain int nodes, where the
/// whole list is observable.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "le/utility/circularListAlgorithms.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

struct Node
{
    int value;
    Node *next{nullptr};
    Node *previous{nullptr};
};

struct NodeTraits
{
    using node_ptr = Node *;
    using const_node_ptr = Node const *;

    static node_ptr get_next(const_node_ptr const n) { return n->next; }
    static node_ptr get_previous(const_node_ptr const n) { return n->previous; }
    static void set_next(node_ptr const n, node_ptr const next) { n->next = next; }
    static void set_previous(node_ptr const n, node_ptr const previous) { n->previous = previous; }
};

using List = LE::Utility::CircularListAlgorithms<NodeTraits>;

/// The cycle starting after \p header, up to but not including it.
std::vector<int> forwards(Node const &header)
{
    std::vector<int> values;
    for (Node const *p(header.next); p != &header; p = p->next)
        values.push_back(p->value);
    return values;
}

/// The same walked backwards, which is what catches a one-sided link.
std::vector<int> backwards(Node const &header)
{
    std::vector<int> values;
    for (Node const *p(header.previous); p != &header; p = p->previous)
        values.push_back(p->value);
    return values;
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("An initialised node is not in a list", "[circular-list]")
{
    Node node{0};
    List::init(&node);
    CHECK(List::inited(&node));

    List::init_header(&node);
    CHECK_FALSE(List::inited(&node));
    CHECK(node.next == &node);
    CHECK(node.previous == &node);
    CHECK(List::count(&node) == 1); // the header itself
}

TEST_CASE("link_before appends and link_after prepends", "[circular-list]")
{
    Node header{-1};
    List::init_header(&header);

    Node one{1}, two{2}, three{3};
    // Before the header is the end of the list.
    List::link_before(&header, &one);
    List::link_before(&header, &two);
    List::link_before(&header, &three);

    CHECK(forwards(header) == std::vector<int>{1, 2, 3});
    CHECK(backwards(header) == std::vector<int>{3, 2, 1});
    CHECK(List::count(&header) == 4);

    Node zero{0};
    List::link_after(&header, &zero); // after the header is the front
    CHECK(forwards(header) == std::vector<int>{0, 1, 2, 3});
    CHECK(backwards(header) == std::vector<int>{3, 2, 1, 0});
}

TEST_CASE("link_before and link_after work on a one-node list", "[circular-list]")
{
    // The aliasing case the update order in both functions exists for: with a
    // freshly initialised header, the node before it and the node after it are
    // both the header.
    Node header{-1};
    List::init_header(&header);

    Node only{1};
    List::link_before(&header, &only);
    CHECK(header.next == &only);
    CHECK(header.previous == &only);
    CHECK(only.next == &header);
    CHECK(only.previous == &header);

    Node other{-2};
    List::init_header(&other);
    Node onlyAfter{2};
    List::link_after(&other, &onlyAfter);
    CHECK(other.next == &onlyAfter);
    CHECK(other.previous == &onlyAfter);
    CHECK(onlyAfter.next == &other);
    CHECK(onlyAfter.previous == &other);
}

TEST_CASE("unlink removes a node and returns its successor", "[circular-list]")
{
    Node header{-1};
    List::init_header(&header);

    Node one{1}, two{2}, three{3};
    List::link_before(&header, &one);
    List::link_before(&header, &two);
    List::link_before(&header, &three);

    CHECK(List::unlink(&two) == &three);
    CHECK(forwards(header) == std::vector<int>{1, 3});
    CHECK(backwards(header) == std::vector<int>{3, 1});
    CHECK(List::count(&header) == 3);

    // The chain unlinks the whole list this way -- ModuleChainImpl::clear()
    // walks with the returned pointer.
    CHECK(List::unlink(&one) == &three);
    CHECK(List::unlink(&three) == &header);
    CHECK(forwards(header).empty());
    CHECK(List::count(&header) == 1);
}

TEST_CASE("transfer splices a range in front of a position", "[circular-list]")
{
    Node source{-2};
    List::init_header(&source);
    Node one{1}, two{2}, three{3};
    List::link_before(&source, &one);
    List::link_before(&source, &two);
    List::link_before(&source, &three);

    Node destination{-1};
    List::init_header(&destination);
    Node nine{9};
    List::link_before(&destination, &nine);

    // Everything in source, in front of the destination's header -- i.e.
    // appended -- which is what ModuleChainImpl's move constructor does.
    List::transfer(&destination, source.next, &source);

    CHECK(forwards(destination) == std::vector<int>{9, 1, 2, 3});
    CHECK(backwards(destination) == std::vector<int>{3, 2, 1, 9});
    CHECK(forwards(source).empty());
    CHECK(List::count(&source) == 1);
}

TEST_CASE("transfer of an empty range does nothing", "[circular-list]")
{
    Node header{-1};
    List::init_header(&header);
    Node one{1};
    List::link_before(&header, &one);

    List::transfer(&header, &one, &one);

    CHECK(forwards(header) == std::vector<int>{1});
    CHECK(backwards(header) == std::vector<int>{1});
}
