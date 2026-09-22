//  ---------------------------------------------------------------------------
//  This file is part of reSID, a MOS6581 SID emulator engine.
//  Copyright (C) 1999  Dag Lem <resid@nimrod.no>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  ---------------------------------------------------------------------------

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

enum ChipModel : std::uint8_t
{
    kMos6581,
    kMos8580
};

enum SamplingMethod : std::uint8_t
{
    kSampleFast,
    kSampleInterpolate,
    kSampleResampleInterpolate,
    kSampleResampleFast
};

extern "C"
{
    extern const char* resid_version_string;
}

#define RESID_INLINE inline

#ifdef RESID_DLL
#ifdef RESID_EXPORTS
#define RESID_API __declspec(dllexport)
#else
#define RESID_API __declspec(dllimport)
#endif // RESID_EXPORTS
#else  // !RESID_DLL
#define RESID_API
#endif // RESID_DLL
