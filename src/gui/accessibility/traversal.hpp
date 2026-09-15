////////////////////////////////////////////////////////////////////////////////
///
/// \file traversal.hpp
/// -------------------
///
///   The tab and screen reader order, after sst-jucegui's KeyboardTraverser.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef traversal_hpp__A14E3B90_27C6_4D1F_9E58_C0B2D6F7A389
#define traversal_hpp__A14E3B90_27C6_4D1F_9E58_C0B2D6F7A389
//------------------------------------------------------------------------------
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace LE::SW::GUI::Accessibility
{

/// \brief Where \p component sits among what its parent holds. Zero is unset.
void setTraversalOrder(juce::Component &component, int order);
int traversalOrder(juce::Component const &component);

/// \brief Out of the screen reader's list itself, with its children still in.
void setPassThrough(juce::Component &component);

/// \brief The sum of the orders from \p component up to, not including, \p root.
int traversalKey(juce::Component const &component, juce::Component const &root);

class Traverser final : public juce::ComponentTraverser
{
  public:
    enum class Purpose : std::uint8_t
    {
        keyboard,
        screenReader
    };

    explicit Traverser(Purpose const purpose) : purpose_(purpose) {}

    juce::Component *getDefaultComponent(juce::Component *parent) override;
    juce::Component *getNextComponent(juce::Component *current) override;
    juce::Component *getPreviousComponent(juce::Component *current) override;
    std::vector<juce::Component *> getAllComponents(juce::Component *parent) override;

  private:
    juce::Component *neighbour(juce::Component *current, int step);

    Purpose const purpose_;
}; // class Traverser

} // namespace LE::SW::GUI::Accessibility

#endif // traversal_hpp
