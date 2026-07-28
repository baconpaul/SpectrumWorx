////////////////////////////////////////////////////////////////////////////////
///
/// \file fft.hpp
/// -------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef fft_hpp__3EFDE32C_A81E_4A2B_9BDB_252EC2DD74ED
#define fft_hpp__3EFDE32C_A81E_4A2B_9BDB_252EC2DD74ED
//------------------------------------------------------------------------------
#include "le/spectrumworx/engine/buffers.hpp"

#if defined(__APPLE__)
#define LE_ACC_FFT
#else
//#define LE_PURE_REAL_FFT_TEST
//#define LE_SORENSEN_PURE_REAL_FFT_TEST
#endif

#ifdef LE_ACC_FFT
typedef struct OpaqueFFTSetup *FFTSetup;
typedef unsigned long vDSP_Length;
struct DSPSplitComplex;
#endif // LE_ACC_FFT

#include <cstdint>
#include "le/utility/span.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
LE_IMPL_NAMESPACE_BEGIN(Math)
//------------------------------------------------------------------------------

//...mrmlj...cleanup these duplicated typedefs (also in effects.hpp)...
typedef LE::Utility::Span<float> DataRange;
typedef LE::Utility::Span<float const> ReadOnlyDataRange;

////////////////////////////////////////////////////////////////////////////////
///
/// \class FFT_float_real_1D
///
/// \brief Performs single precision 1D forward and inverse FFT on real data.
///
////////////////////////////////////////////////////////////////////////////////
// Implementation note:
//   Check revision 5921 for the last version containing the ACML based
// implementation. Other alternative implementations can be found in the SVN
// revision 746 of this module.
//                                            (24.02.2012.) (Domagoj Saric)
////////////////////////////////////////////////////////////////////////////////

class FFT_float_real_1D
{
  public:
    FFT_float_real_1D();
#ifdef LE_ACC_FFT
    ~FFT_float_real_1D();
#endif // __APPLE__

    // real
    void transform(float *data /*inplace: in time     , out DFT reals*/,
                   DataRange const &imaginaryTargetSubRange, bool doFFTShift) const;
    void inverseTransform(float *data /*inplace: in DFT reals, out time     */,
                          ReadOnlyDataRange const &imaginarySourceSubRange, bool doFFTShift) const;

    void transform(float *data /*inplace: in time     , out DFT reals*/,
                   float *imaginaryTargetSubRange, std::uint16_t size) const;
    void inverseTransform(float *data /*inplace: in DFT reals, out time     */,
                          float const *imaginarySourceSubRange, std::uint16_t size) const;

    // complex
    void transform(float *pReals, float *pImags) const;
    void inverseTransform(float *pReals, float *pImags) const;

#ifdef LE_PURE_REAL_FFT_TEST
    void transform(float const *pTimeDomainData, float const *pWindow, float *pReals,
                   DataRange const &imags) const;
    void inverseTransform(float *pTimeDomainData, float const *pReals,
                          ReadOnlyDataRange const &imags) const;
    void inverseTransform(DataRange const &reals, DataRange const &imags) const;
#endif // LE_PURE_REAL_FFT_TEST

    void resize(SW::Engine::StorageFactors const &factors, SW::Engine::Storage &);

    std::uint16_t size() const { return size_; } //...mrmlj...actually "maximum allowed size"...

    static float maximumAmplitude(float size);

  private:
#if defined(LE_ACC_FFT)
    ::DSPSplitComplex &workBufferSplit() const;
#endif // LE_ACC_FFT

    void fftshift(float *pTimeDomainData) const;

  private:
    std::uint16_t size_;

#if defined(LE_ACC_FFT)
    FFTSetup fftSetup_;

    struct DSPSplitComplex
    {
        float *realp;
        float *imagp;
    };
    DSPSplitComplex workBufferSplit_;
    typedef SW::Engine::DoubleFFTBuffer<> WorkBuffer;
#else  // NT2
    typedef SW::Engine::SharedStorageFFTBasedBuffer<
        SW::Engine::real_t, 1, 1, Utility::Constants::vectorAlignment / sizeof(SW::Engine::real_t)>
        WorkBuffer;
#endif // FFT implementation

    mutable WorkBuffer workBuffer_;

  public:
    static std::uint32_t requiredStorage(SW::Engine::StorageFactors const &);
}; // class FFT_float_real_1D

//------------------------------------------------------------------------------
LE_IMPL_NAMESPACE_END(Math)
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // fft_hpp
