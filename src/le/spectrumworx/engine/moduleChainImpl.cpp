////////////////////////////////////////////////////////////////////////////////
///
/// moduleChainImpl.cpp
/// -------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "moduleChainImpl.hpp"

#include "module.hpp"

#include "le/utility/assert.hpp"

#include <utility>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace Engine
{
//------------------------------------------------------------------------------

LE_OPTIMIZE_FOR_SIZE_BEGIN()

LE_COLD ModuleChainBase::ModuleChainBase()
{
    referenceCount_.verifyCountEqual(0);
    ++referenceCount_;
    referenceCount_.verifyCountEqual(1);
    resetRoot();
    referenceCount_.verifyCountEqual(3);
}

LE_COLD ModuleChainBase::ModuleChainBase(ModuleChainBase &&other)
{
    referenceCount_.verifyCountEqual(0);
    ++referenceCount_;
    referenceCount_.verifyCountEqual(1);
    resetRoot();
    referenceCount_.verifyCountEqual(3);

    moveAssign(std::forward<ModuleChainBase>(other));
}

LE_COLD ModuleChainBase &ModuleChainBase::operator=(ModuleChainBase &&other)
{
    clear();
    moveAssign(std::forward<ModuleChainBase>(other));
    return *this;
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note Because nodes form cyclic references through next and previous
/// pointers, each node has to be explicitly unlinked in order to avoid leaks.
///                                           (12.03.2014.) (Domagoj Saric)
///
/// \note `clear()` is not enough, and was for as long as a chain outlived every
/// module that had ever been in it. `unlink()` writes the *neighbours*' links and
/// deliberately leaves the removed node's own -- so every ex-member still holds a
/// counted pointer at this root. That cost nothing while the chain was the last
/// owner: the modules were destroyed on the spot and released it on the way out.
///
///   Stage 6 made chains that die first. A preset load builds a chain, publishes
/// it, and the one it displaces comes back to be destroyed here -- while the
/// editor's strips still hold counted references to the modules that were in it.
/// The root was then freed with those pointers still aimed at it: this
/// destructor's own assertion is what caught it, and what it caught was a
/// use-after-free waiting for the user to close a strip.
///
///   So the links go too. Nothing walks a chain that is being destroyed, which is
/// the one thing the note on `remove()` says they are kept for.
///
/// \note The 2016 sources met the same thing from the other end and left it: the
/// two assertions in `resetRoot()` are commented out under "a module sent to be
/// destroyed in the GUI thread might still be referencing the root node". That is
/// this, exactly -- a module outliving the chain's account of it -- and it was
/// silenced there rather than fixed. This destructor's copy of the same assertion
/// is the one that survived to catch it.
///                                           (02.08.2026.) (SW port)
///
////////////////////////////////////////////////////////////////////////////////

LE_COLD ModuleChainBase::~ModuleChainBase()
{
    while (next_.get() != this)
    {
        /// \note Held across the unlink, because the unlink drops the chain's
        /// reference and this may be the last one.
        NodePtr const pNode(next_);
        node_algorithms::unlink(pNode.get());
        pNode->next_.reset();
        pNode->previous_.reset();
    }
    LE_ASSERT(empty());
    LE_ASSERT(this->referenceCount_ == 1 || this->referenceCount_ == 3);
}

LE_COLD void ModuleChainBase::moveAssign(ModuleChainBase &&other)
{
    LE_ASSERT(this->empty());

#ifndef NDEBUG
    auto const otherSize(other.size());
#endif // NDEBUG

    Node *const begin(other.next_.get());
    LE_ASSERT(&*begin == other.begin());
    Node *const end(&other);
    LE_ASSERT(&*end == other.end());
    node_algorithms::transfer(this, begin, end);
    //this->previous_ = other.previous_;
    //this->next_     = other.next_    ;
    //other.resetRoot();
    LE_ASSERT(other.referenceCount_ == 3);
    LE_ASSERT(other.empty());
    LE_ASSERT(this->size() == otherSize);
}

LE_COLD std::uint8_t ModuleChainBase::getIndexForModule(Node const &module) const
{
#if 0
    LE_ASSERT_MSG
    (
        std::find_if
        (
            begin(),
            end  (),
            [&]( Node const & other ) { return &other == &module; }
        ) != end(),
        "Module not in chain."
    );
    return static_cast<std::uint8_t>( std::distance( begin(), iterator_to( module ) ) );
#elif 1
    unsigned int index(0);
    for (iterator pCurrentModule(this->begin());; ++pCurrentModule, ++index)
    {
        if (pCurrentModule.get() ==
            module.next_
                .get()) //...mrmlj...ugh...use the next node to handle the case when this module is already removed...
            break;
        LE_ASSERT_MSG(!isEnd(pCurrentModule), "Module not from this chain!");
    }
    LE_ASSERT(index <= this->size());
    return index - 1;
#endif // impl
}

LE_COLD ModuleChainBase::iterator ModuleChainBase::module(std::uint8_t index)
{
#if !defined(LE_NO_RTTI)
    LE_ASSERT_MSG(!dynamic_cast<Engine::ModuleDSP const *>(end().get()),
                  "Root node is not supposed to be an actual module.");
#endif
    iterator pCurrentModule(this->begin());
    while (index && !isEnd(pCurrentModule))
    {
        --index;
        ++pCurrentModule;
    }
    return pCurrentModule;
}
LE_COLD ModuleChainBase::const_iterator ModuleChainBase::module(std::uint8_t const index) const
{
    return const_cast<ModuleChainBase &>(*this).module(index);
}

LE_COLD void ModuleChainBase::clear()
{
    iterator pCurrentModule(this->begin());
    while (pCurrentModule != pCurrentModule.get()->next_)
    {
        pCurrentModule = node_algorithms::unlink(pCurrentModule.get());
    }
    resetRoot();
}

void ModuleChainBase::swap(ModuleChainBase &other)
{
    /// \note A scratch chain on the stack, which allocates nothing: the
    /// constructor links the root node to itself and the destructor clears an
    /// already empty list. `moveAssign` requires an empty destination, which each
    /// of the three steps has by construction.
    ModuleChainBase scratch;
    scratch.moveAssign(std::move(other));
    other.moveAssign(std::move(*this));
    this->moveAssign(std::move(scratch));
}

LE_COLD void ModuleChainBase::moveModule(std::uint8_t const sourceIndex,
                                         std::uint8_t const targetIndex)
{
    LE_ASSUME(sourceIndex != targetIndex);
    //LE_ASSUME( sourceIndex < Constants::maxNumberOfModules );
    //LE_ASSUME( targetIndex < Constants::maxNumberOfModules );
    iterator const pSource(module(sourceIndex));
    iterator const pTarget(module(targetIndex));
    LE_ASSERT(pSource != end());
    LE_ASSERT(pTarget != end());
    node_algorithms::unlink(pSource.get());
    if (sourceIndex < targetIndex)
        node_algorithms::link_after(pTarget.get(), pSource.get());
    else
        node_algorithms::link_before(pTarget.get(), pSource.get());
    //...mrmlj...
    //boost::swap( pSource->next_    , pTarget->next_     );
    //boost::swap( pSource->previous_, pTarget->previous_ );
    //insert( erase( iterator_to( *pSource ) ), *pTarget );
    //insert( erase( iterator_to( *pTarget ) ), *pSource );
}

LE_COLD void ModuleChainBase::push_back(Node &module)
{
    //...mrmlj...LE_ASSERT_MSG( node_algorithms::inited( &module ), "Module already belongs to a different chain." );
    node_algorithms::link_before(this, &module);
}

LE_COLD void ModuleChainBase::insertAtAndReplace(iterator const &pInsertPosition,
                                                 Node &moduleToInsert)
{
    LE_ASSERT_MSG(node_algorithms::inited(&moduleToInsert),
                  "Module already belongs to a different chain.");
    node_algorithms::link_before(pInsertPosition.get(), &moduleToInsert);
    if (!isEnd(pInsertPosition))
        remove(*pInsertPosition);
}

LE_COLD void ModuleChainBase::remove(Node &node)
{
#ifndef NDEBUG
    LE_ASSERT_MSG(&node != this, "You can't remove the root node.");
    auto const previous(node.previous_);
    auto const next(node.next_);
    bool const referenced(node.referenceCount_ > 2);
    LE_ASSERT(previous_);
    LE_ASSERT(next_);
#endif // NDEBUG
    node_algorithms::unlink(&node);
#ifndef NDEBUG
    /// \note The unlink procedure must leave the unlinked node's previous and
    /// next pointers intact in case another thread is using the node and wishes
    /// to continue on to the next one after it finishes with the one being
    /// unlinked.
    ///                                       (11.09.2014.) (Domagoj Saric)
    LE_ASSERT((node.previous_ == previous && node.next_ == next) || !referenced);
#endif // NDEBUG
}

LE_COLD ModuleChainBase::iterator ModuleChainBase::iterator_to(Node &module)
{
    LE_ASSUME(&module);
    return iterator(&module);
}
LE_COLD ModuleChainBase::const_iterator ModuleChainBase::iterator_to(Node const &module)
{
    LE_ASSUME(&module);
    return const_iterator(&module);
}

LE_COLD ModuleChainBase::iterator ModuleChainBase::begin()
{
    //LE_ASSUME( this->next_.get() );
    //return reinterpret_cast<iterator const &>( this->next_ );
    auto *LE_RESTRICT const pNode(this->next_.get());
    LE_ASSUME(pNode);
    return iterator(pNode);
}
LE_COLD ModuleChainBase::const_iterator ModuleChainBase::begin() const
{
    auto const *LE_RESTRICT const pNode(this->next_.get());
    LE_ASSUME(pNode);
    return const_iterator(pNode);
    //return reinterpret_cast<const_iterator const &>( const_cast<ModuleChainBase &>( *this ).begin() );
}

LE_COLD ModuleChainBase::iterator ModuleChainBase::end() { return this; }
LE_COLD ModuleChainBase::const_iterator ModuleChainBase::end() const { return this; }
LE_COLD bool ModuleChainBase::isEnd(Node const *const pNode) const { return pNode == this; }
LE_COLD void ModuleChainBase::resetRoot()
{
    //...mrmlj...a module sent to be destroyed in the GUI thread might still be
    //...mrmlj...referencing the root node...
    //LE_ASSERT( this->referenceCount_ == 1 || this->referenceCount_ == 3 );
    node_algorithms::init_header(&rootNode());
    //LE_ASSERT( this->referenceCount_ == 3 );
}

LE_COLD bool ModuleChainBase::empty() const { return this->next_.get() == this; }

LE_COLD std::uint8_t ModuleChainBase::size() const
{
    return static_cast<std::uint8_t>(node_algorithms::count(&rootNode()) - 1);
}

namespace Detail
{

module_node_traits::node_ptr module_node_traits::get_next(const_node_ptr const n)
{
    return n->next_.get();
}
module_node_traits::node_ptr module_node_traits::get_previous(const_node_ptr const n)
{
    return n->previous_.get();
}

// next and prev can be null here (e.g. when called by the node_algorithms::init() function).
void module_node_traits::set_next(node_ptr const n, node_ptr const next) { n->next_ = next; }
void module_node_traits::set_previous(node_ptr const n, node_ptr const prev)
{
    n->previous_ = prev;
}

//...mrmlj...cannot (fully) use Boost.Intrusive containers yet...
//module_node_value_traits::node_ptr module_node_value_traits::to_node_ptr( value_type & value )
//{
//    return node_ptr( &value );
//}
//
//module_node_value_traits::const_node_ptr module_node_value_traits::to_node_ptr( value_type const & value )
//{
//    return const_node_ptr( &value );
//}
//
//module_node_value_traits::pointer module_node_value_traits::to_value_ptr( node_ptr const & n )
//{
//    //return boost::static_pointer_cast<value_type>( n );
//    return static_cast<value_type *>( n );
//}
//
//module_node_value_traits::const_pointer module_node_value_traits::to_value_ptr( const_node_ptr const & n )
//{
//    //return boost::static_pointer_cast<value_type const>( n );
//    return static_cast<value_type const *>( n );
//}
} // namespace Detail

LE_COLD void ModuleChainImpl::preProcessAll(Parameters::LFOImpl::Timer const &timer,
                                            Setup const &engineSetup)
{
    forEach<Module>([&](Module &module) { module.preProcess(timer, engineSetup); });
}

LE_COLD void ModuleChainImpl::resetAll()
{
    forEach<ModuleDSP>([](ModuleDSP &module) { module.reset(); });
}

namespace
{
ModuleChainImpl::iterator LE_COLD resize(ModuleChainImpl::iterator const pModulesBegin,
                                         ModuleChainImpl::Node const &endNode,
                                         StorageFactors const &factors)
{
    auto pModulePtr(pModulesBegin);
    while (pModulePtr.get() != &endNode)
    {
        auto &module(actualModule<ModuleDSP>(*pModulePtr));
        if (!module.resize(factors))
            break;
        ++pModulePtr;
    }
    return pModulePtr;
}
} // anonymous namespace

LE_COLD bool ModuleChainImpl::resizeAll(Engine::StorageFactors const &newfactors,
                                        Engine::StorageFactors const &currentFactors)
{
    iterator const pModulesBegin(this->begin());
    iterator const reachedIterator(resize(pModulesBegin, *end(), newfactors));
    if (isEnd(reachedIterator))
        return true;
    LE_VERIFY(resize(pModulesBegin, *reachedIterator, currentFactors) == reachedIterator);
    return false;
}

LE_OPTIMIZE_FOR_SIZE_END()

//------------------------------------------------------------------------------
} // namespace Engine
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
