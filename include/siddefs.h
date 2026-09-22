#pragma once

#include <cstdint>

// We could have used the smallest possible data type for each SID register,
// however this would give a slower engine because of data type conversions.
// An int is assumed to be at least 32 bits (necessary in the types Reg24,
// CycleCount, and SoundSample). GNU does not support 16-bit machines
// (GNU Coding Standards: Portability between CPUs), so this should be
// a valid assumption.

using Reg4 = unsigned int;
using Reg8 = unsigned int;
using Reg12 = unsigned int;
using Reg16 = unsigned int;
using Reg24 = unsigned int;

using CycleCount = int;
using SoundSample = int;
using FcPoint = SoundSample[2];

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
