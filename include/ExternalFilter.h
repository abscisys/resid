#pragma once

#include "siddefs.h"

namespace synthaxes::hw::engine::sid
{

    /// The Commodore 64 audio output stage that follows the SID chip.
    ///
    /// It consists of two STC networks, a low-pass filter with 3-dB frequency 16kHz followed by a
    /// high-pass filter with 3-dB frequency 16Hz (the latter provided an audio equipment input
    /// impedance of 1kOhm). The STC networks are connected with a BJT supposedly meant to act as a
    /// unity gain buffer, which is not really how it works. A more elaborate model would include
    /// the BJT, however DC circuit analysis yields BJT base-emitter and emitter-base impedances
    /// sufficiently low to produce additional low-pass and high-pass 3dB-frequencies in the order
    /// of hundreds of kHz. This calls for a sampling frequency of several MHz, which is far too
    /// high for practical use.
    class ExternalFilter
    {
    public:
        /// Constructs an enabled filter configured for a MOS6581.
        ExternalFilter();

        /// Enables or bypasses the filter. When bypassed, only the mixer DC offset is removed.
        /// @param enable True to filter, false to bypass.
        void enableFilter(bool enable);

        /// Selects the chip revision, which sets the mixer DC offset to remove.
        /// @param model Chip revision to emulate.
        void setChipModel(ChipModel model);

        /// Advances the filter by one cycle.
        /// @param vi Filter input: the SID mixer output.
        RESID_INLINE void clock(SoundSample vi);

        /// Advances the filter by several cycles with a constant input.
        /// @param deltaT Number of cycles to advance.
        /// @param vi Filter input: the SID mixer output.
        RESID_INLINE void clock(CycleCount deltaT, SoundSample vi);

        /// Clears the filter state.
        void reset();

        /// Filtered audio output.
        /// @return Output sample (about 20 bits).
        RESID_INLINE SoundSample output();

    protected:
        // Filter enabled.
        bool m_enabled;

        // Maximum mixer DC offset.
        SoundSample m_mixerDc;

        // State of filters.
        SoundSample m_vlp; // lowpass
        SoundSample m_vhp; // highpass
        SoundSample m_vo;

        // Cutoff frequencies.
        SoundSample m_w0lp;
        SoundSample m_w0hp;

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
    void ExternalFilter::clock(SoundSample vi)
    {
        // This is handy for testing.
        if(!this->m_enabled)
        {
            // Remove maximum DC level since there is no filter to do it.
            this->m_vlp = this->m_vhp = 0;
            this->m_vo = vi - this->m_mixerDc;
            return;
        }

        // delta_t is converted to seconds given a 1MHz clock by dividing
        // with 1 000 000.

        // Calculate filter outputs.
        // Vo  = Vlp - Vhp;
        // Vlp = Vlp + w0lp*(Vi - Vlp)*delta_t;
        // Vhp = Vhp + w0hp*(Vlp - Vhp)*delta_t;

        SoundSample dVlp = (this->m_w0lp >> 8) * (vi - this->m_vlp) >> 12;
        SoundSample dVhp = this->m_w0hp * (this->m_vlp - this->m_vhp) >> 20;
        this->m_vo = this->m_vlp - this->m_vhp;
        this->m_vlp += dVlp;
        this->m_vhp += dVhp;
    }

    // ----------------------------------------------------------------------------
    // SID clocking - delta_t cycles.
    // ----------------------------------------------------------------------------
    RESID_INLINE
    void ExternalFilter::clock(CycleCount deltaT, SoundSample vi)
    {
        // This is handy for testing.
        if(!this->m_enabled)
        {
            // Remove maximum DC level since there is no filter to do it.
            this->m_vlp = this->m_vhp = 0;
            this->m_vo = vi - this->m_mixerDc;
            return;
        }

        // Maximum delta cycles for the external filter to work satisfactorily
        // is approximately 8.
        CycleCount deltaTFlt = 8;

        while(deltaT)
        {
            if(deltaT < deltaTFlt)
            {
                deltaTFlt = deltaT;
            }

            // delta_t is converted to seconds given a 1MHz clock by dividing
            // with 1 000 000.

            // Calculate filter outputs.
            // Vo  = Vlp - Vhp;
            // Vlp = Vlp + w0lp*(Vi - Vlp)*delta_t;
            // Vhp = Vhp + w0hp*(Vlp - Vhp)*delta_t;

            SoundSample dVlp = (this->m_w0lp * deltaTFlt >> 8) * (vi - this->m_vlp) >> 12;
            SoundSample dVhp = this->m_w0hp * deltaTFlt * (this->m_vlp - this->m_vhp) >> 20;
            this->m_vo = this->m_vlp - this->m_vhp;
            this->m_vlp += dVlp;
            this->m_vhp += dVhp;

            deltaT -= deltaTFlt;
        }
    }

    // ----------------------------------------------------------------------------
    // Audio output (19.5 bits).
    // ----------------------------------------------------------------------------
    RESID_INLINE
    SoundSample ExternalFilter::output()
    {
        return this->m_vo;
    }

} // namespace synthaxes::hw::engine::sid
