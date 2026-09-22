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

#include "siddefs.h"

namespace synthaxes::hw::engine::sid
{

    // ----------------------------------------------------------------------------
    // A 24 bit accumulator is the basis for waveform generation. FREQ is added to
    // the lower 16 bits of the accumulator each cycle.
    // The accumulator is set to zero when TEST is set, and starts counting
    // when TEST is cleared.
    // The noise waveform is taken from intermediate bits of a 23 bit shift
    // register. This register is clocked by bit 19 of the accumulator.
    // ----------------------------------------------------------------------------
    class RESID_API WaveformGenerator
    {
    public:
        WaveformGenerator();

        void setSyncSource(WaveformGenerator*);
        void setChipModel(ChipModel model);

        RESID_INLINE void clock();
        RESID_INLINE void clock(CycleCount deltaT);
        RESID_INLINE void synchronize();
        void reset();

        void writeFreqLo(Reg8);
        void writeFreqHi(Reg8);
        void writePwLo(Reg8);
        void writePwHi(Reg8);
        void writeControlReg(Reg8);
        Reg8 readOSC();

        // 12-bit waveform output.
        RESID_INLINE Reg12 output();

    protected:
        const WaveformGenerator* m_syncSource;
        WaveformGenerator* m_syncDest;

        // Tell whether the accumulator MSB was set high on this cycle.
        bool m_msbRising;

        Reg24 m_accumulator;
        Reg24 m_shiftRegister;

        // Fout  = (Fn*Fclk/16777216)Hz
        Reg16 m_freq;
        // PWout = (PWn/40.95)%
        Reg12 m_pw;

        // The control register right-shifted 4 bits; used for output function
        // table lookup.
        Reg8 m_waveform;

        // The remaining control register bits.
        Reg8 m_test;
        Reg8 m_ringMod;
        Reg8 m_sync;
        // The gate bit is handled by the EnvelopeGenerator.

        // 16 possible combinations of waveforms.
        RESID_INLINE Reg12 outputNone();
        RESID_INLINE Reg12 outputT();
        RESID_INLINE Reg12 outputS();
        RESID_INLINE Reg12 outputSt();
        RESID_INLINE Reg12 outputP();
        RESID_INLINE Reg12 outputPT();
        RESID_INLINE Reg12 outputPs();
        RESID_INLINE Reg12 outputPst();
        RESID_INLINE Reg12 outputN();
        RESID_INLINE Reg12 outputNT();
        RESID_INLINE Reg12 outputNS();
        RESID_INLINE Reg12 outputNSt();
        RESID_INLINE Reg12 outputNp();
        RESID_INLINE Reg12 outputNpT();
        RESID_INLINE Reg12 outputNps();
        RESID_INLINE Reg12 outputNPST();

        // Sample data for combinations of waveforms.
        static Reg8 wave6581St[];
        static Reg8 wave6581PT[];
        static Reg8 wave6581Ps[];
        static Reg8 wave6581Pst[];

        static Reg8 wave8580St[];
        static Reg8 wave8580PT[];
        static Reg8 wave8580Ps[];
        static Reg8 wave8580Pst[];

        Reg8* m_waveSt;
        Reg8* m_wavePT;
        Reg8* m_wavePs;
        Reg8* m_wavePst;

        friend class Voice;
        friend class SID;
    };

    // ----------------------------------------------------------------------------
    // Inline functions.
    // The following functions are defined inline because they are called every
    // time a sample is calculated.
    // ----------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // SID clocking - 1 cycle.
    // ----------------------------------------------------------------------------
    RESID_INLINE
    void WaveformGenerator::clock()
    {
        // No operation if test bit is set.
        if(this->m_test)
        {
            return;
        }

        Reg24 accumulatorPrev = this->m_accumulator;

        // Calculate new accumulator value;
        this->m_accumulator += this->m_freq;
        this->m_accumulator &= 0xffffff;

        // Check whether the MSB is set high. This is used for synchronization.
        this->m_msbRising = !(accumulatorPrev & 0x800000) && (this->m_accumulator & 0x800000);

        // Shift noise register once for each time accumulator bit 19 is set high.
        if(!(accumulatorPrev & 0x080000) && (this->m_accumulator & 0x080000))
        {
            Reg24 bit0 = ((this->m_shiftRegister >> 22) ^ (this->m_shiftRegister >> 17)) & 0x1;
            this->m_shiftRegister <<= 1;
            this->m_shiftRegister &= 0x7fffff;
            this->m_shiftRegister |= bit0;
        }
    }

    // ----------------------------------------------------------------------------
    // SID clocking - delta_t cycles.
    // ----------------------------------------------------------------------------
    RESID_INLINE
    void WaveformGenerator::clock(CycleCount deltaT)
    {
        // No operation if test bit is set.
        if(this->m_test)
        {
            return;
        }

        Reg24 accumulatorPrev = this->m_accumulator;

        // Calculate new accumulator value;
        Reg24 deltaAccumulator = deltaT * this->m_freq;
        this->m_accumulator += deltaAccumulator;
        this->m_accumulator &= 0xffffff;

        // Check whether the MSB is set high. This is used for synchronization.
        this->m_msbRising = !(accumulatorPrev & 0x800000) && (this->m_accumulator & 0x800000);

        // Shift noise register once for each time accumulator bit 19 is set high.
        // Bit 19 is set high each time 2^20 (0x100000) is added to the accumulator.
        Reg24 shiftPeriod = 0x100000;

        while(deltaAccumulator)
        {
            if(deltaAccumulator < shiftPeriod)
            {
                shiftPeriod = deltaAccumulator;
                // Determine whether bit 19 is set on the last period.
                // NB! Requires two's complement integer.
                if(shiftPeriod <= 0x080000)
                {
                    // Check for flip from 0 to 1.
                    if(((this->m_accumulator - shiftPeriod) & 0x080000) || !(this->m_accumulator & 0x080000))
                    {
                        break;
                    }
                }
                else
                {
                    // Check for flip from 0 (to 1 or via 1 to 0) or from 1 via 0 to 1.
                    if(((this->m_accumulator - shiftPeriod) & 0x080000) && !(this->m_accumulator & 0x080000))
                    {
                        break;
                    }
                }
            }

            // Shift the noise/random register.
            // NB! The shift is actually delayed 2 cycles, this is not modeled.
            Reg24 bit0 = ((this->m_shiftRegister >> 22) ^ (this->m_shiftRegister >> 17)) & 0x1;
            this->m_shiftRegister <<= 1;
            this->m_shiftRegister &= 0x7fffff;
            this->m_shiftRegister |= bit0;

            deltaAccumulator -= shiftPeriod;
        }
    }

    // ----------------------------------------------------------------------------
    // Synchronize oscillators.
    // This must be done after all the oscillators have been clock()'ed since the
    // oscillators operate in parallel.
    // Note that the oscillators must be clocked exactly on the cycle when the
    // MSB is set high for hard sync to operate correctly. See SID::clock().
    // ----------------------------------------------------------------------------
    RESID_INLINE
    void WaveformGenerator::synchronize()
    {
        // A special case occurs when a sync source is synced itself on the same
        // cycle as when its MSB is set high. In this case the destination will
        // not be synced. This has been verified by sampling OSC3.
        if(this->m_msbRising && this->m_syncDest->m_sync && !(this->m_sync && this->m_syncSource->m_msbRising))
        {
            this->m_syncDest->m_accumulator = 0;
        }
    }

    // ----------------------------------------------------------------------------
    // Output functions.
    // NB! The output from SID 8580 is delayed one cycle compared to SID 6581,
    // this is not modeled.
    // ----------------------------------------------------------------------------

    // No waveform:
    // Zero output.
    //
    RESID_INLINE
    Reg12 WaveformGenerator::outputNone()
    {
        return 0x000;
    }

    // Triangle:
    // The upper 12 bits of the accumulator are used.
    // The MSB is used to create the falling edge of the triangle by inverting
    // the lower 11 bits. The MSB is thrown away and the lower 11 bits are
    // left-shifted (half the resolution, full amplitude).
    // Ring modulation substitutes the MSB with MSB EOR sync_source MSB.
    //
    RESID_INLINE
    Reg12 WaveformGenerator::outputT()
    {
        Reg24 msb = (this->m_ringMod ? this->m_accumulator ^ this->m_syncSource->m_accumulator : this->m_accumulator) & 0x800000;
        return ((msb ? ~this->m_accumulator : this->m_accumulator) >> 11) & 0xfff;
    }

    // Sawtooth:
    // The output is identical to the upper 12 bits of the accumulator.
    //
    RESID_INLINE
    Reg12 WaveformGenerator::outputS()
    {
        return this->m_accumulator >> 12;
    }

    // Pulse:
    // The upper 12 bits of the accumulator are used.
    // These bits are compared to the pulse width register by a 12 bit digital
    // comparator; output is either all one or all zero bits.
    // NB! The output is actually delayed one cycle after the compare.
    // This is not modeled.
    //
    // The test bit, when set to one, holds the pulse waveform output at 0xfff
    // regardless of the pulse width setting.
    //
    RESID_INLINE
    Reg12 WaveformGenerator::outputP()
    {
        return (this->m_test || (this->m_accumulator >> 12) >= this->m_pw) ? 0xfff : 0x000;
    }

    // Noise:
    // The noise output is taken from intermediate bits of a 23-bit shift register
    // which is clocked by bit 19 of the accumulator.
    // NB! The output is actually delayed 2 cycles after bit 19 is set high.
    // This is not modeled.
    //
    // Operation: Calculate EOR result, shift register, set bit 0 = result.
    //
    //                        ----------------------->---------------------
    //                        |                                            |
    //                   ----EOR----                                       |
    //                   |         |                                       |
    //                   2 2 2 1 1 1 1 1 1 1 1 1 1                         |
    // Register bits:    2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 <---
    //                   |   |       |     |   |       |     |   |
    // OSC3 bits  :      7   6       5     4   3       2     1   0
    //
    // Since waveform output is 12 bits the output is left-shifted 4 times.
    //
    RESID_INLINE
    Reg12 WaveformGenerator::outputN()
    {
        return ((this->m_shiftRegister & 0x400000) >> 11) | ((this->m_shiftRegister & 0x100000) >> 10) | ((this->m_shiftRegister & 0x010000) >> 7) | ((this->m_shiftRegister & 0x002000) >> 5) | ((this->m_shiftRegister & 0x000800) >> 4) | ((this->m_shiftRegister & 0x000080) >> 1) |
               ((this->m_shiftRegister & 0x000010) << 1) | ((this->m_shiftRegister & 0x000004) << 2);
    }

    // Combined waveforms:
    // By combining waveforms, the bits of each waveform are effectively short
    // circuited. A zero bit in one waveform will result in a zero output bit
    // (thus the infamous claim that the waveforms are AND'ed).
    // However, a zero bit in one waveform will also affect the neighboring bits
    // in the output. The reason for this has not been determined.
    //
    // Example:
    //
    //             1 1
    // Bit #       1 0 9 8 7 6 5 4 3 2 1 0
    //             -----------------------
    // Sawtooth    0 0 0 1 1 1 1 1 1 0 0 0
    //
    // Triangle    0 0 1 1 1 1 1 1 0 0 0 0
    //
    // AND         0 0 0 1 1 1 1 1 0 0 0 0
    //
    // Output      0 0 0 0 1 1 1 0 0 0 0 0
    //
    //
    // This behavior would be quite difficult to model exactly, since the SID
    // in this case does not act as a digital state machine. Tests show that minor
    // (1 bit)  differences can actually occur in the output from otherwise
    // identical samples from OSC3 when waveforms are combined. To further
    // complicate the situation the output changes slightly with time (more
    // neighboring bits are successively set) when the 12-bit waveform
    // registers are kept unchanged.
    //
    // It is probably possible to come up with a valid model for the
    // behavior, however this would be far too slow for practical use since it
    // would have to be based on the mutual influence of individual bits.
    //
    // The output is instead approximated by using the upper bits of the
    // accumulator as an index to look up the combined output in a table
    // containing actual combined waveform samples from OSC3.
    // These samples are 8 bit, so 4 bits of waveform resolution is lost.
    // All OSC3 samples are taken with FREQ=0x1000, adding a 1 to the upper 12
    // bits of the accumulator each cycle for a sample period of 4096 cycles.
    //
    // Sawtooth+Triangle:
    // The sawtooth output is used to look up an OSC3 sample.
    //
    // Pulse+Triangle:
    // The triangle output is right-shifted and used to look up an OSC3 sample.
    // The sample is output if the pulse output is on.
    // The reason for using the triangle output as the index is to handle ring
    // modulation. Only the first half of the sample is used, which should be OK
    // since the triangle waveform has half the resolution of the accumulator.
    //
    // Pulse+Sawtooth:
    // The sawtooth output is used to look up an OSC3 sample.
    // The sample is output if the pulse output is on.
    //
    // Pulse+Sawtooth+Triangle:
    // The sawtooth output is used to look up an OSC3 sample.
    // The sample is output if the pulse output is on.
    //
    RESID_INLINE
    Reg12 WaveformGenerator::outputSt()
    {
        return this->m_waveSt[this->outputS()] << 4;
    }

    RESID_INLINE
    Reg12 WaveformGenerator::outputPT()
    {
        return (this->m_wavePT[this->outputT() >> 1] << 4) & this->outputP();
    }

    RESID_INLINE
    Reg12 WaveformGenerator::outputPs()
    {
        return (this->m_wavePs[this->outputS()] << 4) & this->outputP();
    }

    RESID_INLINE
    Reg12 WaveformGenerator::outputPst()
    {
        return (this->m_wavePst[this->outputS()] << 4) & this->outputP();
    }

    // Combined waveforms including noise:
    // All waveform combinations including noise output zero after a few cycles.
    // NB! The effects of such combinations are not fully explored. It is claimed
    // that the shift register may be filled with zeroes and locked up, which
    // seems to be true.
    // We have not attempted to model this behavior, suffice to say that
    // there is very little audible output from waveform combinations including
    // noise. We hope that nobody is actually using it.
    //
    RESID_INLINE
    Reg12 WaveformGenerator::outputNT()
    {
        return 0;
    }

    RESID_INLINE
    Reg12 WaveformGenerator::outputNS()
    {
        return 0;
    }

    RESID_INLINE
    Reg12 WaveformGenerator::outputNSt()
    {
        return 0;
    }

    RESID_INLINE
    Reg12 WaveformGenerator::outputNp()
    {
        return 0;
    }

    RESID_INLINE
    Reg12 WaveformGenerator::outputNpT()
    {
        return 0;
    }

    RESID_INLINE
    Reg12 WaveformGenerator::outputNps()
    {
        return 0;
    }

    RESID_INLINE
    Reg12 WaveformGenerator::outputNPST()
    {
        return 0;
    }

    // ----------------------------------------------------------------------------
    // Select one of 16 possible combinations of waveforms.
    // ----------------------------------------------------------------------------
    RESID_INLINE
    Reg12 WaveformGenerator::output()
    {
        // It may seem cleaner to use an array of member functions to return
        // waveform output; however a switch with inline functions is faster.

        switch(this->m_waveform)
        {
        default:
        case 0x0:
            return this->outputNone();
        case 0x1:
            return this->outputT();
        case 0x2:
            return this->outputS();
        case 0x3:
            return this->outputSt();
        case 0x4:
            return this->outputP();
        case 0x5:
            return this->outputPT();
        case 0x6:
            return this->outputPs();
        case 0x7:
            return this->outputPst();
        case 0x8:
            return this->outputN();
        case 0x9:
            return this->outputNT();
        case 0xa:
            return this->outputNS();
        case 0xb:
            return this->outputNSt();
        case 0xc:
            return this->outputNp();
        case 0xd:
            return this->outputNpT();
        case 0xe:
            return this->outputNps();
        case 0xf:
            return this->outputNPST();
        }
    }

} // namespace synthaxes::hw::engine::sid
