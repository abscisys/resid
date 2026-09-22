#pragma once

#include "EnvelopeGenerator.h"
#include "WaveformGenerator.h"
#include "siddefs.h"

namespace synthaxes::hw::engine::sid
{

    /// One of the three SID voices: an oscillator (WaveformGenerator) whose output is amplitude
    /// modulated by an envelope (EnvelopeGenerator) through the multiplying D/A converter.
    class Voice
    {
    public:
        /// Constructs a voice configured as a MOS6581.
        Voice();

        /// Selects the chip revision, which sets the waveform tables and the D/A DC offsets.
        /// @param model Chip revision to emulate.
        void setChipModel(ChipModel model);

        /// Sets the voice whose oscillator drives this voice's hard sync and ring modulation.
        /// @param source Voice providing the sync/ring-modulation signal.
        void setSyncSource(Voice* source);

        /// Resets the oscillator and envelope to their power-on state.
        void reset();

        /// Writes the voice control register, shared by the oscillator (waveform, test, ring
        /// modulation, sync) and the envelope (gate).
        /// @param control Value written to the CONTROL register.
        void writeControlReg(Reg8 control);

        /// Amplitude-modulated waveform output: oscillator times envelope, plus the D/A DC offset.
        /// @return Voice sample, ideally in [-2048*255, 2047*255].
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
