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

#include <WaveformGenerator.h>

namespace synthaxes::hw::engine::sid
{

    // ----------------------------------------------------------------------------
    // Constructor.
    // ----------------------------------------------------------------------------
    WaveformGenerator::WaveformGenerator()
    {
        this->m_syncSource = this;

        this->setChipModel(kMos6581);

        this->reset();
    }

    // ----------------------------------------------------------------------------
    // Set sync source.
    // ----------------------------------------------------------------------------
    void WaveformGenerator::setSyncSource(WaveformGenerator* source)
    {
        this->m_syncSource = source;
        source->m_syncDest = this;
    }

    // ----------------------------------------------------------------------------
    // Set chip model.
    // ----------------------------------------------------------------------------
    void WaveformGenerator::setChipModel(ChipModel model)
    {
        if(model == kMos6581)
        {
            this->m_waveSt = WaveformGenerator::wave6581St;
            this->m_wavePT = WaveformGenerator::wave6581PT;
            this->m_wavePs = WaveformGenerator::wave6581Ps;
            this->m_wavePst = WaveformGenerator::wave6581Pst;
        }
        else
        {
            this->m_waveSt = WaveformGenerator::wave8580St;
            this->m_wavePT = WaveformGenerator::wave8580PT;
            this->m_wavePs = WaveformGenerator::wave8580Ps;
            this->m_wavePst = WaveformGenerator::wave8580Pst;
        }
    }

    // ----------------------------------------------------------------------------
    // Register functions.
    // ----------------------------------------------------------------------------
    void WaveformGenerator::writeFreqLo(Reg8 freqLo)
    {
        this->m_freq = (this->m_freq & 0xff00) | (freqLo & 0x00ff);
    }

    void WaveformGenerator::writeFreqHi(Reg8 freqHi)
    {
        this->m_freq = ((freqHi << 8) & 0xff00) | (this->m_freq & 0x00ff);
    }

    void WaveformGenerator::writePwLo(Reg8 pwLo)
    {
        this->m_pw = (this->m_pw & 0xf00) | (pwLo & 0x0ff);
    }

    void WaveformGenerator::writePwHi(Reg8 pwHi)
    {
        this->m_pw = ((pwHi << 8) & 0xf00) | (this->m_pw & 0x0ff);
    }

    void WaveformGenerator::writeControlReg(Reg8 control)
    {
        this->m_waveform = (control >> 4) & 0x0f;
        this->m_ringMod = control & 0x04;
        this->m_sync = control & 0x02;

        Reg8 testNext = control & 0x08;

        // Test bit set.
        // The accumulator and the shift register are both cleared.
        // NB! The shift register is not really cleared immediately. It seems like
        // the individual bits in the shift register start to fade down towards
        // zero when test is set. All bits reach zero within approximately
        // $2000 - $4000 cycles.
        // This is not modeled. There should fortunately be little audible output
        // from this peculiar behavior.
        if(testNext)
        {
            this->m_accumulator = 0;
            this->m_shiftRegister = 0;
        }
        // Test bit cleared.
        // The accumulator starts counting, and the shift register is reset to
        // the value 0x7ffff8.
        // NB! The shift register will not actually be set to this exact value if the
        // shift register bits have not had time to fade to zero.
        // This is not modeled.
        else if(this->m_test)
        {
            this->m_shiftRegister = 0x7ffff8;
        }

        this->m_test = testNext;

        // The gate bit is handled by the EnvelopeGenerator.
    }

    Reg8 WaveformGenerator::readOSC()
    {
        return this->output() >> 4;
    }

    // ----------------------------------------------------------------------------
    // SID reset.
    // ----------------------------------------------------------------------------
    void WaveformGenerator::reset()
    {
        this->m_accumulator = 0;
        this->m_shiftRegister = 0x7ffff8;
        this->m_freq = 0;
        this->m_pw = 0;

        this->m_test = 0;
        this->m_ringMod = 0;
        this->m_sync = 0;

        this->m_msbRising = false;
    }

} // namespace synthaxes::hw::engine::sid
