////////////////////////////////////////////////////////////////////////////////
///
/// vector.cpp
/// ----------
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#if defined(__APPLE__)
#include "TargetConditionals.h"
#if (TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR)
#include "Availability.h"
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_5_0
#pragma message("LE.Math.Vector using "                                                            \
                "iOS 4.3 Accelerate framework.")
#define LE_ACC_NO_VFORCE 1
#else
#pragma message("LE.Math.Vector using "                                                            \
                "iOS 5.0 Accelerate framework.")
#define LE_ACC_NO_VFORCE 0
#endif
#define LE_MATH_USE_ACC
#define LE_MATH_NATIVE_POINTER_SIZE_INTERFACE
#else
#define LE_ACC_NO_VFORCE 0
#define LE_MATH_USE_ACC
#define LE_MATH_NATIVE_POINTER_SIZE_INTERFACE
/// \note The text named OS X 10.4, an OS from 2005 and the one thing in the
/// message that had stopped being worth saying. What it is for -- telling you at
/// build time which backend the vector primitives were compiled against -- is
/// all that is left of it.
///                                           (02.08.2026.) (SW port)
#pragma message("LE.Math.Vector using the Accelerate framework.")
// NT2 seems still slightly slower than Acc. on OS X...
//#define LE_MATH_USE_NT2
#endif // OSX/iOS

//...mrmlj...Boost.Fusion vs ObjC "nil" name clash quick-fix...
#undef nil
#else
/// \note NT2 is opt-in now: without LE_HAS_NT2 the primitives below fall back
/// to scalar loops. Stage 4 replaces the backend outright.
///                                       (28.07.2026.) (SW port)
#ifdef LE_HAS_NT2
#define LE_MATH_USE_NT2
#ifdef _XBOX
/// \note NT2 does not yet support Xbox's custom AltiVec.
///                                   (17.02.2012.) (Domagoj Saric)
//#define BOOST_SIMD_HAS_VMX_SUPPORT
#endif
#endif // LE_HAS_NT2
#endif

#ifdef LE_MATH_USE_NT2
#define LE_NOINLINE_NT2 LE_NOINLINE
#else
#define LE_NOINLINE_NT2
#endif
//------------------------------------------------------------------------------

// Implementation note:
//   Disable the debug runtime checks for this module in order to get bearable
// debug performance from NT2. Unfortunately this does not seem to work with
// MSVC10 so it is done globally in the CMake project files.
//                                            (25.08.2011.) (Domagoj Saric)
#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma check_stack(off)
#endif // MSVC

#include "le/utility/platformSpecifics.hpp"
#include "le/utility/span.hpp"

LE_OPTIMIZE_FOR_SPEED_BEGIN()
LE_FAST_MATH_ON()

// A nice list of profilers:
// http://stackoverflow.com/questions/4394606/beyond-stack-sampling-c-profilers

#include "vector.hpp"

#include "constants.hpp"
#include "conversion.hpp"
#include "math.hpp"

#include "le/utility/intrinsics.hpp"

/// \note These were reached transitively through the NT2 and Accelerate headers.
/// The portable arms below use std::memcpy / memmove / memset, std::log, exp,
/// sqrt, sin, cos and std::{min,max}_element with neither in the include graph.
///                                           (29.07.2026.) (SW port)
#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef LE_MATH_USE_NT2

// NT2

// http://nt2.metascale.org/doc/html
// http://nt2.metascale.fr/doc/html/setting_up_nt__.html
// http://nt2.metascale.org/doc/html/module_system/structure_of_a_module.html
// http://nt2.metascale.fr/doc/html/boost_simd_functions_and_operators/general_informations.html#boost_simd_functions_and_operators.general_informations.how_to_use_a_boost_simd_function_or_constant__

//...mrmlj...
#define BOOST_SIMD_TOOLBOX_IEEE_HAS_COPYSIGN
#define BOOST_SIMD_HAS_FABSF

#include "boost/simd/sdk/simd/extensions.hpp"

// http://v8.googlecode.com/svn-history/r2949/trunk/src/arm/constants-arm.h
// https://wiki.edubuntu.org/ARM/Thumb2PortingHowto
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0205g/Babbacdb.html
// http://stackoverflow.com/questions/13782488/android-ndk-cpu-macros-armv7
// https://sourceware.org/ml/libc-ports/2012-08/msg00150.html
#if defined(BOOST_SIMD_DETECTED) || (defined(__GNUC__) && (__ARM_ARCH >= 7))
#define LE_MATH_NT2_FULL_VECTORIZATION
#elif defined(__GNUC__) && !defined(__SOFTFP__)
#define LE_MATH_NT2_SIMPLE_VECTORIZATION
#else
#define LE_MATH_NT2_NO_VECTORIZATION
#endif

#include "nt2/signal/details/missing_functionality.hpp"
#include "nt2/signal/details/extra_registers.hpp"
#include "nt2/signal/details/interleaved_data_transformation.hpp"

#include "boost/simd/arithmetic/include/functions/simd/sqr.hpp"
#include "boost/simd/arithmetic/include/functions/simd/sqrt.hpp"
#include "boost/simd/arithmetic/include/functions/simd/fma.hpp"
#include "boost/simd/constant/constants/pi.hpp"
#include "boost/simd/constant/constants/zero.hpp"
#include "boost/simd/include/functions/simd/load.hpp"
#include "boost/simd/include/functions/simd/store.hpp"
#include "boost/simd/memory/is_aligned.hpp"
#include "boost/simd/memory/include/functions/simd/splat.hpp"
#include "boost/simd/meta/is_pointing_to.hpp"
#include "boost/simd/operator/include/functions/simd/multiplies.hpp"
#include "boost/simd/operator/include/functions/simd/plus.hpp"
#include "boost/simd/operator/include/functions/simd/unary_minus.hpp"

#include "boost/simd/sdk/config/arch.hpp"
#include "boost/simd/sdk/simd/native.hpp"
#include "boost/simd/swar/include/functions/simd/reverse.hpp"
#include "boost/simd/arithmetic/include/functions/simd/fast_hypot.hpp"

#include "nt2/exponential/include/functions/simd/exp.hpp"
#include "nt2/exponential/include/functions/simd/log.hpp"
#if defined(LE_MATH_NT2_FULL_VECTORIZATION)
#include "nt2/trigonometric/include/functions/simd/nbd_atan2.hpp"
// https://bugs.launchpad.net/linaro-android/+bug/908125
#include "nt2/trigonometric/include/functions/simd/sinecosine.hpp"
#else
#include "nt2/trigonometric/include/functions/scalar/nbd_atan2.hpp"
#include "nt2/trigonometric/include/functions/scalar/sinecosine.hpp"
#endif // LE_MATH_NT2_FULL_VECTORIZATION

#pragma message("LE.Math.Vector using NT2 for " BOOST_SIMD_ARCH " with " BOOST_SIMD_STRING ".")

#endif // LE_MATH_USE_NT2

#if defined(LE_MATH_USE_ACC)
// Implementation note:
//   vForce uses int const * to pass the size parameter while we use
// unsigned int so we assert here that it is safe to do a pointer
// reinterpret_cast.
//                                        (17.05.2011.) (Domagoj Saric)
static_assert((std::endian::native == std::endian::little) || (sizeof(unsigned int) == sizeof(int)),
              "Unexpected data sizes");

#if (TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR || (__MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_10_9))
#include "Accelerate/Accelerate.h"
#else
#include "vecLib/vDSP.h"
#include "vecLib/vForce.h"
#endif
#endif // LE_MATH_USE_ACC

// Other OSS libs
// http://simdx86.sourceforge.net
// http://sseplus.sourceforge.net
// http://sourceforge.net/projects/v3d
// http://sourceforge.net/projects/libsimd
// http://sourceforge.net/projects/framewave

#include "le/utility/assert.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <numeric>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Math
{
//------------------------------------------------------------------------------

#ifdef LE_MATH_USE_NT2

namespace
{
using vector_t = boost::simd::native<float, BOOST_SIMD_DEFAULT_EXTENSION>;
using native_t = vector_t::native_type;

//...mrmlj...temporary "quick-patches"/workarounds to get the most out of
//...mrmlj...the incomplete NT2 ARM support (based on GCC/Clang builtin
//...mrmlj...vector support)
#if defined(LE_MATH_NT2_FULL_VECTORIZATION)
using complex_op_vector_t = vector_t;
using simple_op_vector_t = vector_t;
using complex_tag = boost::simd::tag::simd_type;
using simple_tag = boost::simd::tag::simd_type;
template <typename T> T *asComplex(T *const pointer) { return pointer; }
template <typename T> T *asSimple(T *const pointer) { return pointer; }
#elif defined(LE_MATH_NT2_SIMPLE_VECTORIZATION)
using complex_op_vector_t = float;
using simple_op_vector_t = vector_t;
using complex_tag = boost::simd::tag::not_simd_type;
using simple_tag = boost::simd::tag::simd_type;
LE_ALIGN(BOOST_SIMD_CONFIG_ALIGNMENT) complex_op_vector_t *asComplex(vector_t *const pointer)
{
    return pointer->data();
}
LE_ALIGN(BOOST_SIMD_CONFIG_ALIGNMENT)
complex_op_vector_t const *asComplex(vector_t const *const pointer) { return pointer->data(); }
LE_ALIGN(BOOST_SIMD_CONFIG_ALIGNMENT) simple_op_vector_t *asSimple(vector_t *const pointer)
{
    return pointer;
}
LE_ALIGN(BOOST_SIMD_CONFIG_ALIGNMENT)
simple_op_vector_t const *asSimple(vector_t const *const pointer) { return pointer; }
#else  // LE_MATH_NT2_NO_VECTORIZATION
using complex_op_vector_t = float;
using simple_op_vector_t = float;
using complex_tag = boost::simd::tag::not_simd_type;
using simple_tag = boost::simd::tag::not_simd_type;
LE_ALIGN(BOOST_SIMD_CONFIG_ALIGNMENT) complex_op_vector_t *asComplex(vector_t *const pointer)
{
    return pointer->data();
}
LE_ALIGN(BOOST_SIMD_CONFIG_ALIGNMENT)
complex_op_vector_t const *asComplex(vector_t const *const pointer) { return pointer->data(); }
LE_ALIGN(BOOST_SIMD_CONFIG_ALIGNMENT) simple_op_vector_t *asSimple(vector_t *const pointer)
{
    return pointer->data();
}
LE_ALIGN(BOOST_SIMD_CONFIG_ALIGNMENT)
simple_op_vector_t const *asSimple(vector_t const *const pointer) { return pointer->data(); }
#endif // BOOST_SIMD_DETECTED

std::uint16_t adjustVectorCounter(unsigned int const count)
{
    return static_cast<std::uint16_t>(count * sizeof(vector_t) / sizeof(complex_op_vector_t));
}

using vector_ptr_t = vector_t *LE_RESTRICT;
using vector_c_ptr_t = vector_t const *LE_RESTRICT;
using extra_vector_ptr_t = boost::simd::make_extra_pointer_register<vector_t>::type;
using extra_vector_c_ptr_t = boost::simd::make_extra_pointer_register<vector_t const>::type;

using AlignedRange = LE::Utility::Span<vector_t>;

vector_t *alignDown(float *const p_unaligned)
{
    return reinterpret_cast<vector_t *>(reinterpret_cast<std::size_t>(p_unaligned) &
                                        ~(sizeof(vector_t) - 1));
}

vector_t const *alignDown(float const *const p_unaligned)
{
    return alignDown(const_cast<float *>(p_unaligned));
}

vector_t *alignUp(float *const p_unaligned)
{
    return reinterpret_cast<vector_t *>(
        (reinterpret_cast<std::size_t>(p_unaligned) + sizeof(vector_t) - 1) &
        ~(sizeof(vector_t) - 1));
}

vector_t const *alignUp(float const *const p_unaligned)
{
    return alignUp(const_cast<float *>(p_unaligned));
}

#pragma warning(push)
#pragma warning(disable : 4512) // Assignment operator could not be generated.

class EdgeRestoredAlignedRange : public AlignedRange
{
  public:
    LE_NOINLINE EdgeRestoredAlignedRange(float *LE_RESTRICT const pBegin,
                                         float const *LE_RESTRICT const pEnd)
        : AlignedRange(alignDown(pBegin), alignUp(const_cast<float *>(pEnd))),
          beginRestoreSize_(static_cast<unsigned short>(pBegin - begin()->data())),
          endRestoreSize_(static_cast<unsigned short>(end()->data() - pEnd)),
#ifdef NDEBUG
          beginData_(front()), endData_(back())
#else
          beginData_(empty() ? vector_t() : front()), endData_(empty() ? vector_t() : back())
#endif // NDEBUG
    {
        //...mrmlj...no stack alignment test...
        //nt2::unaligned_store( front(), beginData_.begin() );
        //nt2::unaligned_store( back (), endData_  .begin() );
    }

    LE_NOINLINE ~EdgeRestoredAlignedRange()
    {
#ifndef NDEBUG
        if (empty())
            return;
#endif // NDEBUG

        using boost::fusion::at_c;

        // Implementation note:
        //   Duff's device, we do not want no memcpy() calls inserted
        // because of 1-3 floats.
        //                                (03.08.2011.) (Domagoj Saric)

        {
            LE_ALIGN(BOOST_SIMD_CONFIG_ALIGNMENT)
            float *LE_RESTRICT const pBegin(this->front().data());
            switch (beginRestoreSize_)
            {
            case 3:
                pBegin[2] = at_c<2>(beginData_);
            case 2:
                pBegin[1] = at_c<1>(beginData_);
            case 1:
                pBegin[0] = at_c<0>(beginData_);
            case 0:
                break;

                LE_DEFAULT_CASE_UNREACHABLE();
            }
        }
        {
            LE_ALIGN(BOOST_SIMD_CONFIG_ALIGNMENT)
            float *LE_RESTRICT const pEnd(this->back().data());
            switch (endRestoreSize_)
            {
            case 3:
                pEnd[1] = at_c<1>(endData_);
            case 2:
                pEnd[2] = at_c<2>(endData_);
            case 1:
                pEnd[3] = at_c<3>(endData_);
            case 0:
                break;

                LE_DEFAULT_CASE_UNREACHABLE();
            }
        }
    }

    unsigned int size() const { return static_cast<unsigned int>(AlignedRange::size()); }

    bool compatiblyAligned(float const *const pointer) const
    {
        return static_cast<std::size_t>(pointer - alignDown(pointer)->data()) == beginRestoreSize_;
    }

  private: // Hide mutable iterator_range members.
    void advance_begin();
    void advance_end();

    void pop_front();
    void pop_back();

  private:
    unsigned short const beginRestoreSize_;
    unsigned short const endRestoreSize_;

    vector_t const beginData_;
    vector_t const endData_;

    //...mrmlj...no stack alignment test...
    //using UnalignedData = std::array<vector_t::value_type, vector_t::static_size>;
    //UnalignedData beginData_;
    //UnalignedData endData_  ;
};

#pragma warning(pop)

template <typename T> class Index
{
  public:
    Index(std::uint16_t const typedIndex) : index_(typedIndex * sizeof(T)) {}

    Index &operator++()
    {
        index_ += sizeof(T);
        return *this;
    }
    Index &operator--()
    {
        index_ -= sizeof(T);
        return *this;
    }

    Index &operator+=(std::int16_t const numberOfElements)
    {
        index_ += numberOfElements * sizeof(T);
        return *this;
    }
    Index &operator-=(std::int16_t const numberOfElements)
    {
        index_ -= numberOfElements * sizeof(T);
        return *this;
    }

    bool operator==(Index const &other) const { return index_ == other.index_; }
    bool operator!=(Index const &other) const { return index_ != other.index_; }

    std::uint32_t byteIndex() const { return index_; }

    explicit operator bool() const { return index_ != 0; }

  private:
    std::uint32_t index_;
}; // Index

#pragma warning(push)
#pragma warning(disable : 4512) // Assignment operator could not be generated.

template <typename T> class FastIndexedPointer
{
  private:
    using Byte = typename boost::mpl::if_<std::is_const<T>, char const, char>::type;

  public:
    using index_type = Index<T const>;

    FastIndexedPointer(T *LE_RESTRICT const typedPointer)
        : pointer_(reinterpret_cast<Byte *>(typedPointer))
    {
        LE_ASSUME(reinterpret_cast<std::size_t>(pointer_) % sizeof(T) == 0);
    }

    LE_FORCEINLINE T &operator[](index_type const &index) const
    {
        Byte *LE_RESTRICT const pElement(&pointer_[index.byteIndex()]);
        LE_ASSUME(pElement);
        return *reinterpret_cast<T *LE_RESTRICT>(pElement);
    }

  private:
    Byte *LE_RESTRICT const pointer_;
}; // class FastIndexedPointer

#pragma warning(pop)
} // anonymous namespace

#endif // LE_MATH_USE_NT2

namespace Constants
{
#ifdef LE_MATH_USE_NT2
std::size_t const vectorSize = vector_t::static_size;
#else
std::size_t const vectorSize = Utility::Constants::vectorAlignment / sizeof(float);
#endif // LE_MATH_USE_NT2
} // namespace Constants

void *align(void *const pointer)
{
    std::size_t const vectorAlignment = Constants::vectorSize * sizeof(float); //...mrmlj...
    return reinterpret_cast<void *>((reinterpret_cast<std::size_t>(pointer) + vectorAlignment - 1) &
                                    ~(vectorAlignment - 1));
}

unsigned int alignIndex(unsigned int const index)
{
    return (index + Constants::vectorSize - 1) & ~(Constants::vectorSize - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// Range based interfaces.
////////////////////////////////////////////////////////////////////////////////

void copy(InputRange const &input, OutputRange const &output)
{
    LE_ASSERT_MSG(input.size() <= output.size(), "Buffer sizes mismatch.");
    copy(input.begin(), output.begin(), static_cast<unsigned int>(input.size()));
}

void clear(InputOutputRange const data)
{
#ifdef LE_MATH_NATIVE_POINTER_SIZE_INTERFACE
    clear(data.begin(), static_cast<unsigned int>(data.size()));
#else
    clear(data.begin(), data.end());
#endif
}

void fill(InputOutputRange const data, float const value)
{
#ifdef LE_MATH_NATIVE_POINTER_SIZE_INTERFACE
    fill(data.begin(), value, static_cast<unsigned int>(data.size()));
#else
    fill(data.begin(), data.end(), value);
#endif
}

/// \note vector.hpp declared this alongside the strided overload but nothing
/// ever defined it. Nothing called it either; sw-tests is the first thing that
/// tried.
///                                       (28.07.2026.) (SW port)
void negate(InputOutputRange const data) { negate(data, 1); }

void negate(InputOutputRange data, unsigned int const stride)
{
#if defined(LE_MATH_USE_ACC)
    negate(data.begin(), stride, static_cast<unsigned int>(data.size()));
#else
    // Implementation note:
    //   We cannot just write while ( data ) because this checks for equality
    // between begin() and end() and begin can actually go past end with strides
    // larger than one.
    //                                        (01.07.2011.) (Domagoj Saric)
    while (data.begin() < data.end())
    {
        data.front() = -data.front();
        data.advance_begin(stride);
    }
#endif
}

float const &min(InputRange const &data)
{
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    return min(data.begin(), static_cast<unsigned int>(data.size()));
#else
    return min(data.begin(), data.end());
#endif
}

float const &max(InputRange const &data)
{
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    return max(data.begin(), static_cast<unsigned int>(data.size()));
#else
    return max(data.begin(), data.end());
#endif
}

void add(InputRange const &input, InputOutputRange const &inputOutput)
{
    LE_ASSERT_MSG(input.size() <= inputOutput.size(), "Buffer sizes mismatch.");
    add(input.begin(), inputOutput.begin(), inputOutput.end());
}

void add(InputRange const &input, float const constant, OutputRange const &output)
{
    LE_ASSERT_MSG(input.size() <= output.size(), "Buffer sizes mismatch.");
    add(input.begin(), constant, output.begin(), output.end());
}

void multiply(InputRange const &input, float const multiplier, OutputRange const output)
{
    LE_ASSERT_MSG(input.size() <= output.size(), "Buffer sizes mismatch.");
    multiply(multiplier, input.begin(), output.begin(), output.begin() + input.size());
}

void multiply(InputOutputRange const &data, float const multiplier)
{
    multiply(multiplier, data.begin(), data.end());
}

void ln(InputRange const &input, OutputRange const &output)
{
    LE_ASSERT_MSG(input.size() == output.size(), "Buffer sizes mismatch.");
    ln(input.begin(), output.begin(), output.end());
}

void ln(InputOutputRange const &data) { ln(data.begin(), data.end()); }

void exp(InputOutputRange const &data) { exp(data.begin(), data.end()); }

void square(InputOutputRange const data) { square(data.begin(), data.end()); }

void squareRoot(InputOutputRange const data) { squareRoot(data.begin(), data.end()); }

float rms(InputRange const &data) { return rms(data.begin(), data.end()); }

void mix(InputRange const &amps, InputRange const &phases, InputOutputRange const &realsParam,
         InputOutputRange const &imagsParam, float const amPhGain, float const reImGain)
{
    //...mrmlj...use vForce...
    // https://developer.apple.com/library/mac/#documentation/Performance/Conceptual/vecLib/Reference/reference.html

#ifdef LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const reals(realsParam.begin(), realsParam.end());
    EdgeRestoredAlignedRange const imags(imagsParam.begin(), imagsParam.end());
    LE_ASSERT_MSG(reals.compatiblyAligned(amps.begin()), "Misaligned data");
    LE_ASSERT_MSG(reals.compatiblyAligned(phases.begin()), "Misaligned data");

    complex_op_vector_t const amPhWeight(boost::simd::splat<complex_op_vector_t>(amPhGain));
    complex_op_vector_t const reImWeight(boost::simd::splat<complex_op_vector_t>(reImGain));

    complex_op_vector_t const *LE_RESTRICT pAmp(asComplex(alignDown(amps.begin())));
    complex_op_vector_t const *LE_RESTRICT pPhase(asComplex(alignDown(phases.begin())));
    complex_op_vector_t *LE_RESTRICT pReal(asComplex(reals.begin()));
    complex_op_vector_t *LE_RESTRICT pImag(asComplex(imags.begin()));
    auto counter(adjustVectorCounter(reals.size()));
    while (counter--)
    {
        complex_op_vector_t inputReal;
        complex_op_vector_t inputImag(nt2::sinecosine<nt2::small_>(*pPhase++, inputReal));
        complex_op_vector_t const weightedAmp(*pAmp++ * amPhWeight);
        inputReal *= weightedAmp;
        inputImag *= weightedAmp;

        complex_op_vector_t const outputReal(*pReal * reImWeight);
        complex_op_vector_t const outputImag(*pImag * reImWeight);

        *pReal++ = inputReal + outputReal;
        *pImag++ = inputImag + outputImag;
    }
#else
    LE_ASSERT_MSG(amps.size() >= realsParam.size(), "Buffer sizes mismatch.");
    LE_ASSERT_MSG(phases.size() >= realsParam.size(), "Buffer sizes mismatch.");
    LE_ASSERT_MSG(realsParam.size() == imagsParam.size(), "Buffer sizes mismatch.");

    float const *LE_RESTRICT pAmp(amps.begin());
    float const *LE_RESTRICT pPhase(phases.begin());
    float *LE_RESTRICT pReal(realsParam.begin());
    float *LE_RESTRICT pImag(imagsParam.begin());
    auto counter(realsParam.size());
    while (counter--)
    {
        float const weightedAmp(*pAmp++ * amPhGain);
        float const phase(*pPhase++);
        *pReal = weightedAmp * std::cos(phase) + *pReal * reImGain;
        *pImag = weightedAmp * std::sin(phase) + *pImag * reImGain;
        ++pReal;
        ++pImag;
    }
#endif // LE_MATH_USE_NT2
}

void mix(InputRange const &amps, InputRange const &phases, InputOutputRange const &realsParam,
         InputOutputRange const &imagsParam, float const amPhWeight)
{
    mix(amps, phases, realsParam, imagsParam, amPhWeight, 1 - amPhWeight);
}

// http://www.analog.com/static/imported-files/tech_docs/dsp_book_Ch15.pdf
void movingAverage(InputOutputRange const &data, unsigned int const windowWidth)
{
    LE_ASSERT_MSG(windowWidth, "Wrong parameters.");

    float const inverseWindowWidth(1 / convert<float>(windowWidth));
    InputOutputRange window(&data[0], &data[windowWidth]);
    float sum(std::accumulate(window.begin(), window.end(), 0.0f));

    // Main full window section
    {
        while (window.end() != data.end())
        {
            float const oldSumValue(window.front());
            window.front() = sum * inverseWindowWidth;
            window.advance_begin(1);
            window.advance_end(1);
            float const newSumValue(window.back());
            sum -= oldSumValue;
            sum += newSumValue;
        }
    }

    // Trailing partial window section
    {
        float tailWindowWidth(convert<float>(windowWidth));
        while (window)
        {
            float const oldSumValue(window.front());
            window.front() = sum / tailWindowWidth--;
            window.advance_begin(1);
            sum -= oldSumValue;
        }
    }
}

void symmetricMovingAverage(InputRange const &input, OutputRange const output,
                            unsigned int const windowWidth)
{
    LE_ASSERT_MSG(windowWidth, "Wrong parameters.");
    LE_ASSERT_MSG(input.size() == output.size(), "Input/output buffer sizes mismatched.");
    LE_ASSERT_MSG(unsigned(input.size()) > windowWidth, "Window larger than data.");

    unsigned int const halfWindowWidth(windowWidth / 2);
    unsigned int const fullWindowWidth(halfWindowWidth + 1 + halfWindowWidth);
    InputRange window(&input[0], &input[halfWindowWidth + 1 - 1] + 1);
    float *pOutputSample(output.begin());
    float sum(std::accumulate(window.begin(), window.end(), 0.0f));

    // Leading partial window section (before the halfWindowWidth + 1 sample)
    {
        LE_ASSERT_MSG(unsigned(window.size()) == halfWindowWidth + 1, "Algorithm bug.");

        float leadWindowWidth(convert<float>(halfWindowWidth + 1));
        float const *const pLeadWindowEnd(&input[fullWindowWidth - 1] + 1);
        while (window.end() != pLeadWindowEnd)
        {
            *pOutputSample++ = sum / leadWindowWidth++;
            window.advance_end(1);
            sum += window.back();
        }
    }

    // Main full window section
    {
        LE_ASSERT_MSG(pOutputSample == &output[halfWindowWidth], "Algorithm bug.");
        LE_ASSERT_MSG(unsigned(window.size()) == fullWindowWidth, "Algorithm bug.");

        float const inverseWindowWidth(1 / convert<float>(fullWindowWidth));
        while (window.end() != input.end())
        {
            *pOutputSample++ = sum * inverseWindowWidth;
            sum -= window.front();
            window.advance_begin(1);
            window.advance_end(1);
            sum += window.back();
        }
    }

    // Trailing partial window section
    {
        LE_ASSERT_MSG(pOutputSample == output.end() - (halfWindowWidth + 1), "Algorithm bug.");
        LE_ASSERT_MSG(unsigned(window.size()) == fullWindowWidth, "Algorithm bug.");

        float tailWindowWidth(convert<float>(fullWindowWidth));
        while (pOutputSample != output.end())
        {
            *pOutputSample++ = sum / tailWindowWidth--;
            sum -= window.front();
            window.advance_begin(1);
        }
    }
}

void swap(InputOutputRange const &range1, InputOutputRange const &range2)
{
    LE_ASSERT_MSG(range1.size() == range2.size(), "Buffer sizes mismatch.");
#ifdef LE_MATH_NATIVE_POINTER_SIZE_INTERFACE
    swap(range1.begin(), range2.begin(), static_cast<unsigned int>(range1.size()));
#else
    swap(range1.begin(), range1.end(), range2.begin());
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// Iterator interfaces.
////////////////////////////////////////////////////////////////////////////////

void copy(float const *const pBegin, float const *const pBeginEnd, float *const pDestination)
{
    LE_ASSERT_MSG(pBegin <= pBeginEnd, "Invalid range.");
    copy(pBegin, pDestination, static_cast<unsigned int>(pBeginEnd - pBegin));
}

void clear(float *const pBegin, float const *const pEnd)
{
    LE_ASSERT_MSG(pBegin <= pEnd, "Invalid range.");
    clear(pBegin, static_cast<unsigned int>(pEnd - pBegin));
}

void fill(float *const pBegin, float const *const pEnd, float const value)
{
    LE_ASSERT_MSG(pBegin <= pEnd, "Invalid range.");
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    fill(pBegin, value, static_cast<unsigned int>(pEnd - pBegin));
#else
    std::fill<float *>(pBegin, const_cast<float *>(pEnd), value);
#endif
}

void negate(float *pBegin, float const *const pEnd)
{
    LE_ASSERT_MSG(pBegin <= pEnd, "Invalid range.");
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)

    negate(pBegin, static_cast<unsigned int>(pEnd - pBegin));

#elif defined(LE_MATH_USE_NT2)

    EdgeRestoredAlignedRange const outputRange(pBegin, pEnd);
    for (auto &pack : outputRange)
    {
        pack = -pack;
    }

#else
    while (pBegin != pEnd)
    {
        *pBegin = -*pBegin;
        ++pBegin;
    }
#endif
}

void reverse(float *LE_RESTRICT const pBegin, float const *const pEnd)
{
    LE_ASSERT_MSG(pBegin <= pEnd, "Invalid range.");
    //...mrmlj...ACC vDSP_vrvrs seems slower/non-vectorized so we use NT2 whenever possible...
#if defined(LE_MATH_USE_NT2)
    if (boost::simd::is_aligned(pBegin) && boost::simd::is_aligned(pEnd))
    {
        auto *LE_RESTRICT pLeft(reinterpret_cast<vector_t *>(pBegin));
        auto *LE_RESTRICT pRight(
            const_cast<vector_t *>(reinterpret_cast<vector_t const *>(pEnd) - 1));
        while (pLeft < pRight)
        {
            vector_t const reversedLeft(boost::simd::reverse(*pLeft));
            vector_t const reversedRight(boost::simd::reverse(*pRight));
            *pRight-- = reversedLeft;
            *pLeft++ = reversedRight;
        }
    }
    else
    {
        std::reverse /*<float * LE_RESTRICT>*/ (pBegin, const_cast<float *LE_RESTRICT>(pEnd));
    }
#elif defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE) &&                                            \
    !defined(LE_MATH_USE_ACC) /*not really vectorized*/
    reverse(pBegin, pEnd - pBegin);
#else
    std::reverse /*<float * LE_RESTRICT>*/ (pBegin, const_cast<float *LE_RESTRICT>(pEnd));
#endif
}

void swap(float *LE_RESTRICT const pBegin, float const *const pEnd,
          float *LE_RESTRICT const pDestination)
{
    LE_ASSERT_MSG(pBegin <= pEnd, "Invalid range.");
    //...mrmlj...ACC vDSP_vswap seems slower/non-vectorized so we use NT2 whenever possible...
#if defined(LE_MATH_USE_NT2)
    EdgeRestoredAlignedRange const data1(pBegin, pEnd);
    EdgeRestoredAlignedRange const data2(pDestination, pDestination + (pEnd - pBegin));
    LE_ASSERT_MSG(data1.compatiblyAligned(pDestination), "Misaligned data");
    if (data1.size() % 4 == 0)
    {
        // we can unroll:
        vector_t *LE_RESTRICT pData1(data1.begin());
        vector_t *LE_RESTRICT pData2(data2.begin());
        vector_t const *LE_RESTRICT const pData2End(data2.end());
        while (pData2 != pData2End)
        {
            using native_t = vector_t::native_type;

            native_t const data1_0(pData1[0]);
            native_t const data1_1(pData1[1]);
            native_t const data1_2(pData1[2]);
            native_t const data1_3(pData1[3]);
            native_t const data2_0(pData2[0]);
            native_t const data2_1(pData2[1]);
            native_t const data2_2(pData2[2]);
            native_t const data2_3(pData2[3]);

            *pData2++ = data1_0;
            *pData2++ = data1_1;
            *pData2++ = data1_2;
            *pData2++ = data1_3;
            *pData1++ = data2_0;
            *pData1++ = data2_1;
            *pData1++ = data2_2;
            *pData1++ = data2_3;
        }
    }
    else
    {
        //...mrmlj...MSVC10...:
        std::swap_ranges<vector_t::native_type *LE_RESTRICT>(
            &data1.begin()->data_, &data1.end()->data_, &data2.begin()->data_);
    }
#elif defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    swap(pBegin, pDestination, pEnd - pBegin);
#else
    std::swap_ranges<float *LE_RESTRICT>(pBegin, const_cast<float *LE_RESTRICT>(pEnd),
                                         pDestination);
#endif
}

float const &min(float const *const pBegin, float const *const pEnd)
{
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    return min(pBegin, static_cast<unsigned int>(pEnd - pBegin));
#else
    return *std::min_element(pBegin, pEnd);
#endif
}

#if !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
LE_NOINLINE
#endif
float const &max(float const *const pBegin, float const *const pEnd)
{
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    return max(pBegin, static_cast<unsigned int>(pEnd - pBegin));
#else
    return *std::max_element(pBegin, pEnd);
#endif
}

void add(float const *const pInputData, float *const pInputOutput, float const *const pOutputEnd)
{
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    add(pInputData, pInputOutput, static_cast<unsigned int>(pOutputEnd - pInputOutput));
#elif defined LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const outputRange(pInputOutput, pOutputEnd);
    vector_t const *LE_RESTRICT pInput(alignDown(pInputData));
    for (auto &pack : outputRange)
    {
        pack += *pInput++;
    }
#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    float const *LE_RESTRICT pInput(pInputData);
    float *LE_RESTRICT pOutput(pInputOutput);
    while (pOutput != pOutputEnd)
        *pOutput++ += *pInput++;
#endif // LE_MATH_USE_NT2
}

void add(float const *const pInputData, float const scalar, float *const pOutput,
         float const *const pOutputEnd)
{
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    add(pInputData, scalar, pOutput, static_cast<unsigned int>(pOutputEnd - pOutput));
#elif defined LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const outputRange(pOutput, pOutputEnd);
    LE_ASSERT_MSG(outputRange.compatiblyAligned(pInputData), "Misaligned data");
    vector_t const *LE_RESTRICT pInput(alignDown(pInputData));
    vector_t const constant(boost::simd::splat<vector_t>(scalar));
    for (auto &pack : outputRange)
    {
        pack = *pInput++ + constant;
    }
#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    float const *LE_RESTRICT pInput(pInputData);
    float *LE_RESTRICT pOutputValue(pOutput);
    while (pOutputValue != pOutputEnd)
        *pOutputValue++ = *pInput++ + scalar;
#endif // LE_MATH_USE_NT2
}

LE_NOINLINE_NT2 void multiply(float const *const pFirstArray, float const *const pSecondArray,
                              float *const pOutput, float const *const pOutputEnd)
{
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    multiply(pFirstArray, pSecondArray, pOutput, static_cast<unsigned int>(pOutputEnd - pOutput));
#elif defined LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const outputRange(pOutput, pOutputEnd);
    LE_ASSERT_MSG(outputRange.compatiblyAligned(pFirstArray), "Misaligned data");
    LE_ASSERT_MSG(outputRange.compatiblyAligned(pSecondArray), "Misaligned data");
    vector_t const *LE_RESTRICT pInput1(alignDown(pFirstArray));
    vector_t const *LE_RESTRICT pInput2(alignDown(pSecondArray));
    for (auto &pack : outputRange)
    {
        pack = *pInput1++ * *pInput2++;
    }
#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    float const *LE_RESTRICT pInput1(pFirstArray);
    float const *LE_RESTRICT pInput2(pSecondArray);
    float *LE_RESTRICT pOutputValue(pOutput);
    while (pOutputValue != pOutputEnd)
        *pOutputValue++ = *pInput1++ * *pInput2++;
#endif // LE_MATH_USE_NT2
}

LE_NOINLINE_NT2 void multiply(float const *const pInputData, float *const pInputOutput,
                              float const *const pOutputEnd)
{
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    multiply(pInputData, pInputOutput, static_cast<unsigned int>(pOutputEnd - pInputOutput));
#elif defined LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const outputRange(pInputOutput, pOutputEnd);
    LE_ASSERT_MSG(outputRange.compatiblyAligned(pInputData), "Misaligned data");
    vector_t const *LE_RESTRICT pInput(alignDown(pInputData));
    for (auto &pack : outputRange)
    {
        pack *= *pInput++;
    }
#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    float const *LE_RESTRICT pInput(pInputData);
    float *LE_RESTRICT pOutput(pInputOutput);
    while (pOutput != pOutputEnd)
        *pOutput++ *= *pInput++;
#endif // LE_MATH_USE_NT2
}

LE_NOINLINE_NT2 void multiply(float const scalar, float const *LE_RESTRICT const pInputData,
                              float *LE_RESTRICT const pOutput,
                              float const *LE_RESTRICT const pOutputEnd)
{
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    multiply(pInputData, scalar, pOutput, static_cast<unsigned int>(pOutputEnd - pOutput));
#elif defined LE_MATH_USE_NT2
    if (scalar == 0)
        return clear(pOutput, pOutputEnd);
    else if (scalar == 1)
        return copy(pInputData, pInputData + (pOutputEnd - pOutput), pOutput);

    EdgeRestoredAlignedRange const outputRange(pOutput, pOutputEnd);
    vector_t const constant(boost::simd::splat<vector_t>(scalar));

    if (outputRange.compatiblyAligned(pInputData))
    {
        vector_t const *LE_RESTRICT pInput(alignDown(pInputData));
        for (auto &pack : outputRange)
        {
            pack = constant * *pInput++;
        }
    }
    else
    {
        float const *LE_RESTRICT pInput(pInputData);
        for (auto &pack : outputRange)
        {
            pack = constant * boost::simd::load<vector_t>(pInput);
            pInput += vector_t::static_size;
        }
    }
#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    /// \note No scalar == 0 / scalar == 1 short circuits, unlike the NT2 arm
    /// above: vDSP_vsmul has none either, and the difference is observable on a
    /// non-finite input.
    float const *LE_RESTRICT pInput(pInputData);
    float *LE_RESTRICT pOutputValue(pOutput);
    while (pOutputValue != pOutputEnd)
        *pOutputValue++ = scalar * *pInput++;
#endif // LE_MATH_USE_NT2
}

LE_NOINLINE_NT2 void multiply(float const scalar, float *LE_RESTRICT const pInputOutput,
                              float const *LE_RESTRICT const pOutputEnd)
{
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    multiply(pInputOutput, scalar, static_cast<unsigned int>(pOutputEnd - pInputOutput));
#elif defined LE_MATH_USE_NT2
    if (scalar == 0)
        return clear(pInputOutput, pOutputEnd);
    else if (scalar == 1)
        return;

    EdgeRestoredAlignedRange const outputRange(pInputOutput, pOutputEnd);
    vector_t const constant(boost::simd::splat<vector_t>(scalar));
    for (auto &pack : outputRange)
    {
        pack *= constant;
    }
#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    float *LE_RESTRICT pOutput(pInputOutput);
    while (pOutput != pOutputEnd)
        *pOutput++ *= scalar;
#endif // LE_MATH_USE_NT2
}

LE_NOINLINE_NT2 void addProduct(float const *LE_RESTRICT const pInputData1,
                                float const *LE_RESTRICT const pInputData2,
                                float *LE_RESTRICT pInput3AndOutput,
                                float const *LE_RESTRICT const pOutputEnd)
{
#if defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    addProduct(pInputData1, pInputData2, pInput3AndOutput,
               static_cast<unsigned int>(pOutputEnd - pInput3AndOutput));
#elif defined LE_MATH_USE_NT2

    EdgeRestoredAlignedRange const outputRange(pInput3AndOutput, pOutputEnd);
    if (outputRange.compatiblyAligned(pInputData1) && outputRange.compatiblyAligned(pInputData2))
    {
        vector_t const *LE_RESTRICT pInput1(alignDown(pInputData1));
        vector_t const *LE_RESTRICT pInput2(alignDown(pInputData2));
        for (auto &pack : outputRange)
        {
            pack = nt2::fma(*pInput1++, *pInput2++, pack);
        }
    }
    else
    {
        /// \note
        ///   The misaligned path becomes required when the host is set to
        /// process data in non-power-of-two sized chunks.
        ///                                   (25.01.2012.) (Domagoj Saric)
        LE_ASSERT_MSG(boost::simd::is_aligned(pInputData1), "Misaligned data");
        LE_ASSERT_MSG(boost::simd::is_aligned(pInputData2), "Misaligned data");
        auto const *LE_RESTRICT pInput1(reinterpret_cast<vector_t const *>(pInputData1));
        auto const *LE_RESTRICT pInput2(reinterpret_cast<vector_t const *>(pInputData2));

        while (pInput3AndOutput != pOutputEnd)
        {
            vector_t const input3(boost::simd::load<vector_t>(pInput3AndOutput));
            vector_t const output(nt2::fma(*pInput1++, *pInput2++, input3));
            boost::simd::store<vector_t>(output, pInput3AndOutput, 0);
            pInput3AndOutput += vector_t::static_size;
        }
    }

#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    float const *LE_RESTRICT pInput1(pInputData1);
    float const *LE_RESTRICT pInput2(pInputData2);
    while (pInput3AndOutput != pOutputEnd)
        *pInput3AndOutput++ += *pInput1++ * *pInput2++;
#endif // LE_MATH_USE_NT2
}

void rectangular2polar(float const *LE_RESTRICT const pReals, float const *LE_RESTRICT const pImags,
                       float *LE_RESTRICT const pAmplitudes, float *LE_RESTRICT const pPhases,
                       float const *const pPhasesEnd)
{
    rectangular2polar(pReals, pImags, pAmplitudes, pPhases,
                      static_cast<std::uint16_t>(pPhasesEnd - pPhases));
}

void ln(float *LE_RESTRICT const pInputOutput, float const *LE_RESTRICT const pOutputEnd)
{
#ifdef LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const outputRange(pInputOutput, pOutputEnd);
    for (auto &pack : outputRange)
    {
        pack = nt2::log(pack);
    }
#elif defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    ln(pInputOutput, static_cast<unsigned int>(pOutputEnd - pInputOutput));
#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    float *LE_RESTRICT pOutput(pInputOutput);
    while (pOutput != pOutputEnd)
    {
        *pOutput = std::log(*pOutput);
        ++pOutput;
    }
#endif // LE_MATH_USE_NT2
}

void ln(float const *LE_RESTRICT const pInput, float *LE_RESTRICT const pOutput,
        float const *LE_RESTRICT const pOutputEnd)
{
#ifdef LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const outputRange(pOutput, pOutputEnd);
    LE_ASSERT_MSG(outputRange.compatiblyAligned(pInput), "Misaligned data");
    vector_t const *LE_RESTRICT pInputPack(alignDown(pInput));
    for (auto &pack : outputRange)
    {
        pack = nt2::log(*pInputPack++);
    }
#elif defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    ln(pInput, pOutput, static_cast<unsigned int>(pOutputEnd - pOutput));
#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    float const *LE_RESTRICT pInputValue(pInput);
    float *LE_RESTRICT pOutputValue(pOutput);
    while (pOutputValue != pOutputEnd)
        *pOutputValue++ = std::log(*pInputValue++);
#endif // LE_MATH_USE_NT2
}

void exp(float *LE_RESTRICT const pInputOutput, float const *LE_RESTRICT const pOutputEnd)
{
#ifdef LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const outputRange(pInputOutput, pOutputEnd);
    for (auto &pack : outputRange)
    {
        pack = nt2::exp(pack);
    }
#elif defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    exp(pInputOutput, static_cast<unsigned int>(pOutputEnd - pInputOutput));
#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    float *LE_RESTRICT pOutput(pInputOutput);
    while (pOutput != pOutputEnd)
    {
        *pOutput = std::exp(*pOutput);
        ++pOutput;
    }
#endif // LE_MATH_USE_NT2
}

void square(float *const pInputOutput, float const *const pOutputEnd)
{
#ifdef LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const outputRange(pInputOutput, pOutputEnd);
    for (auto &pack : outputRange)
    {
        pack = nt2::sqr(pack);
    }
#elif defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    square(pInputOutput, static_cast<unsigned int>(pOutputEnd - pInputOutput));
#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    float *LE_RESTRICT pOutput(pInputOutput);
    while (pOutput != pOutputEnd)
    {
        float const value(*pOutput);
        *pOutput++ = value * value;
    }
#endif // LE_MATH_USE_NT2
}

void squareRoot(float *const pInputOutput, float const *const pOutputEnd)
{
#ifdef LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const outputRange(pInputOutput, pOutputEnd);
    for (auto &pack : outputRange)
    {
        pack = nt2::sqrt(pack);
    }
#elif defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    squareRoot(pInputOutput, static_cast<unsigned int>(pOutputEnd - pInputOutput));
#else
    /// \note The portable arm, replacing what was an unreachable stub: with
    /// neither Accelerate nor NT2 these had no implementation at all in
    /// either interface -- the pointer-pair form fell through to here and the
    /// count form delegated straight back to it. Elementwise and in the
    /// caller's order, so both GCC and Clang vectorise them at -O2 and above
    /// without needing reassociation, and the rounding is the scalar loop's.
    ///                                   (29.07.2026.) (SW port)
    float *LE_RESTRICT pOutput(pInputOutput);
    while (pOutput != pOutputEnd)
    {
        *pOutput = std::sqrt(*pOutput);
        ++pOutput;
    }
#endif // LE_MATH_USE_NT2
}

float LE_NOINLINE_NT2 rms(float const *const pData, float const *const pDataEnd)
{
#if defined(LE_MATH_USE_ACC)
    return rms(pData, static_cast<unsigned int>(pDataEnd - pData));
#else
    float const *LE_RESTRICT pValue(pData);
    float result(0);
    while (pValue != pDataEnd)
    {
        result += *pValue * *pValue;
        ++pValue;
    }
    result = std::sqrt(result / convert<float>(pDataEnd - pData));
    return result;
#endif
}

void LE_NOINLINE
mix(float const *LE_RESTRICT pInput1, float const *LE_RESTRICT pInput2,
    float *LE_RESTRICT pOutput, // restrict allows two pointers to point to the exact same object
    float const *LE_RESTRICT const pOutputEnd, float const input1Weight)
{
    // Blending formula:
    //
    // out = x * carrier + ( 1 - x ) * blender
    // out = x * carrier + blender - x * blender
    // out = x * ( carrier - blender ) + blender
    //
    // x = 1 => out = carrier
    // x = 0 => out = blender

    while (pOutput < pOutputEnd)
    {
        float const input1(*pInput1++);
        float const input2(*pInput2++);
        *pOutput++ = input1Weight * (input1 - input2) + input2;
    }
}

void LE_NOINLINE
mix(float const *LE_RESTRICT pInput1, float const *LE_RESTRICT pInput2,
    float *LE_RESTRICT pOutput, // restrict allows two pointers to point to the exact same object
    float const *LE_RESTRICT const pOutputEnd, float const input1Weight, float const input2Weight)
{
    while (pOutput < pOutputEnd)
    {
        float const input1(*pInput1++);
        float const input2(*pInput2++);
        *pOutput++ = input1 * input1Weight + input2 * input2Weight;
    }
}

////////////////////////////////////////////////////////////////////////////////
/// "Pointers + size" based interfaces.
////////////////////////////////////////////////////////////////////////////////

void copy(float const *LE_RESTRICT const pInput, float *LE_RESTRICT const pOutput,
          unsigned int const numberOfElements)
{
    LE_ASSERT_MSG((pOutput >= pInput + numberOfElements) || (pInput >= pOutput + numberOfElements),
                  "Buffer overlap."); // Use move for overlapping ranges.
    std::memcpy(pOutput, pInput, numberOfElements * sizeof(*pInput));
}

void move(float const *const pInput, float *const pOutput, unsigned int const numberOfElements)
{
    //...mrmlj...vDSP_mmov seems slower/non-vectorized
    std::memmove(pOutput, pInput, numberOfElements * sizeof(*pInput));
}

void clear(float *const pArray, unsigned int const numberOfElements)
{
    //...mrmlj...vDSP_vclr seems slower/non-vectorized
    std::memset(pArray, 0, numberOfElements * sizeof(*pArray));
}

void fill(float *const pArray, float const value, unsigned int const numberOfElements)
{
#ifdef LE_MATH_USE_ACC
    vDSP_vfill(const_cast<float *>(&value), pArray, 1, numberOfElements);
#else
    fill(pArray, pArray + numberOfElements, value);
#endif
}

void negate(float *const pArray, unsigned int const numberOfElements)
{
#ifdef LE_MATH_USE_ACC
    negate(pArray, 1, numberOfElements);
#else
    negate(pArray, pArray + numberOfElements);
#endif
}

void negate(float *const pArray, unsigned int const stride, unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    vDSP_vneg(pArray, stride, pArray, stride, numberOfElements);
#else
    negate(InputOutputRange(pArray, pArray + numberOfElements), stride);
#endif
}

void reverse(float *const pArray, unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC) &&                                                                    \
    /* ...mrmlj...vDSP_vrvrs seems slower/non-vectorized */ !defined(LE_MATH_USE_NT2)
    vDSP_vrvrs(pArray, 1, numberOfElements);
#else
    reverse(pArray, pArray + numberOfElements);
#endif
}

void swap(float *const pFirstArray, float *const pSecondArray, unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC) &&                                                                    \
    /* ...mrmlj...vDSP_vswap seems slower/non-vectorized */ !defined(LE_MATH_USE_NT2)
    vDSP_vswap(pFirstArray, 1, pSecondArray, 1, numberOfElements);
#else
    swap(pFirstArray, pFirstArray + numberOfElements, pSecondArray);
#endif
}

float const &min(float const *const pArray, unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    float result;
    vDSP_Length resultIndex;
    vDSP_minvi(const_cast<float *>(pArray), 1, &result, &resultIndex, numberOfElements);
    LE_ASSERT(pArray[resultIndex] == result);
    return pArray[resultIndex];
#else
    return min(pArray, pArray + numberOfElements);
#endif
}

float const &max(float const *const pArray, unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    float result;
    vDSP_Length resultIndex;
    vDSP_maxvi(const_cast<float *>(pArray), 1, &result, &resultIndex, numberOfElements);
    LE_ASSERT(pArray[resultIndex] == result);
    return pArray[resultIndex];
#else
    return max(pArray, pArray + numberOfElements);
#endif
}

void add(float const *const pInput, float *const pInputOutput, unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    vDSP_vadd(pInput, 1, pInputOutput, 1, pInputOutput, 1, numberOfElements);
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    add(pInput, pInputOutput, pInputOutput + numberOfElements);
#endif // LE_MATH_USE_ACC
}

void add(float const *const pInput, float const constant, float *const pOutput,
         unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    vDSP_vsadd(const_cast<float *>(pInput), 1, const_cast<float *>(&constant), pOutput, 1,
               numberOfElements);
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    add(pInput, constant, pOutput, pOutput + numberOfElements);
#endif // LE_MATH_USE_ACC
}

void multiply(float const *const pFirstArray, float const *const pSecondArray, float *const pOutput,
              unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    vDSP_vmul(pFirstArray, 1, pSecondArray, 1, pOutput, 1, numberOfElements);
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    multiply(pFirstArray, pSecondArray, pOutput, pOutput + numberOfElements);
#endif // LE_MATH_USE_ACC
}

void multiply(float const *const pInput, float *const pInputOutput,
              unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    multiply(pInput, pInputOutput, pInputOutput, numberOfElements);
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    multiply(pInput, pInputOutput, pInputOutput + numberOfElements);
#endif // LE_MATH_USE_ACC
}

void multiply(float const *const pInput, float const scalar, float *const pOutput,
              unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    vDSP_vsmul(pInput, 1, &scalar, pOutput, 1, numberOfElements);
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    multiply(scalar, pInput, pOutput, pOutput + numberOfElements);
#endif // LE_MATH_USE_ACC
}

void multiply(float *const pInputOutput, float const scalar, unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    multiply(pInputOutput, scalar, pInputOutput, numberOfElements);
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    multiply(scalar, pInputOutput, pInputOutput + numberOfElements);
#endif // LE_MATH_USE_ACC
}

void addProduct(float const *const pInput1, float const *const pInput2,
                float *const pInput3AndOutput, unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    vDSP_vma(const_cast<float *>(pInput1), 1, const_cast<float *>(pInput2), 1, pInput3AndOutput, 1,
             pInput3AndOutput, 1, numberOfElements);
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    addProduct(pInput1, pInput2, pInput3AndOutput, pInput3AndOutput + numberOfElements);
#endif // LE_MATH_USE_ACC
}

void rectangular2polar(float const *LE_RESTRICT const pReals, float const *LE_RESTRICT const pImags,
                       float *LE_RESTRICT const pAmplitudes, float *LE_RESTRICT const pPhases,
                       std::uint16_t const numberOfElements)
{
#ifdef LE_MATH_USE_NT2
    // Implementation note:
    //   It frequently happens that a real component is zero which causes a
    // division-by-zero FPU exception in the atan2 function so we have to
    // locally mask them.
    //                                        (13.10.2011.) (Domagoj Saric)
    LE_LOCALLY_DISABLE_FPU_EXCEPTIONS();

    EdgeRestoredAlignedRange const amplitudes(pAmplitudes, pAmplitudes + numberOfElements);
    EdgeRestoredAlignedRange const phases(pPhases, pPhases + numberOfElements);
    LE_ASSERT_MSG(amplitudes.compatiblyAligned(pPhases), "Misaligned data");
    LE_ASSERT_MSG(amplitudes.compatiblyAligned(pReals), "Misaligned data");
    LE_ASSERT_MSG(amplitudes.compatiblyAligned(pImags), "Misaligned data");

    complex_op_vector_t const *LE_RESTRICT pReal(asComplex(alignDown(pReals)));
    complex_op_vector_t const *LE_RESTRICT pImag(asComplex(alignDown(pImags)));
    complex_op_vector_t *LE_RESTRICT pAmp(asComplex(amplitudes.begin()));
    complex_op_vector_t *LE_RESTRICT pPhase(asComplex(phases.begin()));
    auto counter(adjustVectorCounter(amplitudes.size()));
    while (counter--)
    {
        complex_op_vector_t const &reals(*pReal++);
        complex_op_vector_t const &imags(*pImag++);
        *pAmp++ = boost::simd::fast_hypot(reals, imags);
        *pPhase++ = nt2 ::nbd_atan2(imags, reals);
    }
#elif defined(LE_MATH_USE_ACC)
    DSPSplitComplex data = {const_cast<float *>(pReals), const_cast<float *>(pImags)};
    vDSP_zvabs(&data, 1, pAmplitudes, 1, numberOfElements);
//...mrmlj...simulator...
#if LE_ACC_NO_VFORCE
    vDSP_zvphas(&data, 1, pPhases, 1, numberOfElements);
#else
    int const vDSPNumberOfElements(numberOfElements);
    vvatan2f(pPhases, pImags, pReals, &vDSPNumberOfElements);
#endif // LE_ACC_NO_VFORCE
#if TARGET_OS_IPHONE && !defined(NDEBUG)
    // Implementation note:
    //   The iOS implementation of vDSP_zvphas returns NaNs for inputs
    // with zero reals. This causes a chain of assertion failures but
    // the output still sounds fine. For this reason we zero the NaNs in
    // development builds.
    //                                    (28.11.2011.) (Domagoj Saric)
    for (float &phase : LE::Utility::makeSpan(pPhases, numberOfElements))
    {
        if (!std::isfinite(phase))
            phase = 0;
    }
#endif // TARGET_OS_IPHONE
#else
    for (std::uint16_t element(0); element < numberOfElements; ++element)
    {
        float const real(pReals[element]);
        float const imag(pImags[element]);
        pAmplitudes[element] = std::hypot(real, imag);
        pPhases[element] = std::atan2(imag, real);
    }
#endif // LE_MATH_USE_NT2

    LE_MATH_VERIFY_VALUES(
        Math::InvalidOrSlow | Negative,
        LE::Utility::Span<float const>(pAmplitudes, pAmplitudes + numberOfElements), "amplitudes");
    LE_MATH_VERIFY_VALUES(Math::InvalidOrSlow,
                          LE::Utility::Span<float const>(pPhases, pPhases + numberOfElements),
                          "phases");
}

void amplitudes(float const *LE_RESTRICT const pReals, float const *LE_RESTRICT const pImags,
                float *LE_RESTRICT const pAmplitudes, float const *LE_RESTRICT const pAmplitudesEnd)
{
#ifdef LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const amplitudes(pAmplitudes, pAmplitudesEnd);
    LE_ASSERT_MSG(amplitudes.compatiblyAligned(pReals), "Misaligned data");
    LE_ASSERT_MSG(amplitudes.compatiblyAligned(pImags), "Misaligned data");

    complex_op_vector_t const *LE_RESTRICT pReal(asComplex(alignDown(pReals)));
    complex_op_vector_t const *LE_RESTRICT pImag(asComplex(alignDown(pImags)));
    complex_op_vector_t *LE_RESTRICT pAmp(asComplex(amplitudes.begin()));
    auto counter(adjustVectorCounter(amplitudes.size()));
    while (counter--)
    {
        complex_op_vector_t const &reals(*pReal++);
        complex_op_vector_t const &imags(*pImag++);
        *pAmp++ = boost::simd::fast_hypot(reals, imags);
    }
#elif defined(LE_MATH_USE_ACC)
    DSPSplitComplex data = {const_cast<float *>(pReals), const_cast<float *>(pImags)};
    vDSP_zvabs(&data, 1, pAmplitudes, 1, pAmplitudesEnd - pAmplitudes);
#else
    for (auto index(pAmplitudesEnd - pAmplitudes); index--;)
    {
        pAmplitudes[index] = std::hypot(pReals[index], pImags[index]);
    }
#endif // LE_MATH_USE_NT2

    LE_MATH_VERIFY_VALUES(Math::InvalidOrSlow | Negative,
                          LE::Utility::Span<float const>(pAmplitudes, pAmplitudesEnd),
                          "amplitudes");
}

#ifdef LE_MATH_USE_NT2
namespace
{
#pragma warning(push)
#pragma warning(disable : 4510) // Default constructor could not be generated.
#pragma warning(disable : 4512) // Assignment operator could not be generated.
#pragma warning(disable                                                                            \
                : 4610)      // Class can never be instantiated - user-defined constructor required.
struct Polar2rectangularData // tiny x86 register file workaround
{
    FastIndexedPointer<vector_t const> const pAmps;
    FastIndexedPointer<vector_t const> const pPhases;
    FastIndexedPointer<vector_t> const pReals;
    FastIndexedPointer<vector_t> const pImags;
};
#pragma warning(pop)

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wassume"
#endif // __clang__
void LE_HOT polar2rectangular(Polar2rectangularData const *LE_RESTRICT const pData,
                              std::uint16_t const size)
{
    using index_t = FastIndexedPointer<vector_t>::index_type;
#if defined(BOOST_SIMD_ARCH_X86) && !defined(BOOST_SIMD_ARCH_X86_64)
    //...mrmlj...x86 GP register starvation...
    index_t pack(size - 1);
    index_t const endPack(index_t(0) -= 1);
    std::int8_t const increment(-1);
#else
    index_t pack(0);
    index_t const endPack(size);
    std::uint8_t const increment(+1);
#endif // arch
    LE_ASSUME(pack != endPack);
    do
    {
        vector_t *LE_RESTRICT const pReal(&pData->pReals[pack]);
        vector_t *LE_RESTRICT const pImag(&pData->pImags[pack]);
        *pImag = nt2::sinecosine<nt2::small_>(pData->pPhases[pack], *pReal);
        vector_t const *LE_RESTRICT const pAmps(&pData->pAmps[pack]);
        *pReal *= *pAmps;
        *pImag *= *pAmps;

        pack += increment;
    } while (pack != endPack);
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__
} // anonymous namespace
#endif // LE_MATH_USE_NT2

void polar2rectangular(float const *LE_RESTRICT const pAmplitudes,
                       float const *LE_RESTRICT const pPhases, float *LE_RESTRICT const pReals,
                       float *LE_RESTRICT const pImags, std::uint16_t const numberOfElements)
{
#ifdef LE_MATH_USE_NT2
    EdgeRestoredAlignedRange const reals(pReals, pReals + numberOfElements);
    EdgeRestoredAlignedRange const imags(pImags, pImags + numberOfElements);
    LE_ASSERT_MSG(reals.compatiblyAligned(pImags), "Misaligned data");
    LE_ASSERT_MSG(reals.compatiblyAligned(pAmplitudes), "Misaligned data");
    LE_ASSERT_MSG(reals.compatiblyAligned(pPhases), "Misaligned data");

    Polar2rectangularData const data = {alignDown(pAmplitudes), alignDown(pPhases), reals.begin(),
                                        imags.begin()};
    polar2rectangular(&data, reals.size());
#elif defined(LE_MATH_USE_ACC) && !LE_ACC_NO_VFORCE
    int const vDSPNumberOfElements(numberOfElements);
    vvsincosf(pImags, pReals, pPhases, &vDSPNumberOfElements);
    multiply(pAmplitudes, pReals, numberOfElements);
    multiply(pAmplitudes, pImags, numberOfElements);
#else
    {
        float const *LE_RESTRICT pPhase(pPhases);
        float *LE_RESTRICT pCosines(pReals);
        float *LE_RESTRICT pSines(pImags);
        auto counter(numberOfElements);
        while (counter--)
        {
            float const phase(*pPhase++);
            *pSines++ = std::sin(phase);
            *pCosines++ = std::cos(phase);
        }
    }
    multiply(pAmplitudes, pReals, numberOfElements);
    multiply(pAmplitudes, pImags, numberOfElements);
#endif // Math impl
}

void ln(float const *LE_RESTRICT pInput, float *LE_RESTRICT pOutput,
        unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
#if LE_ACC_NO_VFORCE
    float const *pInputValue(pInput);
    float *pOutputValue(pOutput);
    float const *const pEnd(pOutput + numberOfElements);
    while (pOutputValue != pEnd)
    {
        *pOutputValue++ = nt2::log(*pInputValue++);
    }
#else
    ::vvlogf(pOutput, pInput, &reinterpret_cast<int const &>(numberOfElements));
#endif
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    ln(pInput, pOutput, pOutput + numberOfElements);
#else
    LE_UNREACHABLE_CODE();
#endif
}

void ln(float *pInputOutput, unsigned int numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
#if LE_ACC_NO_VFORCE
    while (numberOfElements--)
    {
        *pInputOutput = nt2::log(*pInputOutput);
        ++pInputOutput;
    }
#else
    ::vvlogf(pInputOutput, pInputOutput, &reinterpret_cast<int const &>(numberOfElements));
#endif
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    ln(pInputOutput, pInputOutput + numberOfElements);
#endif // LE_MATH_USE_ACC
}

void exp(float *pInputOutput, unsigned int numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
#if LE_ACC_NO_VFORCE
    while (numberOfElements--)
    {
        *pInputOutput = nt2::exp(*pInputOutput);
        ++pInputOutput;
    }
#else
    ::vvexpf(pInputOutput, pInputOutput, &reinterpret_cast<int const &>(numberOfElements));
#endif
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    exp(pInputOutput, pInputOutput + numberOfElements);
#endif // LE_MATH_USE_ACC
}

void square(float *LE_RESTRICT pInputOutput, unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    vDSP_vsq(pInputOutput, 1, pInputOutput, 1, numberOfElements);
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    square(pInputOutput, pInputOutput + numberOfElements);
#else
    while (numberOfElements--)
    {
        float const inputValue(*pInputOutput);
        *pInputOutput++ = inputValue * inputValue;
    }
#endif // math impl
}

void squareRoot(float *const pInputOutput, unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
#if LE_ACC_NO_VFORCE
    float *pInputOutputValue(pInputOutput);
    float const *const pEnd(pInputOutput + numberOfElements);
    while (pInputOutputValue != pEnd)
    {
        *pInputOutputValue = /*std*/ ::sqrtf(*pInputOutputValue);
        ++pInputOutputValue;
    }
#else
    vvsqrtf(pInputOutput, pInputOutput, &reinterpret_cast<int const &>(numberOfElements));
#endif
#elif !defined(LE_MATH_NATIVE_POINTER_SIZE_INTERFACE)
    squareRoot(pInputOutput, pInputOutput + numberOfElements);
#endif // LE_MATH_USE_ACC
}

float rms(float const *const pData, unsigned int const numberOfElements)
{
#if defined(LE_MATH_USE_ACC)
    float result;
    vDSP_rmsqv(const_cast<float *>(pData), 1, &result, numberOfElements);
    return result;
#else
    return rms(pData, pData + numberOfElements);
#endif // LE_MATH_USE_ACC
}

void mix(float const *const pInput1, float const *const pInput2, float *const pOutput,
         float const input1Weight, unsigned int const numberOfElements)
{
    mix(pInput1, pInput2, pOutput, pOutput + numberOfElements, input1Weight);
}

void interleave(float const *LE_RESTRICT const *LE_RESTRICT const pInputs,
                float *LE_RESTRICT pOutput, std::uint16_t const numberOfElements,
                std::uint8_t const numberOfChannels)
{
    std::uint16_t element(0);
    switch (numberOfChannels)
    {
    case 1:
        LE_UNREACHABLE_CODE();

#ifdef LE_MATH_USE_NT2
    case 2:
    {
        EdgeRestoredAlignedRange const output(pOutput, pOutput + (numberOfElements * 2));
        if (output.compatiblyAligned(pInputs[0]) && output.compatiblyAligned(pInputs[1]))
        {
            vector_t const *LE_RESTRICT const pChannel0(alignDown(pInputs[0]));
            vector_t const *LE_RESTRICT const pChannel1(alignDown(pInputs[1]));
            std::uint16_t const vectorizedSize((output.size() / 2) * 2);
            element += vectorizedSize * vector_t::static_size / numberOfChannels;
            pOutput += vectorizedSize * vector_t::static_size / numberOfChannels;
            boost::simd::details::interleave_two_channels(pChannel0, pChannel1, output.begin(),
                                                          vectorizedSize);
        }
        // intentional fall through for tail or misaligned data
    }
#endif // LE_MATH_USE_NT2

    default:
        LE_DISABLE_LOOP_UNROLLING()
        for (; element < numberOfElements; ++element)
        {
            LE_DISABLE_LOOP_UNROLLING()
            for (std::uint8_t channel(0); channel < numberOfChannels; ++channel)
            {
                *pOutput++ = pInputs[channel][element];
            }
        }
    }
}

void deinterleave(float const *LE_RESTRICT pInput,
                  float *LE_RESTRICT const *LE_RESTRICT const pOutputs,
                  std::uint16_t const numberOfElements, std::uint8_t const numberOfChannels)
{
    std::uint16_t element(0);
    switch (numberOfChannels)
    {
    case 1:
        LE_UNREACHABLE_CODE();

#ifdef LE_MATH_USE_NT2
    case 2:
    {
        EdgeRestoredAlignedRange const channel0(pOutputs[0], pOutputs[0] + numberOfElements);
        EdgeRestoredAlignedRange const channel1(pOutputs[1], pOutputs[1] + numberOfElements);
        vector_t const *LE_RESTRICT const pInterleavedInput(alignDown(pInput));
        if (channel0.compatiblyAligned(pOutputs[1]) && channel0.compatiblyAligned(pInput))
        {
            std::uint16_t const vectorizedSize(channel0.size() * 2);
            element += vectorizedSize * vector_t::static_size / numberOfChannels;
            pInput += vectorizedSize * vector_t::static_size / numberOfChannels;
            boost::simd::details::deinterleave_two_channels(pInterleavedInput, channel0.begin(),
                                                            channel1.begin(), vectorizedSize);
        }
        // intentional fall through for tail or misaligned data
    }
#endif // LE_MATH_USE_NT2

    default:
        LE_DISABLE_LOOP_UNROLLING()
        for (; element < numberOfElements; ++element)
        {
            LE_DISABLE_LOOP_UNROLLING()
            for (std::uint8_t channel(0); channel < numberOfChannels; ++channel)
            {
                pOutputs[channel][element] = *pInput++;
            }
        }
    }
}

//------------------------------------------------------------------------------
} // namespace Math
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// Scalar NT2 replacements of CRT functions (for math.cpp and conversion.cpp)
/// implemented here to minimise the NT2 inclusion compile time hit
////////////////////////////////////////////////////////////////////////////////
/// \note Xcode 6.3 & 7 Clang miscompiled nt2::exp2 (e.g. pshifter had no
/// effect), so Apple always took the CRT. Without LE_HAS_NT2 everyone does, and
/// then math.cpp and conversion.cpp own these — see their matching guards.
///                                       (28.07.2026.) (SW port)
#if defined(LE_HAS_NT2) && !defined(__APPLE__)
#define LE_MATH_SCALAR_NT2 1
#endif

#if LE_MATH_SCALAR_NT2
#include "nt2/exponential/include/functions/scalar/exp.hpp"
#include "nt2/exponential/include/functions/scalar/exp2.hpp"
#include "nt2/exponential/include/functions/scalar/log.hpp"
#include "nt2/exponential/include/functions/scalar/log2.hpp"
#include "nt2/exponential/include/functions/scalar/log10.hpp"
#include "nt2/signal/include/functions/scalar/db2mag.hpp"
#include "nt2/signal/include/functions/scalar/db2pow.hpp"
#include "nt2/signal/include/functions/scalar/mag2db.hpp"
#include "nt2/signal/include/functions/scalar/pow2db.hpp"

//...mrmlj...broken nt2 headers/includes...
#include "nt2/signal/include/functions/db2mag.hpp"
#include "nt2/signal/include/functions/db2pow.hpp"
#include "nt2/signal/include/functions/mag2db.hpp"
#include "nt2/signal/include/functions/pow2db.hpp"
#endif // LE_MATH_SCALAR_NT2
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Math
{
//------------------------------------------------------------------------------

#if LE_MATH_SCALAR_NT2

// https://github.com/MetaScale/nt2/issues/374 Fast pow2 and log2 implementations
float ln(float const value) { return nt2::log(value); }
float log2(float const value) { return nt2::log2(value); } //::__builtin_log2f
float log10(float const value) { return nt2::log10(value); }
float exp(float const value) { return nt2::exp(value); }
float exp2(float const value) { return nt2::exp2(value); }

#if !defined(__APPLE__) /*Denoiser assertion failures!?*/
float dB2NormalisedLinear(float const dBValue) { return nt2::db2mag(dBValue); }
float dB2NormalisedLinear(std::int8_t const dBValue)
{
    return /*nt2::db2mag( dBValue );*/ dB2NormalisedLinear(static_cast<float>(dBValue));
}
float dB2NormalisedLinear(std::uint8_t const dBValue)
{
    return /*nt2::db2mag( dBValue );*/ dB2NormalisedLinear(static_cast<std::int8_t>(dBValue));
}
float dB2NormalisedLinear(std::int16_t const dBValue)
{
    return /*nt2::db2mag( dBValue );*/ dB2NormalisedLinear(static_cast<std::int8_t>(dBValue));
}
float dB2NormalisedLinear(std::uint16_t const dBValue)
{
    return /*nt2::db2mag( dBValue );*/ dB2NormalisedLinear(static_cast<std::int8_t>(dBValue));
}
float normalisedLinear2dB(float const linearNormalisedValue)
{
    return nt2::mag2db(linearNormalisedValue);
}
double normalisedLinear2dB(double const linearNormalisedValue)
{
    return nt2::mag2db(linearNormalisedValue);
}
float normalisedPower2dB(float const linearPowerValue) { return nt2::pow2db(linearPowerValue); }
float dB2NormalisedPower(float const dBValue)
{
    return nt2::db2pow(dBValue / 5);
} // https://github.com/jfalcou/nt2/issues/983
#endif // !__APPLE__!?

#endif // LE_MATH_SCALAR_NT2

void addPolar(float const amp1, float const phase1, float &LE_GNU_SPECIFIC(__restrict) amp2,
              float &LE_GNU_SPECIFIC(__restrict) phase2)
{
#if LE_MATH_SCALAR_NT2
    float real1;
    float imag1(nt2::sinecosine<nt2::small_>(phase1, real1));
    float real2;
    float imag2(nt2::sinecosine<nt2::small_>(phase2, real2));
#else
    float real1(std::cos(phase1));
    float imag1(std::sin(phase1));
    float real2(std::cos(phase2));
    float imag2(std::sin(phase2));
#endif // LE_MATH_SCALAR_NT2

    real1 *= amp1;
    imag1 *= amp1;
    real2 *= amp2;
    imag2 *= amp2;

    auto const real(real1 + real2);
    auto const imag(imag1 + imag2);

#if LE_MATH_SCALAR_NT2
    amp2 = boost::simd::fast_hypot(real, imag);
    phase2 = nt2 ::nbd_atan2(imag, real);
#else
    amp2 = std::hypot(real, imag);
    phase2 = std::atan2(imag, real);
#endif // LE_MATH_SCALAR_NT2
}

//------------------------------------------------------------------------------
} // namespace Math
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------

LE_OPTIMIZE_FOR_SPEED_END()
