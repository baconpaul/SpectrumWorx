////////////////////////////////////////////////////////////////////////////////
///
/// \file circularListAlgorithms.hpp
/// --------------------------------
///
///   The seven operations the module chain performs on its circular doubly
/// linked list, over a caller-supplied NodeTraits.
///
///   Was boost::intrusive::circular_list_algorithms, the last Boost library in
/// the tree that was not the preprocessor. Boost.Intrusive's *containers* cannot
/// be used here -- they have no shared-ownership semantics and the chain's nodes
/// are reference counted -- so only the algorithms were ever taken, and they are
/// pointer surgery with no dependencies.
///
///   The traits' set_next / set_previous may have side effects: in the module
/// chain they assign an IntrusivePtr and so change a reference count. That is
/// why the update order below matters and is Boost's, comments included -- the
/// two arguments of link_before and link_after may be the same node.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef circularListAlgorithms_hpp__9F31D7C2_46A8_4E15_BD90_38C6E7A21B54
#define circularListAlgorithms_hpp__9F31D7C2_46A8_4E15_BD90_38C6E7A21B54
//------------------------------------------------------------------------------
#include <cstddef>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \struct CircularListAlgorithms
///
/// \brief NodeTraits must provide node_ptr, const_node_ptr, get_next,
/// get_previous, set_next and set_previous.
///
////////////////////////////////////////////////////////////////////////////////

template <class NodeTraits> struct CircularListAlgorithms
{
    using node_ptr = typename NodeTraits::node_ptr;
    using const_node_ptr = typename NodeTraits::const_node_ptr;

    /// \brief Puts a node into the "does not belong to a list" state.
    static void init(node_ptr const node)
    {
        NodeTraits::set_next(node, node_ptr());
        NodeTraits::set_previous(node, node_ptr());
    }

    /// \brief Whether \p node is in the state init() leaves it in.
    static bool inited(const_node_ptr const node) { return !NodeTraits::get_next(node); }

    /// \brief Makes \p node an empty list: a cycle of one, itself.
    static void init_header(node_ptr const node)
    {
        NodeTraits::set_next(node, node);
        NodeTraits::set_previous(node, node);
    }

    /// \brief The number of nodes in the cycle \p node belongs to, itself
    /// included -- so a list's header counts as one.
    static std::size_t count(const_node_ptr const node)
    {
        std::size_t counted{0};
        const_node_ptr current{node};
        do
        {
            current = NodeTraits::get_next(current);
            ++counted;
        } while (current != node);
        return counted;
    }

    /// \brief Removes \p node from its list and returns the node that followed
    /// it. \p node's own links are left pointing into the list.
    static node_ptr unlink(node_ptr const node)
    {
        node_ptr const next(NodeTraits::get_next(node));
        node_ptr const previous(NodeTraits::get_previous(node));
        NodeTraits::set_next(previous, next);
        NodeTraits::set_previous(next, previous);
        return next;
    }

    /// \brief Inserts \p node immediately before \p next.
    static void link_before(node_ptr const next, node_ptr const node)
    {
        node_ptr const previous(NodeTraits::get_previous(next));
        NodeTraits::set_previous(node, previous);
        NodeTraits::set_next(node, next);
        // next may alias previous, so update it only once both of node's links
        // have been written.
        NodeTraits::set_next(previous, node);
        NodeTraits::set_previous(next, node);
    }

    /// \brief Inserts \p node immediately after \p previous.
    static void link_after(node_ptr const previous, node_ptr const node)
    {
        node_ptr const next(NodeTraits::get_next(previous));
        NodeTraits::set_previous(node, previous);
        NodeTraits::set_next(node, next);
        // previous may alias next; see link_before().
        NodeTraits::set_previous(next, node);
        NodeTraits::set_next(previous, node);
    }

    /// \brief Moves the half-open range [begin, end) to just before \p position.
    static void transfer(node_ptr const position, node_ptr const begin, node_ptr const end)
    {
        if (begin == end)
            return;

        node_ptr const beforePosition(NodeTraits::get_previous(position));
        node_ptr const beforeBegin(NodeTraits::get_previous(begin));
        node_ptr const beforeEnd(NodeTraits::get_previous(end));

        NodeTraits::set_next(beforeEnd, position);
        NodeTraits::set_previous(position, beforeEnd);
        NodeTraits::set_next(beforeBegin, end);
        NodeTraits::set_previous(end, beforeBegin);
        NodeTraits::set_next(beforePosition, begin);
        NodeTraits::set_previous(begin, beforePosition);
    }
}; // struct CircularListAlgorithms

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // circularListAlgorithms_hpp
