#pragma once

#include <cstdint>

// Define bool, true, and false for C++ compilers that lack these keywords.
#define RESID_HAVE_BOOL 1

#if !RESID_HAVE_BOOL
typedef int bool;
const bool true = 1;
const bool false = 0;
#endif

// We could have used the smallest possible data type for each SID register,
// however this would give a slower engine because of data type conversions.
// An int is assumed to be at least 32 bits (necessary in the types reg24,
// cycle_count, and sound_sample). GNU does not support 16-bit machines
// (GNU Coding Standards: Portability between CPUs), so this should be
// a valid assumption.

typedef unsigned int Reg4;
typedef unsigned int Reg8;
typedef unsigned int Reg12;
typedef unsigned int Reg16;
typedef unsigned int Reg24;

typedef int CycleCount;
typedef int SoundSample;
typedef SoundSample FcPoint[2];

/// SID chip revision to emulate. The two revisions differ in combined-waveform tables, filter
/// cutoff curve and DC offsets.
enum ChipModel : std::uint8_t
{
    kMos6581, ///< Original NMOS SID: DC offsets in the waveform, voice and mixer stages, tanh-shaped cutoff curve.
    kMos8580  ///< HMOS-II revision: no DC offsets, near-linear cutoff curve.
};

/// How SID::clock() turns the ~1 MHz chip output into samples at the requested sample rate.
/// Listed from cheapest to most accurate; the resampling methods cost a FIR convolution per sample.
enum SamplingMethod : std::uint8_t
{
    kSampleFast,                ///< Clock in delta steps and pick the nearest sample. Fastest; aliases.
    kSampleInterpolate,         ///< Clock every cycle and linearly interpolate between the two nearest samples.
    kSampleResampleInterpolate, ///< Band-limited resampling with a Kaiser-windowed sinc FIR, linearly interpolated between small filter tables.
    kSampleResampleFast         ///< Band-limited resampling with one large, non-interpolated FIR table. Faster, more memory.
};

extern "C"
{
    extern const char* resid_version_string;
}

#define RESID_INLINE inline
