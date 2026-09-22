//  ---------------------------------------------------------------------------
//  This file is part of reSID, a MOS6581 SID emulator engine.
//  Copyright (C) 2004  Dag Lem <resid@nimrod.no>
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

#include "ExternalFilter.h"
#include "Filter.h"
#include "Potentiometer.h"
#include "Voice.h"
#include "siddefs.h"

namespace synthaxes::hw::engine::sid
{

    class RESID_API SID
    {
    public:
        SID();
        ~SID();

        void setChipModel(ChipModel model);
        void enableFilter(bool enable);
        void enableExternalFilter(bool enable);
        bool setSamplingParameters(double clockFreq, SamplingMethod method, double sampleFreq, double passFreq = -1, double filterScale = 0.97);
        void adjustSamplingFrequency(double sampleFreq);

        void fcDefault(const FcPoint*& points, int& count);
        PointPlotter<SoundSample> fcPlotter();

        void clock();
        void clock(CycleCount deltaT);
        int clock(CycleCount& deltaT, short* buf, int n, int interleave = 1);
        void reset();

        // Read/write registers.
        Reg8 read(Reg8 offset);
        void write(Reg8 offset, Reg8 value);

        // Read/write state.
        class State
        {
        public:
            State();

            char m_sidRegister[0x20];

            Reg8 m_busValue;
            CycleCount m_busValueTtl;

            Reg24 m_accumulator[3];
            Reg24 m_shiftRegister[3];
            Reg16 m_rateCounter[3];
            Reg16 m_rateCounterPeriod[3];
            Reg16 m_exponentialCounter[3];
            Reg16 m_exponentialCounterPeriod[3];
            Reg8 m_envelopeCounter[3];
            EnvelopeGenerator::State m_envelopeState[3];
            bool m_holdZero[3];
        };

        State readState();
        void writeState(const State& state);

        // 16-bit input (EXT IN).
        void input(int sample);

        // 16-bit output (AUDIO OUT).
        int output();
        // n-bit output.
        int output(int bits);

    protected:
        static double i0(double x);
        RESID_INLINE int clockFast(CycleCount& deltaT, short* buf, int n, int interleave);
        RESID_INLINE int clockInterpolate(CycleCount& deltaT, short* buf, int n, int interleave);
        RESID_INLINE int clockResampleInterpolate(CycleCount& deltaT, short* buf, int n, int interleave);
        RESID_INLINE int clockResampleFast(CycleCount& deltaT, short* buf, int n, int interleave);

        Voice m_voice[3];
        Filter m_filter;
        ExternalFilter m_extfilt;
        Potentiometer m_potx;
        Potentiometer m_poty;

        Reg8 m_busValue;
        CycleCount m_busValueTtl;

        double m_clockFrequency;

        // External audio input.
        int m_extIn;

        // Resampling constants.
        // The error in interpolated lookup is bounded by 1.234/L^2,
        // while the error in non-interpolated lookup is bounded by
        // 0.7854/L + 0.4113/L^2, see
        // http://www-ccrma.stanford.edu/~jos/resample/Choice_Table_Size.html
        // For a resolution of 16 bits this yields L >= 285 and L >= 51473,
        // respectively.
        static const int kFirN = 125;
        static const int kFirResInterpolate = 285;
        static const int kFirResFast = 51473;
        static const int kFirShift = 15;
        static const int kRingSize = 16384;

        // Fixpoint constants (16.16 bits).
        static const int kFixpShift = 16;
        static const int kFixpMask = 0xffff;

        // Sampling variables.
        SamplingMethod m_sampling;
        CycleCount m_cyclesPerSample;
        CycleCount m_sampleOffset;
        int m_sampleIndex;
        short m_samplePrev;
        int m_firN;
        int m_firRes;

        // Ring buffer with overflow for contiguous storage of RINGSIZE samples.
        short* m_sample;

        // FIR_RES filter tables (FIR_N*FIR_RES).
        short* m_fir;
    };

} // namespace synthaxes::hw::engine::sid
