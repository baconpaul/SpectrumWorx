////////////////////////////////////////////////////////////////////////////////
///
/// \file page.cpp
/// --------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "page.hpp"

#include <algorithm>
//------------------------------------------------------------------------------
namespace SWShowUI
{
//------------------------------------------------------------------------------

namespace
{
std::vector<Page> &mutablePages()
{
    static std::vector<Page> pages;
    return pages;
}
} // anonymous namespace

void registerPage(Page page)
{
    auto &pages(mutablePages());
    pages.push_back(page);
    // Static initialisation order across translation units is unspecified;
    // sorting keeps --list and the default page deterministic regardless.
    std::sort(pages.begin(), pages.end(), [](Page const &left, Page const &right) {
        return juce::String(left.name).compareIgnoreCase(right.name) < 0;
    });
}

std::vector<Page> const &pages() { return mutablePages(); }

Page const *findPage(juce::String const &name)
{
    for (auto const &page : pages())
        if (name == page.name)
            return &page;
    return nullptr;
}

juce::String defaultPageName()
{
    return pages().empty() ? juce::String() : juce::String(pages().front().name);
}

//------------------------------------------------------------------------------
} // namespace SWShowUI
//------------------------------------------------------------------------------
