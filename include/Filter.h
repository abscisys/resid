#pragma once

#include "siddefs.h"
#include "spline.h"

namespace synthaxes::hw::engine::sid
{

    /// Programmable SID filter: a state-variable (two-integrator-loop biquad) filter with
    /// high-, band- and low-pass outputs, followed by the mixer and master volume.
    ///
    /// The SID filter is modeled with a two-integrator-loop biquadratic filter,
    /// which has been confirmed by Bob Yannes to be the actual circuit used in
    /// the SID chip.
    ///
    /// Measurements show that excellent emulation of the SID filter is achieved,
    /// except when high resonance is combined with high sustain levels.
    /// In this case the SID op-amps are performing less than ideally and are
    /// causing some peculiar behavior of the SID filter. This however seems to
    /// have more effect on the overall amplitude than on the color of the sound.
    ///
    /// The theory for the filter circuit can be found in "Microelectric Circuits"
    /// by Adel S. Sedra and Kenneth C. Smith.
    /// The circuit is modeled based on the explanation found there except that
    /// an additional inverter is used in the feedback from the bandpass output,
    /// allowing the summer op-amp to operate in single-ended mode. This yields
    /// inverted filter outputs with levels independent of Q, which corresponds with
    /// the results obtained from a real SID.
    ///
    /// We have been able to model the summer and the two integrators of the circuit
    /// to form components of an IIR filter.
    /// Vhp is the output of the summer, Vbp is the output of the first integrator,
    /// and Vlp is the output of the second integrator in the filter circuit.
    ///
    /// According to Bob Yannes, the active stages of the SID filter are not really
    /// op-amps. Rather, simple NMOS inverters are used. By biasing an inverter
    /// into its region of quasi-linear operation using a feedback resistor from
    /// input to output, a MOS inverter can be made to act like an op-amp for
    /// small signals centered around the switching threshold.
    ///
    /// Qualified guesses at SID filter schematics are depicted below.
    ///
    /// @verbatim
    /// SID filter
    /// ----------
    ///
    ///     -----------------------------------------------
    ///    |                                               |
    ///    |            ---Rq--                            |
    ///    |           |       |                           |
    ///    |  ------------<A]-----R1---------              |
    ///    | |                               |             |
    ///    | |                        ---C---|      ---C---|
    ///    | |                       |       |     |       |
    ///    |  --R1--    ---R1--      |---Rs--|     |---Rs--|
    ///    |        |  |       |     |       |     |       |
    ///     ----R1--|-----[A>--|--R-----[A>--|--R-----[A>--|
    ///             |          |             |             |
    /// vi -----R1--           |             |             |
    ///
    ///                       vhp           vbp           vlp
    ///
    ///
    /// vi  - input voltage
    /// vhp - highpass output
    /// vbp - bandpass output
    /// vlp - lowpass output
    /// [A> - op-amp
    /// R1  - summer resistor
    /// Rq  - resistor array controlling resonance (4 resistors)
    /// R   - NMOS FET voltage controlled resistor controlling cutoff frequency
    /// Rs  - shunt resitor
    /// C   - capacitor
    ///
    ///
    ///
    /// SID integrator
    /// --------------
    ///
    ///                                   V+
    ///
    ///                                   |
    ///                                   |
    ///                              -----|
    ///                             |     |
    ///                             | ||--
    ///                              -||
    ///                   ---C---     ||->
    ///                  |       |        |
    ///                  |---Rs-----------|---- vo
    ///                  |                |
    ///                  |            ||--
    /// vi ----     -----|------------||
    ///        |   ^     |            ||->
    ///        |___|     |                |
    ///        -----     |                |
    ///          |       |                |
    ///          |---R2--                 |
    ///          |
    ///          R1                       V-
    ///          |
    ///          |
    ///
    ///          Vw
    /// @endverbatim
    class Filter
    {
    public:
        /// Constructs an enabled filter configured for a MOS6581 and builds the FC-to-cutoff
        /// tables for both chip revisions.
        Filter();

        /// Enables or bypasses the filter. When bypassed, all inputs go straight to the mixer.
        /// @param enable True to filter, false to bypass.
        void enableFilter(bool enable);

        /// Selects the chip revision, which sets the cutoff curve and the mixer DC offset.
        /// @param model Chip revision to emulate.
        void setChipModel(ChipModel model);

        /// Advances the filter by one cycle.
        /// @param voice1 Output of voice 1 (20 bits).
        /// @param voice2 Output of voice 2 (20 bits).
        /// @param voice3 Output of voice 3 (20 bits).
        /// @param extIn External audio input (EXT IN).
        inline void clock(SoundSample voice1, SoundSample voice2, SoundSample voice3, SoundSample extIn);

        /// Advances the filter by several cycles with constant inputs.
        /// @param deltaT Number of cycles to advance.
        /// @param voice1 Output of voice 1 (20 bits).
        /// @param voice2 Output of voice 2 (20 bits).
        /// @param voice3 Output of voice 3 (20 bits).
        /// @param extIn External audio input (EXT IN).
        inline void clock(CycleCount deltaT, SoundSample voice1, SoundSample voice2, SoundSample voice3, SoundSample extIn);

        /// Resets the registers and filter state to their power-on values.
        void reset();

        /// Writes the low 3 bits of the 11-bit cutoff frequency register.
        /// @param fcLo FC LO value.
        void writeFcLo(Reg8 fcLo);

        /// Writes the high 8 bits of the 11-bit cutoff frequency register.
        /// @param fcHi FC HI value.
        void writeFcHi(Reg8 fcHi);

        /// Writes the RES/FILT register.
        /// @param resFilt Resonance in the high nibble; in the low nibble, which inputs (voice 1-3,
        ///                EXT IN) are routed through the filter.
        void writeResFilt(Reg8 resFilt);

        /// Writes the MODE/VOL register.
        /// @param modeVol Voice 3 off (bit 7), high/band/low-pass select (bits 6-4) and master
        ///                volume (low nibble).
        void writeModeVol(Reg8 modeVol);

        /// Mixed output: the unfiltered inputs plus the selected filter outputs, scaled by the
        /// master volume.
        /// @return Mixer output sample.
        SoundSample output();

        /// Returns the spline interpolation points that map the FC register to the cutoff
        /// frequency for the current chip revision.
        /// @param[out] points Set to the first interpolation point.
        /// @param[out] count Set to the number of points.
        void fcDefault(const FcPoint*& points, int& count);

        /// Returns a plotter that writes into the active FC-to-cutoff table, for installing a
        /// custom mapping with interpolate(p, p + n - 1, filter.fcPlotter(), 1.0). The x range of
        /// the points must be [0, 2047], and the end points must be repeated since they are not
        /// interpolated.
        /// @return Plotter targeting the current cutoff table.
        PointPlotter<SoundSample> fcPlotter();

    protected:
        void setW0();
        void setQ();

        // Filter enabled.
        bool m_enabled;

        // Filter cutoff frequency.
        Reg12 m_fc;

        // Filter resonance.
        Reg8 m_res;

        // Selects which inputs to route through filter.
        Reg8 m_filt;

        // Switch voice 3 off.
        Reg8 m_voice3off;

        // Highpass, bandpass, and lowpass filter modes.
        Reg8 m_hpBpLp;

        // Output master volume.
        Reg4 m_vol;

        // Mixer DC offset.
        SoundSample m_mixerDc;

        // State of filter.
        SoundSample m_vhp; // highpass
        SoundSample m_vbp; // bandpass
        SoundSample m_vlp; // lowpass
        SoundSample m_vnf; // not filtered

        // Cutoff frequency, resonance.
        SoundSample m_w0, m_w0Ceil1, m_w0CeilDt;
        SoundSample m_1024DivQ;

        // Cutoff frequency tables.
        // FC is an 11 bit register.
        SoundSample m_f06581[2048];
        SoundSample m_f08580[2048];
        SoundSample* m_f0;
        static FcPoint f0Points6581[];
        static FcPoint f0Points8580[];
        FcPoint* m_f0Points;
        int m_f0Count;

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
    inline void Filter::clock(SoundSample voice1, SoundSample voice2, SoundSample voice3, SoundSample extIn)
    {
        // Scale each voice down from 20 to 13 bits.
        voice1 >>= 7;
        voice2 >>= 7;

        // NB! Voice 3 is not silenced by voice3off if it is routed through
        // the filter.
        if(this->m_voice3off && !(this->m_filt & 0x04))
        {
            voice3 = 0;
        }
        else
        {
            voice3 >>= 7;
        }

        extIn >>= 7;

        // This is handy for testing.
        if(!this->m_enabled)
        {
            this->m_vnf = voice1 + voice2 + voice3 + extIn;
            this->m_vhp = this->m_vbp = this->m_vlp = 0;
            return;
        }

        // Route voices into or around filter.
        // The code below is expanded to a switch for faster execution.
        // (filt1 ? Vi : Vnf) += voice1;
        // (filt2 ? Vi : Vnf) += voice2;
        // (filt3 ? Vi : Vnf) += voice3;

        SoundSample vi;

        switch(this->m_filt)
        {
        default:
        case 0x0:
            vi = 0;
            this->m_vnf = voice1 + voice2 + voice3 + extIn;
            break;
        case 0x1:
            vi = voice1;
            this->m_vnf = voice2 + voice3 + extIn;
            break;
        case 0x2:
            vi = voice2;
            this->m_vnf = voice1 + voice3 + extIn;
            break;
        case 0x3:
            vi = voice1 + voice2;
            this->m_vnf = voice3 + extIn;
            break;
        case 0x4:
            vi = voice3;
            this->m_vnf = voice1 + voice2 + extIn;
            break;
        case 0x5:
            vi = voice1 + voice3;
            this->m_vnf = voice2 + extIn;
            break;
        case 0x6:
            vi = voice2 + voice3;
            this->m_vnf = voice1 + extIn;
            break;
        case 0x7:
            vi = voice1 + voice2 + voice3;
            this->m_vnf = extIn;
            break;
        case 0x8:
            vi = extIn;
            this->m_vnf = voice1 + voice2 + voice3;
            break;
        case 0x9:
            vi = voice1 + extIn;
            this->m_vnf = voice2 + voice3;
            break;
        case 0xa:
            vi = voice2 + extIn;
            this->m_vnf = voice1 + voice3;
            break;
        case 0xb:
            vi = voice1 + voice2 + extIn;
            this->m_vnf = voice3;
            break;
        case 0xc:
            vi = voice3 + extIn;
            this->m_vnf = voice1 + voice2;
            break;
        case 0xd:
            vi = voice1 + voice3 + extIn;
            this->m_vnf = voice2;
            break;
        case 0xe:
            vi = voice2 + voice3 + extIn;
            this->m_vnf = voice1;
            break;
        case 0xf:
            vi = voice1 + voice2 + voice3 + extIn;
            this->m_vnf = 0;
            break;
        }

        // delta_t = 1 is converted to seconds given a 1MHz clock by dividing
        // with 1 000 000.

        // Calculate filter outputs.
        // Vhp = Vbp/Q - Vlp - Vi;
        // dVbp = -w0*Vhp*dt;
        // dVlp = -w0*Vbp*dt;

        SoundSample dVbp = (this->m_w0Ceil1 * this->m_vhp >> 20);
        SoundSample dVlp = (this->m_w0Ceil1 * this->m_vbp >> 20);
        this->m_vbp -= dVbp;
        this->m_vlp -= dVlp;
        this->m_vhp = (this->m_vbp * this->m_1024DivQ >> 10) - this->m_vlp - vi;
    }

    // ----------------------------------------------------------------------------
    // SID clocking - delta_t cycles.
    // ----------------------------------------------------------------------------
    inline void Filter::clock(CycleCount deltaT, SoundSample voice1, SoundSample voice2, SoundSample voice3, SoundSample extIn)
    {
        // Scale each voice down from 20 to 13 bits.
        voice1 >>= 7;
        voice2 >>= 7;

        // NB! Voice 3 is not silenced by voice3off if it is routed through
        // the filter.
        if(this->m_voice3off && !(this->m_filt & 0x04))
        {
            voice3 = 0;
        }
        else
        {
            voice3 >>= 7;
        }

        extIn >>= 7;

        // Enable filter on/off.
        // This is not really part of SID, but is useful for testing.
        // On slow CPUs it may be necessary to bypass the filter to lower the CPU
        // load.
        if(!this->m_enabled)
        {
            this->m_vnf = voice1 + voice2 + voice3 + extIn;
            this->m_vhp = this->m_vbp = this->m_vlp = 0;
            return;
        }

        // Route voices into or around filter.
        // The code below is expanded to a switch for faster execution.
        // (filt1 ? Vi : Vnf) += voice1;
        // (filt2 ? Vi : Vnf) += voice2;
        // (filt3 ? Vi : Vnf) += voice3;

        SoundSample vi;

        switch(this->m_filt)
        {
        default:
        case 0x0:
            vi = 0;
            this->m_vnf = voice1 + voice2 + voice3 + extIn;
            break;
        case 0x1:
            vi = voice1;
            this->m_vnf = voice2 + voice3 + extIn;
            break;
        case 0x2:
            vi = voice2;
            this->m_vnf = voice1 + voice3 + extIn;
            break;
        case 0x3:
            vi = voice1 + voice2;
            this->m_vnf = voice3 + extIn;
            break;
        case 0x4:
            vi = voice3;
            this->m_vnf = voice1 + voice2 + extIn;
            break;
        case 0x5:
            vi = voice1 + voice3;
            this->m_vnf = voice2 + extIn;
            break;
        case 0x6:
            vi = voice2 + voice3;
            this->m_vnf = voice1 + extIn;
            break;
        case 0x7:
            vi = voice1 + voice2 + voice3;
            this->m_vnf = extIn;
            break;
        case 0x8:
            vi = extIn;
            this->m_vnf = voice1 + voice2 + voice3;
            break;
        case 0x9:
            vi = voice1 + extIn;
            this->m_vnf = voice2 + voice3;
            break;
        case 0xa:
            vi = voice2 + extIn;
            this->m_vnf = voice1 + voice3;
            break;
        case 0xb:
            vi = voice1 + voice2 + extIn;
            this->m_vnf = voice3;
            break;
        case 0xc:
            vi = voice3 + extIn;
            this->m_vnf = voice1 + voice2;
            break;
        case 0xd:
            vi = voice1 + voice3 + extIn;
            this->m_vnf = voice2;
            break;
        case 0xe:
            vi = voice2 + voice3 + extIn;
            this->m_vnf = voice1;
            break;
        case 0xf:
            vi = voice1 + voice2 + voice3 + extIn;
            this->m_vnf = 0;
            break;
        }

        // Maximum delta cycles for the filter to work satisfactorily under current
        // cutoff frequency and resonance constraints is approximately 8.
        CycleCount deltaTFlt = 8;

        while(deltaT)
        {
            if(deltaT < deltaTFlt)
            {
                deltaTFlt = deltaT;
            }

            // delta_t is converted to seconds given a 1MHz clock by dividing
            // with 1 000 000. This is done in two operations to avoid integer
            // multiplication overflow.

            // Calculate filter outputs.
            // Vhp = Vbp/Q - Vlp - Vi;
            // dVbp = -w0*Vhp*dt;
            // dVlp = -w0*Vbp*dt;
            SoundSample w0DeltaT = this->m_w0CeilDt * deltaTFlt >> 6;

            SoundSample dVbp = (w0DeltaT * this->m_vhp >> 14);
            SoundSample dVlp = (w0DeltaT * this->m_vbp >> 14);
            this->m_vbp -= dVbp;
            this->m_vlp -= dVlp;
            this->m_vhp = (this->m_vbp * this->m_1024DivQ >> 10) - this->m_vlp - vi;

            deltaT -= deltaTFlt;
        }
    }

    // ----------------------------------------------------------------------------
    // SID audio output (20 bits).
    // ----------------------------------------------------------------------------
    inline SoundSample Filter::output()
    {
        // This is handy for testing.
        if(!this->m_enabled)
        {
            return (this->m_vnf + this->m_mixerDc) * static_cast<SoundSample>(this->m_vol);
        }

        // Mix highpass, bandpass, and lowpass outputs. The sum is not
        // weighted, this can be confirmed by sampling sound output for
        // e.g. bandpass, lowpass, and bandpass+lowpass from a SID chip.

        // The code below is expanded to a switch for faster execution.
        // if (hp) Vf += Vhp;
        // if (bp) Vf += Vbp;
        // if (lp) Vf += Vlp;

        SoundSample vf;

        switch(this->m_hpBpLp)
        {
        default:
        case 0x0:
            vf = 0;
            break;
        case 0x1:
            vf = this->m_vlp;
            break;
        case 0x2:
            vf = this->m_vbp;
            break;
        case 0x3:
            vf = this->m_vlp + this->m_vbp;
            break;
        case 0x4:
            vf = this->m_vhp;
            break;
        case 0x5:
            vf = this->m_vlp + this->m_vhp;
            break;
        case 0x6:
            vf = this->m_vbp + this->m_vhp;
            break;
        case 0x7:
            vf = this->m_vlp + this->m_vbp + this->m_vhp;
            break;
        }

        // Sum non-filtered and filtered output.
        // Multiply the sum with volume.
        return (this->m_vnf + vf + this->m_mixerDc) * static_cast<SoundSample>(this->m_vol);
    }

} // namespace synthaxes::hw::engine::sid
