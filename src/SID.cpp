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

#include <SID.h>
#include <cmath>
#include <cstddef>

namespace synthaxes::hw::engine::sid
{

    // ----------------------------------------------------------------------------
    // Constructor.
    // ----------------------------------------------------------------------------
    SID::SID()
    {
        // Initialize pointers.
        this->m_sample = 0;
        this->m_fir = 0;

        this->m_voice[0].setSyncSource(&this->m_voice[2]);
        this->m_voice[1].setSyncSource(&this->m_voice[0]);
        this->m_voice[2].setSyncSource(&this->m_voice[1]);

        this->setSamplingParameters(985248, kSampleFast, 44100);

        this->m_busValue = 0;
        this->m_busValueTtl = 0;

        this->m_extIn = 0;
    }

    // ----------------------------------------------------------------------------
    // Destructor.
    // ----------------------------------------------------------------------------
    SID::~SID()
    {
        delete[] this->m_sample;
        delete[] this->m_fir;
    }

    // ----------------------------------------------------------------------------
    // Set chip model.
    // ----------------------------------------------------------------------------
    void SID::setChipModel(ChipModel model)
    {
        for(int i = 0; i < 3; i++)
        {
            this->m_voice[i].setChipModel(model);
        }

        this->m_filter.setChipModel(model);
        this->m_extfilt.setChipModel(model);
    }

    // ----------------------------------------------------------------------------
    // SID reset.
    // ----------------------------------------------------------------------------
    void SID::reset()
    {
        for(int i = 0; i < 3; i++)
        {
            this->m_voice[i].reset();
        }
        this->m_filter.reset();
        this->m_extfilt.reset();

        this->m_busValue = 0;
        this->m_busValueTtl = 0;
    }

    // ----------------------------------------------------------------------------
    // Write 16-bit sample to audio input.
    // NB! The caller is responsible for keeping the value within 16 bits.
    // Note that to mix in an external audio signal, the signal should be
    // resampled to 1MHz first to avoid sampling noise.
    // ----------------------------------------------------------------------------
    void SID::input(int sample)
    {
        // Voice outputs are 20 bits. Scale up to match three voices in order
        // to facilitate simulation of the MOS8580 "digi boost" hardware hack.
        this->m_extIn = (sample << 4) * 3;
    }

    // ----------------------------------------------------------------------------
    // Read sample from audio output.
    // Both 16-bit and n-bit output is provided.
    // ----------------------------------------------------------------------------
    int SID::output()
    {
        const int range = 1 << 16;
        const int half = range >> 1;
        int sample = this->m_extfilt.output() / ((4095 * 255 >> 7) * 3 * 15 * 2 / range);
        if(sample >= half)
        {
            return half - 1;
        }
        if(sample < -half)
        {
            return -half;
        }
        return sample;
    }

    int SID::output(int bits)
    {
        const int range = 1 << bits;
        const int half = range >> 1;
        int sample = this->m_extfilt.output() / ((4095 * 255 >> 7) * 3 * 15 * 2 / range);
        if(sample >= half)
        {
            return half - 1;
        }
        if(sample < -half)
        {
            return -half;
        }
        return sample;
    }

    // ----------------------------------------------------------------------------
    // Read registers.
    //
    // Reading a write only register returns the last byte written to any SID
    // register. The individual bits in this value start to fade down towards
    // zero after a few cycles. All bits reach zero within approximately
    // $2000 - $4000 cycles.
    // It has been claimed that this fading happens in an orderly fashion, however
    // sampling of write only registers reveals that this is not the case.
    // NB! This is not correctly modeled.
    // The actual use of write only registers has largely been made in the belief
    // that all SID registers are readable. To support this belief the read
    // would have to be done immediately after a write to the same register
    // (remember that an intermediate write to another register would yield that
    // value instead). With this in mind we return the last value written to
    // any SID register for $2000 cycles without modeling the bit fading.
    // ----------------------------------------------------------------------------
    Reg8 SID::read(Reg8 offset)
    {
        switch(offset)
        {
        case 0x19:
            return this->m_potx.readPOT();
        case 0x1a:
            return this->m_poty.readPOT();
        case 0x1b:
            return this->m_voice[2].m_wave.readOSC();
        case 0x1c:
            return this->m_voice[2].m_envelope.readENV();
        default:
            return this->m_busValue;
        }
    }

    // ----------------------------------------------------------------------------
    // Write registers.
    // ----------------------------------------------------------------------------
    void SID::write(Reg8 offset, Reg8 value)
    {
        this->m_busValue = value;
        this->m_busValueTtl = 0x2000;

        switch(offset)
        {
        case 0x00:
            this->m_voice[0].m_wave.writeFreqLo(value);
            break;
        case 0x01:
            this->m_voice[0].m_wave.writeFreqHi(value);
            break;
        case 0x02:
            this->m_voice[0].m_wave.writePwLo(value);
            break;
        case 0x03:
            this->m_voice[0].m_wave.writePwHi(value);
            break;
        case 0x04:
            this->m_voice[0].writeControlReg(value);
            break;
        case 0x05:
            this->m_voice[0].m_envelope.writeAttackDecay(value);
            break;
        case 0x06:
            this->m_voice[0].m_envelope.writeSustainRelease(value);
            break;
        case 0x07:
            this->m_voice[1].m_wave.writeFreqLo(value);
            break;
        case 0x08:
            this->m_voice[1].m_wave.writeFreqHi(value);
            break;
        case 0x09:
            this->m_voice[1].m_wave.writePwLo(value);
            break;
        case 0x0a:
            this->m_voice[1].m_wave.writePwHi(value);
            break;
        case 0x0b:
            this->m_voice[1].writeControlReg(value);
            break;
        case 0x0c:
            this->m_voice[1].m_envelope.writeAttackDecay(value);
            break;
        case 0x0d:
            this->m_voice[1].m_envelope.writeSustainRelease(value);
            break;
        case 0x0e:
            this->m_voice[2].m_wave.writeFreqLo(value);
            break;
        case 0x0f:
            this->m_voice[2].m_wave.writeFreqHi(value);
            break;
        case 0x10:
            this->m_voice[2].m_wave.writePwLo(value);
            break;
        case 0x11:
            this->m_voice[2].m_wave.writePwHi(value);
            break;
        case 0x12:
            this->m_voice[2].writeControlReg(value);
            break;
        case 0x13:
            this->m_voice[2].m_envelope.writeAttackDecay(value);
            break;
        case 0x14:
            this->m_voice[2].m_envelope.writeSustainRelease(value);
            break;
        case 0x15:
            this->m_filter.writeFcLo(value);
            break;
        case 0x16:
            this->m_filter.writeFcHi(value);
            break;
        case 0x17:
            this->m_filter.writeResFilt(value);
            break;
        case 0x18:
            this->m_filter.writeModeVol(value);
            break;
        default:
            break;
        }
    }

    // ----------------------------------------------------------------------------
    // Constructor.
    // ----------------------------------------------------------------------------
    SID::State::State()
    {
        int i;

        for(i = 0; i < 0x20; i++)
        {
            this->m_sidRegister[i] = 0;
        }

        this->m_busValue = 0;
        this->m_busValueTtl = 0;

        for(i = 0; i < 3; i++)
        {
            this->m_accumulator[i] = 0;
            this->m_shiftRegister[i] = 0x7ffff8;
            this->m_rateCounter[i] = 0;
            this->m_rateCounterPeriod[i] = 9;
            this->m_exponentialCounter[i] = 0;
            this->m_exponentialCounterPeriod[i] = 1;
            this->m_envelopeCounter[i] = 0;
            this->m_envelopeState[i] = EnvelopeGenerator::kRelease;
            this->m_holdZero[i] = true;
        }
    }

    // ----------------------------------------------------------------------------
    // Read state.
    // ----------------------------------------------------------------------------
    SID::State SID::readState()
    {
        State state;
        int i, j;

        for(i = 0, j = 0; i < 3; i++, j += 7)
        {
            WaveformGenerator& wave = this->m_voice[i].m_wave;
            EnvelopeGenerator& envelope = this->m_voice[i].m_envelope;
            state.m_sidRegister[j + 0] = static_cast<char>(wave.m_freq & 0xff);
            state.m_sidRegister[j + 1] = static_cast<char>(wave.m_freq >> 8);
            state.m_sidRegister[j + 2] = static_cast<char>(wave.m_pw & 0xff);
            state.m_sidRegister[j + 3] = static_cast<char>(wave.m_pw >> 8);
            state.m_sidRegister[j + 4] = static_cast<char>((wave.m_waveform << 4) | (wave.m_test ? 0x08 : 0) | (wave.m_ringMod ? 0x04 : 0) | (wave.m_sync ? 0x02 : 0) | (envelope.m_gate ? 0x01 : 0));
            state.m_sidRegister[j + 5] = static_cast<char>((envelope.m_attack << 4) | envelope.m_decay);
            state.m_sidRegister[j + 6] = static_cast<char>((envelope.m_sustain << 4) | envelope.m_release);
        }

        state.m_sidRegister[j++] = static_cast<char>(this->m_filter.m_fc & 0x007);
        state.m_sidRegister[j++] = static_cast<char>(this->m_filter.m_fc >> 3);
        state.m_sidRegister[j++] = static_cast<char>((this->m_filter.m_res << 4) | this->m_filter.m_filt);
        state.m_sidRegister[j++] = static_cast<char>((this->m_filter.m_voice3off ? 0x80 : 0) | (this->m_filter.m_hpBpLp << 4) | this->m_filter.m_vol);

        // These registers are superfluous, but included for completeness.
        for(; j < 0x1d; j++)
        {
            state.m_sidRegister[j] = static_cast<char>(this->read(j));
        }
        for(; j < 0x20; j++)
        {
            state.m_sidRegister[j] = 0;
        }

        state.m_busValue = this->m_busValue;
        state.m_busValueTtl = this->m_busValueTtl;

        for(i = 0; i < 3; i++)
        {
            state.m_accumulator[i] = this->m_voice[i].m_wave.m_accumulator;
            state.m_shiftRegister[i] = this->m_voice[i].m_wave.m_shiftRegister;
            state.m_rateCounter[i] = this->m_voice[i].m_envelope.m_rateCounter;
            state.m_rateCounterPeriod[i] = this->m_voice[i].m_envelope.m_ratePeriod;
            state.m_exponentialCounter[i] = this->m_voice[i].m_envelope.m_exponentialCounter;
            state.m_exponentialCounterPeriod[i] = this->m_voice[i].m_envelope.m_exponentialCounterPeriod;
            state.m_envelopeCounter[i] = this->m_voice[i].m_envelope.m_envelopeCounter;
            state.m_envelopeState[i] = this->m_voice[i].m_envelope.m_state;
            state.m_holdZero[i] = this->m_voice[i].m_envelope.m_holdZero;
        }

        return state;
    }

    // ----------------------------------------------------------------------------
    // Write state.
    // ----------------------------------------------------------------------------
    void SID::writeState(const State& state)
    {
        int i;

        for(i = 0; i <= 0x18; i++)
        {
            this->write(i, state.m_sidRegister[i]);
        }

        this->m_busValue = state.m_busValue;
        this->m_busValueTtl = state.m_busValueTtl;

        for(i = 0; i < 3; i++)
        {
            this->m_voice[i].m_wave.m_accumulator = state.m_accumulator[i];
            this->m_voice[i].m_wave.m_shiftRegister = state.m_shiftRegister[i];
            this->m_voice[i].m_envelope.m_rateCounter = state.m_rateCounter[i];
            this->m_voice[i].m_envelope.m_ratePeriod = state.m_rateCounterPeriod[i];
            this->m_voice[i].m_envelope.m_exponentialCounter = state.m_exponentialCounter[i];
            this->m_voice[i].m_envelope.m_exponentialCounterPeriod = state.m_exponentialCounterPeriod[i];
            this->m_voice[i].m_envelope.m_envelopeCounter = state.m_envelopeCounter[i];
            this->m_voice[i].m_envelope.m_state = state.m_envelopeState[i];
            this->m_voice[i].m_envelope.m_holdZero = state.m_holdZero[i];
        }
    }

    // ----------------------------------------------------------------------------
    // Enable filter.
    // ----------------------------------------------------------------------------
    void SID::enableFilter(bool enable)
    {
        this->m_filter.enableFilter(enable);
    }

    // ----------------------------------------------------------------------------
    // Enable external filter.
    // ----------------------------------------------------------------------------
    void SID::enableExternalFilter(bool enable)
    {
        this->m_extfilt.enableFilter(enable);
    }

    // ----------------------------------------------------------------------------
    // I0() computes the 0th order modified Bessel function of the first kind.
    // This function is originally from resample-1.5/filterkit.c by J. O. Smith.
    // ----------------------------------------------------------------------------
    double SID::i0(double x)
    {
        // Max error acceptable in I0.
        const double i0e = 1e-6;

        double sum, u, halfx, temp;
        int n;

        sum = u = n = 1;
        halfx = x / 2.0;

        do
        {
            temp = halfx / n++;
            u *= temp * temp;
            sum += u;
        }
        while(u >= i0e * sum);

        return sum;
    }

    // ----------------------------------------------------------------------------
    // Setting of SID sampling parameters.
    //
    // Use a clock freqency of 985248Hz for PAL C64, 1022730Hz for NTSC C64.
    // The default end of passband frequency is pass_freq = 0.9*sample_freq/2
    // for sample frequencies up to ~ 44.1kHz, and 20kHz for higher sample
    // frequencies.
    //
    // For resampling, the ratio between the clock frequency and the sample
    // frequency is limited as follows:
    //   125*clock_freq/sample_freq < 16384
    // E.g. provided a clock frequency of ~ 1MHz, the sample frequency can not
    // be set lower than ~ 8kHz. A lower sample frequency would make the
    // resampling code overfill its 16k sample ring buffer.
    //
    // The end of passband frequency is also limited:
    //   pass_freq <= 0.9*sample_freq/2

    // E.g. for a 44.1kHz sampling rate the end of passband frequency is limited
    // to slightly below 20kHz. This constraint ensures that the FIR table is
    // not overfilled.
    // ----------------------------------------------------------------------------
    bool SID::setSamplingParameters(double clockFreq, SamplingMethod method, double sampleFreq, double passFreq, double filterScale)
    {
        // Check resampling constraints.
        if(method == kSampleResampleInterpolate || method == kSampleResampleFast)
        {
            // Check whether the sample ring buffer would overfill.
            if(SID::kFirN * clockFreq / sampleFreq >= SID::kRingSize)
            {
                return false;
            }

            // The default passband limit is 0.9*sample_freq/2 for sample
            // frequencies below ~ 44.1kHz, and 20kHz for higher sample frequencies.
            if(passFreq < 0)
            {
                passFreq = 20000;
                if(2 * passFreq / sampleFreq >= 0.9)
                {
                    passFreq = 0.9 * sampleFreq / 2;
                }
            }
            // Check whether the FIR table would overfill.
            else if(passFreq > 0.9 * sampleFreq / 2)
            {
                return false;
            }

            // The filter scaling is only included to avoid clipping, so keep
            // it sane.
            if(filterScale < 0.9 || filterScale > 1.0)
            {
                return false;
            }
        }

        this->m_clockFrequency = clockFreq;
        this->m_sampling = method;

        this->m_cyclesPerSample = static_cast<CycleCount>(std::lround(clockFreq / sampleFreq * (1 << SID::kFixpShift)));

        this->m_sampleOffset = 0;
        this->m_samplePrev = 0;

        // FIR initialization is only necessary for resampling.
        if(method != kSampleResampleInterpolate && method != kSampleResampleFast)
        {
            delete[] this->m_sample;
            delete[] this->m_fir;
            this->m_sample = 0;
            this->m_fir = 0;
            return true;
        }

        const double pi = 3.1415926535897932385;

        // 16 bits -> -96dB stopband attenuation.
        const double a = -20 * log10(1.0 / (1 << 16));
        // A fraction of the bandwidth is allocated to the transition band,
        double dw = (1 - 2 * passFreq / sampleFreq) * pi;
        // The cutoff frequency is midway through the transition band.
        double wc = (2 * passFreq / sampleFreq + 1) * pi / 2;

        // For calculation of beta and N see the reference for the kaiserord
        // function in the MATLAB Signal Processing Toolbox:
        // http://www.mathworks.com/access/helpdesk/help/toolbox/signal/kaiserord.html
        const double beta = 0.1102 * (a - 8.7);
        const double i0beta = i0(beta);

        // The filter order will maximally be 124 with the current constraints.
        // N >= (96.33 - 7.95)/(2.285*0.1*pi) -> N >= 123
        // The filter order is equal to the number of zero crossings, i.e.
        // it should be an even number (sinc is symmetric about x = 0).
        int order = static_cast<int>(std::lround((a - 7.95) / (2.285 * dw)));
        order += order & 1;

        double fSamplesPerCycle = sampleFreq / clockFreq;
        double fCyclesPerSample = clockFreq / sampleFreq;

        // The filter length is equal to the filter order + 1.
        // The filter length must be an odd number (sinc is symmetric about x = 0).
        this->m_firN = int(order * fCyclesPerSample) + 1;
        this->m_firN |= 1;

        // We clamp the filter table resolution to 2^n, making the fixpoint
        // sample_offset a whole multiple of the filter table resolution.
        int res = method == kSampleResampleInterpolate ? SID::kFirResInterpolate : SID::kFirResFast;
        int n = (int)ceil(log(res / fCyclesPerSample) / log(2));
        this->m_firRes = 1 << n;

        // Allocate memory for FIR tables.
        delete[] this->m_fir;
        this->m_fir = new short[static_cast<std::size_t>(this->m_firN) * this->m_firRes];

        // Calculate fir_RES FIR tables for linear interpolation.
        for(int i = 0; i < this->m_firRes; i++)
        {
            const int firHalf = this->m_firN / 2;
            int firOffset = i * this->m_firN + firHalf;
            double jOffset = double(i) / this->m_firRes;
            // Calculate FIR table. This is the sinc function, weighted by the
            // Kaiser window.
            for(int j = -firHalf; j <= firHalf; j++)
            {
                double jx = j - jOffset;
                double wt = wc * jx / fCyclesPerSample;
                double temp = jx / firHalf;
                double kaiser = fabs(temp) <= 1 ? i0(beta * sqrt(1 - temp * temp)) / i0beta : 0;
                double sincwt = fabs(wt) >= 1e-6 ? sin(wt) / wt : 1;
                double val = (1 << SID::kFirShift) * filterScale * fSamplesPerCycle * wc / pi * sincwt * kaiser;
                this->m_fir[firOffset + j] = static_cast<short>(std::lround(val));
            }
        }

        // Allocate sample buffer.
        if(!this->m_sample)
        {
            this->m_sample = new short[static_cast<std::size_t>(SID::kRingSize) * 2];
        }
        // Clear sample buffer.
        for(int j = 0; j < SID::kRingSize * 2; j++)
        {
            this->m_sample[j] = 0;
        }
        this->m_sampleIndex = 0;

        return true;
    }

    // ----------------------------------------------------------------------------
    // Adjustment of SID sampling frequency.
    //
    // In some applications, e.g. a C64 emulator, it can be desirable to
    // synchronize sound with a timer source. This is supported by adjustment of
    // the SID sampling frequency.
    //
    // NB! Adjustment of the sampling frequency may lead to noticeable shifts in
    // frequency, and should only be used for interactive applications. Note also
    // that any adjustment of the sampling frequency will change the
    // characteristics of the resampling filter, since the filter is not rebuilt.
    // ----------------------------------------------------------------------------
    void SID::adjustSamplingFrequency(double sampleFreq)
    {
        this->m_cyclesPerSample = static_cast<CycleCount>(std::lround(this->m_clockFrequency / sampleFreq * (1 << SID::kFixpShift)));
    }

    // ----------------------------------------------------------------------------
    // Return array of default spline interpolation points to map FC to
    // filter cutoff frequency.
    // ----------------------------------------------------------------------------
    void SID::fcDefault(const FcPoint*& points, int& count)
    {
        this->m_filter.fcDefault(points, count);
    }

    // ----------------------------------------------------------------------------
    // Return FC spline plotter object.
    // ----------------------------------------------------------------------------
    PointPlotter<SoundSample> SID::fcPlotter()
    {
        return this->m_filter.fcPlotter();
    }

    // ----------------------------------------------------------------------------
    // SID clocking - 1 cycle.
    // ----------------------------------------------------------------------------
    void SID::clock()
    {
        int i;

        // Age bus value.
        if(--this->m_busValueTtl <= 0)
        {
            this->m_busValue = 0;
            this->m_busValueTtl = 0;
        }

        // Clock amplitude modulators.
        for(i = 0; i < 3; i++)
        {
            this->m_voice[i].m_envelope.clock();
        }

        // Clock oscillators.
        for(i = 0; i < 3; i++)
        {
            this->m_voice[i].m_wave.clock();
        }

        // Synchronize oscillators.
        for(i = 0; i < 3; i++)
        {
            this->m_voice[i].m_wave.synchronize();
        }

        // Clock filter.
        this->m_filter.clock(this->m_voice[0].output(), this->m_voice[1].output(), this->m_voice[2].output(), this->m_extIn);

        // Clock external filter.
        this->m_extfilt.clock(this->m_filter.output());
    }

    // ----------------------------------------------------------------------------
    // SID clocking - delta_t cycles.
    // ----------------------------------------------------------------------------
    void SID::clock(CycleCount deltaT)
    {
        int i;

        if(deltaT <= 0)
        {
            return;
        }

        // Age bus value.
        this->m_busValueTtl -= deltaT;
        if(this->m_busValueTtl <= 0)
        {
            this->m_busValue = 0;
            this->m_busValueTtl = 0;
        }

        // Clock amplitude modulators.
        for(i = 0; i < 3; i++)
        {
            this->m_voice[i].m_envelope.clock(deltaT);
        }

        // Clock and synchronize oscillators.
        // Loop until we reach the current cycle.
        CycleCount deltaTOsc = deltaT;
        while(deltaTOsc)
        {
            CycleCount deltaTMin = deltaTOsc;

            // Find minimum number of cycles to an oscillator accumulator MSB toggle.
            // We have to clock on each MSB on / MSB off for hard sync to operate
            // correctly.
            for(i = 0; i < 3; i++)
            {
                WaveformGenerator& wave = this->m_voice[i].m_wave;

                // It is only necessary to clock on the MSB of an oscillator that is
                // a sync source and has freq != 0.
                if(!(wave.m_syncDest->m_sync && wave.m_freq))
                {
                    continue;
                }

                Reg16 freq = wave.m_freq;
                Reg24 accumulator = wave.m_accumulator;

                // Clock on MSB off if MSB is on, clock on MSB on if MSB is off.
                Reg24 deltaAccumulator = (accumulator & 0x800000 ? 0x1000000 : 0x800000) - accumulator;

                CycleCount deltaTNext = static_cast<CycleCount>(deltaAccumulator / freq);
                if(deltaAccumulator % freq)
                {
                    ++deltaTNext;
                }

                if(deltaTNext < deltaTMin)
                {
                    deltaTMin = deltaTNext;
                }
            }

            // Clock oscillators.
            for(i = 0; i < 3; i++)
            {
                this->m_voice[i].m_wave.clock(deltaTMin);
            }

            // Synchronize oscillators.
            for(i = 0; i < 3; i++)
            {
                this->m_voice[i].m_wave.synchronize();
            }

            deltaTOsc -= deltaTMin;
        }

        // Clock filter.
        this->m_filter.clock(deltaT, this->m_voice[0].output(), this->m_voice[1].output(), this->m_voice[2].output(), this->m_extIn);

        // Clock external filter.
        this->m_extfilt.clock(deltaT, this->m_filter.output());
    }

    // ----------------------------------------------------------------------------
    // SID clocking with audio sampling.
    // Fixpoint arithmetics is used.
    //
    // The example below shows how to clock the SID a specified amount of cycles
    // while producing audio output:
    //
    // while (delta_t) {
    //   bufindex += sid.clock(delta_t, buf + bufindex, buflength - bufindex);
    //   write(dsp, buf, bufindex*2);
    //   bufindex = 0;
    // }
    //
    // ----------------------------------------------------------------------------
    int SID::clock(CycleCount& deltaT, short* buf, int n, int interleave)
    {
        switch(this->m_sampling)
        {
        default:
        case kSampleFast:
            return this->clockFast(deltaT, buf, n, interleave);
        case kSampleInterpolate:
            return this->clockInterpolate(deltaT, buf, n, interleave);
        case kSampleResampleInterpolate:
            return this->clockResampleInterpolate(deltaT, buf, n, interleave);
        case kSampleResampleFast:
            return this->clockResampleFast(deltaT, buf, n, interleave);
        }
    }

    // ----------------------------------------------------------------------------
    // SID clocking with audio sampling - delta clocking picking nearest sample.
    // ----------------------------------------------------------------------------
    RESID_INLINE
    int SID::clockFast(CycleCount& deltaT, short* buf, int n, int interleave)
    {
        int s = 0;

        for(;;)
        {
            CycleCount nextSampleOffset = this->m_sampleOffset + this->m_cyclesPerSample + (1 << (SID::kFixpShift - 1));
            CycleCount deltaTSample = nextSampleOffset >> SID::kFixpShift;
            if(deltaTSample > deltaT)
            {
                break;
            }
            if(s >= n)
            {
                return s;
            }
            this->clock(deltaTSample);
            deltaT -= deltaTSample;
            this->m_sampleOffset = (nextSampleOffset & SID::kFixpMask) - (1 << (SID::kFixpShift - 1));
            buf[static_cast<std::ptrdiff_t>(s++) * interleave] = static_cast<short>(this->output());
        }

        this->clock(deltaT);
        this->m_sampleOffset -= deltaT << SID::kFixpShift;
        deltaT = 0;
        return s;
    }

    // ----------------------------------------------------------------------------
    // SID clocking with audio sampling - cycle based with linear sample
    // interpolation.
    //
    // Here the chip is clocked every cycle. This yields higher quality
    // sound since the samples are linearly interpolated, and since the
    // external filter attenuates frequencies above 16kHz, thus reducing
    // sampling noise.
    // ----------------------------------------------------------------------------
    RESID_INLINE
    int SID::clockInterpolate(CycleCount& deltaT, short* buf, int n, int interleave)
    {
        int s = 0;
        int i;

        for(;;)
        {
            CycleCount nextSampleOffset = this->m_sampleOffset + this->m_cyclesPerSample;
            CycleCount deltaTSample = nextSampleOffset >> SID::kFixpShift;
            if(deltaTSample > deltaT)
            {
                break;
            }
            if(s >= n)
            {
                return s;
            }
            for(i = 0; i < deltaTSample - 1; i++)
            {
                this->clock();
            }
            if(i < deltaTSample)
            {
                this->m_samplePrev = static_cast<short>(this->output());
                this->clock();
            }

            deltaT -= deltaTSample;
            this->m_sampleOffset = nextSampleOffset & SID::kFixpMask;

            short sampleNow = static_cast<short>(this->output());
            buf[static_cast<std::ptrdiff_t>(s++) * interleave] = static_cast<short>(this->m_samplePrev + (this->m_sampleOffset * (sampleNow - this->m_samplePrev) >> SID::kFixpShift));
            this->m_samplePrev = sampleNow;
        }

        for(i = 0; i < deltaT - 1; i++)
        {
            this->clock();
        }
        if(i < deltaT)
        {
            this->m_samplePrev = static_cast<short>(this->output());
            this->clock();
        }
        this->m_sampleOffset -= deltaT << SID::kFixpShift;
        deltaT = 0;
        return s;
    }

    // ----------------------------------------------------------------------------
    // SID clocking with audio sampling - cycle based with audio resampling.
    //
    // This is the theoretically correct (and computationally intensive) audio
    // sample generation. The samples are generated by resampling to the specified
    // sampling frequency. The work rate is inversely proportional to the
    // percentage of the bandwidth allocated to the filter transition band.
    //
    // This implementation is based on the paper "A Flexible Sampling-Rate
    // Conversion Method", by J. O. Smith and P. Gosset, or rather on the
    // expanded tutorial on the "Digital Audio Resampling Home Page":
    // http://www-ccrma.stanford.edu/~jos/resample/
    //
    // By building shifted FIR tables with samples according to the
    // sampling frequency, this implementation dramatically reduces the
    // computational effort in the filter convolutions, without any loss
    // of accuracy. The filter convolutions are also vectorizable on
    // current hardware.
    //
    // Further possible optimizations are:
    // * An equiripple filter design could yield a lower filter order, see
    //   http://www.mwrf.com/Articles/ArticleID/7229/7229.html
    // * The Convolution Theorem could be used to bring the complexity of
    //   convolution down from O(n*n) to O(n*log(n)) using the Fast Fourier
    //   Transform, see http://en.wikipedia.org/wiki/Convolution_theorem
    // * Simply resampling in two steps can also yield computational
    //   savings, since the transition band will be wider in the first step
    //   and the required filter order is thus lower in this step.
    //   Laurent Ganier has found the optimal intermediate sampling frequency
    //   to be (via derivation of sum of two steps):
    //     2 * pass_freq + sqrt [ 2 * pass_freq * orig_sample_freq
    //       * (dest_sample_freq - 2 * pass_freq) / dest_sample_freq ]
    //
    // NB! the result of right shifting negative numbers is really
    // implementation dependent in the C++ standard.
    // ----------------------------------------------------------------------------
    RESID_INLINE
    int SID::clockResampleInterpolate(CycleCount& deltaT, short* buf, int n, int interleave)
    {
        int s = 0;

        for(;;)
        {
            CycleCount nextSampleOffset = this->m_sampleOffset + this->m_cyclesPerSample;
            CycleCount deltaTSample = nextSampleOffset >> SID::kFixpShift;
            if(deltaTSample > deltaT)
            {
                break;
            }
            if(s >= n)
            {
                return s;
            }
            for(int i = 0; i < deltaTSample; i++)
            {
                this->clock();
                this->m_sample[this->m_sampleIndex] = this->m_sample[this->m_sampleIndex + SID::kRingSize] = static_cast<short>(this->output());
                ++this->m_sampleIndex;
                this->m_sampleIndex &= 0x3fff;
            }
            deltaT -= deltaTSample;
            this->m_sampleOffset = nextSampleOffset & SID::kFixpMask;

            int firOffset = this->m_sampleOffset * this->m_firRes >> SID::kFixpShift;
            int firOffsetRmd = this->m_sampleOffset * this->m_firRes & SID::kFixpMask;
            short* firStart = this->m_fir + static_cast<std::ptrdiff_t>(firOffset) * this->m_firN;
            short* sampleStart = this->m_sample + this->m_sampleIndex - this->m_firN + SID::kRingSize;

            // Convolution with filter impulse response.
            int v1 = 0;
            for(int j = 0; j < this->m_firN; j++)
            {
                v1 += sampleStart[j] * firStart[j];
            }

            // Use next FIR table, wrap around to first FIR table using
            // previous sample.
            if(++firOffset == this->m_firRes)
            {
                firOffset = 0;
                --sampleStart;
            }
            firStart = this->m_fir + static_cast<std::ptrdiff_t>(firOffset) * this->m_firN;

            // Convolution with filter impulse response.
            int v2 = 0;
            for(int j = 0; j < this->m_firN; j++)
            {
                v2 += sampleStart[j] * firStart[j];
            }

            // Linear interpolation.
            // fir_offset_rmd is equal for all samples, it can thus be factorized out:
            // sum(v1 + rmd*(v2 - v1)) = sum(v1) + rmd*(sum(v2) - sum(v1))
            int v = v1 + (firOffsetRmd * (v2 - v1) >> SID::kFixpShift);

            v >>= SID::kFirShift;

            // Saturated arithmetics to guard against 16 bit sample overflow.
            const int half = 1 << 15;
            if(v >= half)
            {
                v = half - 1;
            }
            else if(v < -half)
            {
                v = -half;
            }

            buf[static_cast<std::ptrdiff_t>(s++) * interleave] = static_cast<short>(v);
        }

        for(int i = 0; i < deltaT; i++)
        {
            this->clock();
            this->m_sample[this->m_sampleIndex] = this->m_sample[this->m_sampleIndex + SID::kRingSize] = static_cast<short>(this->output());
            ++this->m_sampleIndex;
            this->m_sampleIndex &= 0x3fff;
        }
        this->m_sampleOffset -= deltaT << SID::kFixpShift;
        deltaT = 0;
        return s;
    }

    // ----------------------------------------------------------------------------
    // SID clocking with audio sampling - cycle based with audio resampling.
    // ----------------------------------------------------------------------------
    RESID_INLINE
    int SID::clockResampleFast(CycleCount& deltaT, short* buf, int n, int interleave)
    {
        int s = 0;

        for(;;)
        {
            CycleCount nextSampleOffset = this->m_sampleOffset + this->m_cyclesPerSample;
            CycleCount deltaTSample = nextSampleOffset >> SID::kFixpShift;
            if(deltaTSample > deltaT)
            {
                break;
            }
            if(s >= n)
            {
                return s;
            }
            for(int i = 0; i < deltaTSample; i++)
            {
                this->clock();
                this->m_sample[this->m_sampleIndex] = this->m_sample[this->m_sampleIndex + SID::kRingSize] = static_cast<short>(this->output());
                ++this->m_sampleIndex;
                this->m_sampleIndex &= 0x3fff;
            }
            deltaT -= deltaTSample;
            this->m_sampleOffset = nextSampleOffset & SID::kFixpMask;

            int firOffset = this->m_sampleOffset * this->m_firRes >> SID::kFixpShift;
            short* firStart = this->m_fir + static_cast<std::ptrdiff_t>(firOffset) * this->m_firN;
            short* sampleStart = this->m_sample + this->m_sampleIndex - this->m_firN + SID::kRingSize;

            // Convolution with filter impulse response.
            int v = 0;
            for(int j = 0; j < this->m_firN; j++)
            {
                v += sampleStart[j] * firStart[j];
            }

            v >>= SID::kFirShift;

            // Saturated arithmetics to guard against 16 bit sample overflow.
            const int half = 1 << 15;
            if(v >= half)
            {
                v = half - 1;
            }
            else if(v < -half)
            {
                v = -half;
            }

            buf[static_cast<std::ptrdiff_t>(s++) * interleave] = static_cast<short>(v);
        }

        for(int i = 0; i < deltaT; i++)
        {
            this->clock();
            this->m_sample[this->m_sampleIndex] = this->m_sample[this->m_sampleIndex + SID::kRingSize] = static_cast<short>(this->output());
            ++this->m_sampleIndex;
            this->m_sampleIndex &= 0x3fff;
        }
        this->m_sampleOffset -= deltaT << SID::kFixpShift;
        deltaT = 0;
        return s;
    }

} // namespace synthaxes::hw::engine::sid
