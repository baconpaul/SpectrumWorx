////////////////////////////////////////////////////////////////////////////////
///
/// \file page.hpp
/// --------------
///
///   What sw-show-ui can show.
///
///   A page is a name, a sentence, and a factory for the Component that page
/// displays. Pages self-register from their own translation unit, so adding one
/// touches no shared list and a page whose sources do not yet compile can be
/// left out of the target without disturbing the others.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef page_hpp__0F2B7E64_9A15_4C83_B7D0_5E61C2A94F38
#define page_hpp__0F2B7E64_9A15_4C83_B7D0_5E61C2A94F38
//------------------------------------------------------------------------------
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>
//------------------------------------------------------------------------------
namespace SWShowUI
{
//------------------------------------------------------------------------------

struct Page
{
    char const *name;
    char const *description;
    std::unique_ptr<juce::Component> (*construct)();
}; // struct Page

std::vector<Page> const &pages();
Page const *findPage(juce::String const &name);
juce::String defaultPageName();

/// \note Registration runs before main(), so the page list must be a
/// function-local static: a namespace-scope vector would not reliably be
/// constructed first.
void registerPage(Page page);

/// One of these at namespace scope in a page's .cpp is the whole registration.
struct PageRegistration
{
    PageRegistration(char const *const name, char const *const description,
                     std::unique_ptr<juce::Component> (*const construct)())
    {
        registerPage({name, description, construct});
    }
}; // struct PageRegistration

//------------------------------------------------------------------------------
} // namespace SWShowUI
//------------------------------------------------------------------------------
#endif // page_hpp
