////////////////////////////////////////////////////////////////////////////////
///
/// \file intrusivePtr.hpp
/// ----------------------
///
///   Replaces Boost.SmartPtr's intrusive_ptr. The ADL hooks keep their Boost
/// spelling - intrusive_ptr_add_ref() and intrusive_ptr_release() - because the
/// types that provide them declare those functions as friends and there is
/// nothing to be gained from renaming a dozen friend declarations.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef intrusivePtr_hpp__9B2E5D14_A73C_4F60_8E19_C4D7052BA3F8
#define intrusivePtr_hpp__9B2E5D14_A73C_4F60_8E19_C4D7052BA3F8
//------------------------------------------------------------------------------
#include "abi.hpp"

#include <cstddef>
#include <utility>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

template <class T> class IntrusivePtr
{
  public:
    using element_type = T;

    constexpr IntrusivePtr() noexcept : p_(nullptr) {}
    constexpr IntrusivePtr(std::nullptr_t) noexcept : p_(nullptr) {}

    IntrusivePtr(T *const p, bool const addReference = true) : p_(p)
    {
        if (p_ && addReference)
            intrusive_ptr_add_ref(p_);
    }

    IntrusivePtr(IntrusivePtr const &other) : p_(other.p_)
    {
        if (p_)
            intrusive_ptr_add_ref(p_);
    }

    IntrusivePtr(IntrusivePtr &&other) noexcept : p_(other.p_) { other.p_ = nullptr; }

    /// Non-const to const, derived to base.
    template <class U> IntrusivePtr(IntrusivePtr<U> const &other) : p_(other.get())
    {
        if (p_)
            intrusive_ptr_add_ref(p_);
    }

    ~IntrusivePtr()
    {
        if (p_)
            intrusive_ptr_release(p_);
    }

    IntrusivePtr &operator=(IntrusivePtr const &other)
    {
        IntrusivePtr(other).swap(*this);
        return *this;
    }

    IntrusivePtr &operator=(IntrusivePtr &&other) noexcept
    {
        IntrusivePtr(std::move(other)).swap(*this);
        return *this;
    }

    IntrusivePtr &operator=(T *const p)
    {
        IntrusivePtr(p).swap(*this);
        return *this;
    }

    void reset() { IntrusivePtr().swap(*this); }
    void reset(T *const p) { IntrusivePtr(p).swap(*this); }
    void reset(T *const p, bool const addReference) { IntrusivePtr(p, addReference).swap(*this); }

    T *get() const noexcept { return p_; }

    /// Relinquishes ownership without releasing the reference.
    T *detach() noexcept
    {
        T *const p(p_);
        p_ = nullptr;
        return p;
    }

    T &operator*() const noexcept { return *p_; }
    T *operator->() const noexcept { return p_; }

    explicit operator bool() const noexcept { return p_ != nullptr; }

    void swap(IntrusivePtr &other) noexcept
    {
        T *const p(p_);
        p_ = other.p_;
        other.p_ = p;
    }

  private:
    T *p_;
}; // class IntrusivePtr

template <class T, class U>
bool operator==(IntrusivePtr<T> const &left, IntrusivePtr<U> const &right) noexcept
{
    return left.get() == right.get();
}

template <class T, class U> bool operator==(IntrusivePtr<T> const &left, U *const right) noexcept
{
    return left.get() == right;
}

template <class T> bool operator==(IntrusivePtr<T> const &left, std::nullptr_t) noexcept
{
    return left.get() == nullptr;
}

template <class T> void swap(IntrusivePtr<T> &left, IntrusivePtr<T> &right) noexcept
{
    left.swap(right);
}

/// LE::Utility::getPointer equivalent, for the call sites that used it.
template <class T> T *getPointer(IntrusivePtr<T> const &p) noexcept { return p.get(); }

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // intrusivePtr_hpp
