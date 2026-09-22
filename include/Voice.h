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

#include "EnvelopeGenerator.h"
#include "WaveformGenerator.h"
#include "siddefs.h"

namespace synthaxes::hw::engine::sid
{

    class RESID_API Voice
    {
    public:
        Voice();

        void setChipModel(ChipModel model);
        void setSyncSource(Voice*);
        void reset();

        void writeControlReg(Reg8);

        // Amplitude modulated waveform output.
        // Range [-2048*255, 2047*255].
        RESID_INLINE SoundSample output();

    protected:
        WaveformGenerator m_wave;
        EnvelopeGenerator m_envelope;

        // Waveform D/A zero level.
        SoundSample m_waveZero;

        // Multiplying D/A DC offset.
        SoundSample m_voiceDc;

        friend class SID;
    };

    // ----------------------------------------------------------------------------
    // Inline functions.
    // The following function is defined inline because it is called every
    // time a sample is calculated.
    // ----------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Amplitude modulated waveform output.
    // Ideal range [-2048*255, 2047*255].
    // ----------------------------------------------------------------------------
    RESID_INLINE
    SoundSample Voice::output()
    {
        // Multiply oscillator output with envelope output.
        return static_cast<SoundSample>((this->m_wave.output() - this->m_waveZero) * this->m_envelope.output() + this->m_voiceDc);
    }

} // namespace synthaxes::hw::engine::sid
