////////////////////////////////////////////////////////////////////////////////
///
/// traversal.cpp
/// -------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "traversal.hpp"

#include <algorithm>
#include <limits>
#include <tuple>

namespace LE::SW::GUI::Accessibility
{

namespace
{
juce::Identifier const orderProperty{"swTraversalOrder"};
juce::Identifier const passThroughProperty{"swTraversalPassThrough"};

bool stopsAt(juce::Component const &component, Traverser::Purpose const purpose)
{
    return (purpose == Traverser::Purpose::keyboard) ? component.isKeyboardFocusContainer()
                                                     : component.isFocusContainer();
}

bool isVisited(juce::Component const &component, Traverser::Purpose const purpose)
{
    if (purpose == Traverser::Purpose::keyboard)
        return component.getWantsKeyboardFocus();
    return !component.getProperties().contains(passThroughProperty);
}

// juce's own order within one parent, which is what an unordered control keeps
void collect(juce::Component const &parent, std::vector<juce::Component *> &into,
             Traverser::Purpose const purpose)
{
    std::vector<juce::Component *> children;
    for (auto *const pChild : parent.getChildren())
        if (pChild->isVisible() &&
            ((purpose == Traverser::Purpose::screenReader) || pChild->isEnabled()))
            children.push_back(pChild);

    auto const attributes([](juce::Component const *const pComponent) {
        auto const explicitOrder(pComponent->getExplicitFocusOrder());
        return std::make_tuple(explicitOrder > 0 ? explicitOrder : std::numeric_limits<int>::max(),
                               pComponent->isAlwaysOnTop() ? 0 : 1, pComponent->getY(),
                               pComponent->getX());
    });
    std::stable_sort(children.begin(), children.end(),
                     [&](auto const *a, auto const *b) { return attributes(a) < attributes(b); });

    for (auto *const pChild : children)
    {
        if (isVisited(*pChild, purpose))
            into.push_back(pChild);
        if (!stopsAt(*pChild, purpose))
            collect(*pChild, into, purpose);
    }
}
} // anonymous namespace

void setTraversalOrder(juce::Component &component, int const order)
{
    component.getProperties().set(orderProperty, order);
}

int traversalOrder(juce::Component const &component)
{
    if (auto const *const pOrder = component.getProperties().getVarPointer(orderProperty))
        return static_cast<int>(*pOrder);
    return 0;
}

void setPassThrough(juce::Component &component)
{
    component.getProperties().set(passThroughProperty, true);
}

int traversalKey(juce::Component const &component, juce::Component const &root)
{
    int key(0);
    for (auto const *pComponent(&component); pComponent && (pComponent != &root);
         pComponent = pComponent->getParentComponent())
        key += traversalOrder(*pComponent);
    return key;
}

std::vector<juce::Component *> Traverser::getAllComponents(juce::Component *const parent)
{
    std::vector<juce::Component *> components;
    if (!parent)
        return components;

    collect(*parent, components, purpose_);

    // unordered last, in juce's order
    auto const keyOf([parent](juce::Component const *const pComponent) {
        auto const key(traversalKey(*pComponent, *parent));
        return (key == 0) ? std::numeric_limits<int>::max() : key;
    });
    std::stable_sort(components.begin(), components.end(),
                     [&](auto const *a, auto const *b) { return keyOf(a) < keyOf(b); });
    return components;
}

juce::Component *Traverser::getDefaultComponent(juce::Component *const parent)
{
    auto const components(getAllComponents(parent));
    return components.empty() ? nullptr : components.front();
}

juce::Component *Traverser::getNextComponent(juce::Component *const current)
{
    return neighbour(current, +1);
}

juce::Component *Traverser::getPreviousComponent(juce::Component *const current)
{
    return neighbour(current, -1);
}

// nothing past either end: juce wraps from getAllComponents()
juce::Component *Traverser::neighbour(juce::Component *const current, int const step)
{
    if (!current)
        return nullptr;

    auto *const pContainer(purpose_ == Purpose::keyboard ? current->findKeyboardFocusContainer()
                                                         : current->findFocusContainer());
    if (!pContainer)
        return nullptr;

    auto const components(getAllComponents(pContainer));
    auto const position(std::find(components.begin(), components.end(), current));
    if (position == components.end())
        return nullptr;

    auto const index(static_cast<std::ptrdiff_t>(position - components.begin()) + step);
    if ((index < 0) || (index >= static_cast<std::ptrdiff_t>(components.size())))
        return nullptr;
    return components[static_cast<std::size_t>(index)];
}

} // namespace LE::SW::GUI::Accessibility
