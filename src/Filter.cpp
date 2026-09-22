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

#include <Filter.h>

namespace synthaxes::hw::engine::sid
{

    // Maximum cutoff frequency is specified as
    // FCmax = 2.6e-5/C = 2.6e-5/2200e-12 = 11818.
    //
    // Measurements indicate a cutoff frequency range of approximately
    // 220Hz - 18kHz on a MOS6581 fitted with 470pF capacitors. The function
    // mapping FC to cutoff frequency has the shape of the tanh function, with
    // a discontinuity at FCHI = 0x80.
    // In contrast, the MOS8580 almost perfectly corresponds with the
    // specification of a linear mapping from 30Hz to 12kHz.
    //
    // The mappings have been measured by feeding the SID with an external
    // signal since the chip itself is incapable of generating waveforms of
    // higher fundamental frequency than 4kHz. It is best to use the bandpass
    // output at full resonance to pick out the cutoff frequency at any given
    // FC setting.
    //
    // The mapping function is specified with spline interpolation points and
    // the function values are retrieved via table lookup.
    //
    // NB! Cutoff frequency characteristics may vary, we have modeled two
    // particular Commodore 64s.

    FcPoint Filter::f0Points6581[] = {
        //  FC      f         FCHI FCLO
        // ----------------------------
        { 0, 220 },      // 0x00      - repeated end point
        { 0, 220 },      // 0x00
        { 128, 230 },    // 0x10
        { 256, 250 },    // 0x20
        { 384, 300 },    // 0x30
        { 512, 420 },    // 0x40
        { 640, 780 },    // 0x50
        { 768, 1600 },   // 0x60
        { 832, 2300 },   // 0x68
        { 896, 3200 },   // 0x70
        { 960, 4300 },   // 0x78
        { 992, 5000 },   // 0x7c
        { 1008, 5400 },  // 0x7e
        { 1016, 5700 },  // 0x7f
        { 1023, 6000 },  // 0x7f 0x07
        { 1023, 6000 },  // 0x7f 0x07 - discontinuity
        { 1024, 4600 },  // 0x80      -
        { 1024, 4600 },  // 0x80
        { 1032, 4800 },  // 0x81
        { 1056, 5300 },  // 0x84
        { 1088, 6000 },  // 0x88
        { 1120, 6600 },  // 0x8c
        { 1152, 7200 },  // 0x90
        { 1280, 9500 },  // 0xa0
        { 1408, 12000 }, // 0xb0
        { 1536, 14500 }, // 0xc0
        { 1664, 16000 }, // 0xd0
        { 1792, 17100 }, // 0xe0
        { 1920, 17700 }, // 0xf0
        { 2047, 18000 }, // 0xff 0x07
        { 2047, 18000 }  // 0xff 0x07 - repeated end point
    };

    FcPoint Filter::f0Points8580[] = {
        //  FC      f         FCHI FCLO
        // ----------------------------
        { 0, 0 },        // 0x00      - repeated end point
        { 0, 0 },        // 0x00
        { 128, 800 },    // 0x10
        { 256, 1600 },   // 0x20
        { 384, 2500 },   // 0x30
        { 512, 3300 },   // 0x40
        { 640, 4100 },   // 0x50
        { 768, 4800 },   // 0x60
        { 896, 5600 },   // 0x70
        { 1024, 6500 },  // 0x80
        { 1152, 7500 },  // 0x90
        { 1280, 8400 },  // 0xa0
        { 1408, 9200 },  // 0xb0
        { 1536, 9800 },  // 0xc0
        { 1664, 10500 }, // 0xd0
        { 1792, 11000 }, // 0xe0
        { 1920, 11700 }, // 0xf0
        { 2047, 12500 }, // 0xff 0x07
        { 2047, 12500 }  // 0xff 0x07 - repeated end point
    };

    // ----------------------------------------------------------------------------
    // Constructor.
    // ----------------------------------------------------------------------------
    Filter::Filter()
    {
        this->m_fc = 0;

        this->m_res = 0;

        this->m_filt = 0;

        this->m_voice3off = 0;

        this->m_hpBpLp = 0;

        this->m_vol = 0;

        // State of filter.
        this->m_vhp = 0;
        this->m_vbp = 0;
        this->m_vlp = 0;
        this->m_vnf = 0;

        this->enableFilter(true);

        // Create mappings from FC to cutoff frequency.
        interpolate(Filter::f0Points6581, Filter::f0Points6581 + sizeof(Filter::f0Points6581) / sizeof(*Filter::f0Points6581) - 1, PointPlotter<SoundSample>(this->m_f06581), 1.0);
        interpolate(Filter::f0Points8580, Filter::f0Points8580 + sizeof(Filter::f0Points8580) / sizeof(*Filter::f0Points8580) - 1, PointPlotter<SoundSample>(this->m_f08580), 1.0);

        this->setChipModel(kMos6581);
    }

    // ----------------------------------------------------------------------------
    // Enable filter.
    // ----------------------------------------------------------------------------
    void Filter::enableFilter(bool enable)
    {
        this->m_enabled = enable;
    }

    // ----------------------------------------------------------------------------
    // Set chip model.
    // ----------------------------------------------------------------------------
    void Filter::setChipModel(ChipModel model)
    {
        if(model == kMos6581)
        {
            // The mixer has a small input DC offset. This is found as follows:
            //
            // The "zero" output level of the mixer measured on the SID audio
            // output pin is 5.50V at zero volume, and 5.44 at full
            // volume. This yields a DC offset of (5.44V - 5.50V) = -0.06V.
            //
            // The DC offset is thus -0.06V/1.05V ~ -1/18 of the dynamic range
            // of one voice. See voice.cc for measurement of the dynamic
            // range.

            this->m_mixerDc = -0xfff * 0xff / 18 >> 7;

            this->m_f0 = this->m_f06581;
            this->m_f0Points = Filter::f0Points6581;
            this->m_f0Count = sizeof(Filter::f0Points6581) / sizeof(*Filter::f0Points6581);
        }
        else
        {
            // No DC offsets in the MOS8580.
            this->m_mixerDc = 0;

            this->m_f0 = this->m_f08580;
            this->m_f0Points = Filter::f0Points8580;
            this->m_f0Count = sizeof(Filter::f0Points8580) / sizeof(*Filter::f0Points8580);
        }

        this->setW0();
        this->setQ();
    }

    // ----------------------------------------------------------------------------
    // SID reset.
    // ----------------------------------------------------------------------------
    void Filter::reset()
    {
        this->m_fc = 0;

        this->m_res = 0;

        this->m_filt = 0;

        this->m_voice3off = 0;

        this->m_hpBpLp = 0;

        this->m_vol = 0;

        // State of filter.
        this->m_vhp = 0;
        this->m_vbp = 0;
        this->m_vlp = 0;
        this->m_vnf = 0;

        this->setW0();
        this->setQ();
    }

    // ----------------------------------------------------------------------------
    // Register functions.
    // ----------------------------------------------------------------------------
    void Filter::writeFcLo(Reg8 fcLo)
    {
        this->m_fc = (this->m_fc & 0x7f8) | (fcLo & 0x007);
        this->setW0();
    }

    void Filter::writeFcHi(Reg8 fcHi)
    {
        this->m_fc = ((fcHi << 3) & 0x7f8) | (this->m_fc & 0x007);
        this->setW0();
    }

    void Filter::writeResFilt(Reg8 resFilt)
    {
        this->m_res = (resFilt >> 4) & 0x0f;
        this->setQ();

        this->m_filt = resFilt & 0x0f;
    }

    void Filter::writeModeVol(Reg8 modeVol)
    {
        this->m_voice3off = modeVol & 0x80;

        this->m_hpBpLp = (modeVol >> 4) & 0x07;

        this->m_vol = modeVol & 0x0f;
    }

    // Set filter cutoff frequency.
    void Filter::setW0()
    {
        const double pi = 3.1415926535897932385;

        // Multiply with 1.048576 to facilitate division by 1 000 000 by right-
        // shifting 20 times (2 ^ 20 = 1048576).
        this->m_w0 = static_cast<SoundSample>(2 * pi * this->m_f0[this->m_fc] * 1.048576);

        // Limit f0 to 16kHz to keep 1 cycle filter stable.
        const SoundSample w0Max1 = static_cast<SoundSample>(2 * pi * 16000 * 1.048576);
        this->m_w0Ceil1 = this->m_w0 <= w0Max1 ? this->m_w0 : w0Max1;

        // Limit f0 to 4kHz to keep delta_t cycle filter stable.
        const SoundSample w0MaxDt = static_cast<SoundSample>(2 * pi * 4000 * 1.048576);
        this->m_w0CeilDt = this->m_w0 <= w0MaxDt ? this->m_w0 : w0MaxDt;
    }

    // Set filter resonance.
    void Filter::setQ()
    {
        // Q is controlled linearly by res. Q has approximate range [0.707, 1.7].
        // As resonance is increased, the filter must be clocked more often to keep
        // stable.

        // The coefficient 1024 is dispensed of later by right-shifting 10 times
        // (2 ^ 10 = 1024).
        this->m_1024DivQ = static_cast<SoundSample>(1024.0 / (0.707 + 1.0 * this->m_res / 0x0f));
    }

    // ----------------------------------------------------------------------------
    // Spline functions.
    // ----------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Return the array of spline interpolation points used to map the FC register
    // to filter cutoff frequency.
    // ----------------------------------------------------------------------------
    void Filter::fcDefault(const FcPoint*& points, int& count)
    {
        points = this->m_f0Points;
        count = this->m_f0Count;
    }

    // ----------------------------------------------------------------------------
    // Given an array of interpolation points p with n points, the following
    // statement will specify a new FC mapping:
    //   interpolate(p, p + n - 1, filter.fc_plotter(), 1.0);
    // Note that the x range of the interpolation points *must* be [0, 2047],
    // and that additional end points *must* be present since the end points
    // are not interpolated.
    // ----------------------------------------------------------------------------
    PointPlotter<SoundSample> Filter::fcPlotter()
    {
        return PointPlotter<SoundSample>(this->m_f0);
    }

} // namespace synthaxes::hw::engine::sid
