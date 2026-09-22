#pragma once

#include "ExternalFilter.h"
#include "Filter.h"
#include "Potentiometer.h"
#include "Voice.h"
#include "siddefs.h"

namespace synthaxes::hw::engine::sid
{

    /// Emulation of a complete MOS6581/8580 SID chip: three voices, the programmable filter and
    /// the C64 external output stage, with register access and audio sample generation.
    ///
    /// Typical use: pick the chip with setChipModel(), the clock and sample rates with
    /// setSamplingParameters(), then alternate write() calls with clock(deltaT, buf, n) to
    /// produce audio.
    class SID
    {
    public:
        /// Constructs a SID with the voices wired for sync/ring modulation (1<-3, 2<-1, 3<-2),
        /// sampling a PAL clock (985248 Hz) at 44.1 kHz with kSampleFast.
        SID();

        /// Releases the resampling buffers.
        ~SID();

        /// Selects the chip revision for all voices and both filters.
        /// @param model Chip revision to emulate.
        void setChipModel(ChipModel model);

        /// Enables or bypasses the SID filter.
        /// @param enable True to filter, false to route every input straight to the mixer.
        void enableFilter(bool enable);

        /// Enables or bypasses the C64 external output filter.
        /// @param enable True to filter, false to only remove the mixer DC offset.
        void enableExternalFilter(bool enable);

        /// Configures sample generation for clock(deltaT, buf, n, interleave).
        ///
        /// For the resampling methods, 125*clockFreq/sampleFreq must stay below 16384 (so at a
        /// ~1 MHz clock the sample rate cannot go below ~8 kHz), and the pass band is limited to
        /// 0.9*sampleFreq/2.
        /// @param clockFreq Chip clock in Hz: 985248 for a PAL C64, 1022730 for NTSC.
        /// @param method How chip output is converted to samples.
        /// @param sampleFreq Output sample rate in Hz.
        /// @param passFreq End of the pass band in Hz for the resampling methods. Negative selects
        ///                 the default: 20 kHz, or 0.9*sampleFreq/2 when that is lower.
        /// @param filterScale Resampling filter gain, in [0.9, 1.0], to avoid clipping.
        /// @return False, with nothing changed, if a resampling constraint is violated.
        bool setSamplingParameters(double clockFreq, SamplingMethod method, double sampleFreq, double passFreq = -1, double filterScale = 0.97);

        /// Changes the sample rate without rebuilding the resampling filter, e.g. to follow a
        /// timer in an emulator. May shift pitch noticeably; intended for interactive use.
        /// @param sampleFreq New output sample rate in Hz.
        void adjustSamplingFrequency(double sampleFreq);

        /// Returns the default spline points that map the FC register to the filter cutoff.
        /// @param[out] points Set to the first interpolation point.
        /// @param[out] count Set to the number of points.
        void fcDefault(const FcPoint*& points, int& count);

        /// Returns a plotter for installing a custom FC-to-cutoff mapping; see Filter::fcPlotter().
        /// @return Plotter targeting the filter's current cutoff table.
        PointPlotter<SoundSample> fcPlotter();

        /// Advances the whole chip by one cycle.
        void clock();

        /// Advances the whole chip by several cycles, stepping the oscillators on every MSB
        /// change so hard sync stays exact.
        /// @param deltaT Number of cycles to advance; values <= 0 do nothing.
        void clock(CycleCount deltaT);

        /// Advances the chip and writes audio samples with the configured sampling method.
        /// Stops early when @p buf is full.
        /// @param[in,out] deltaT Cycles to run; on return, the cycles not yet run (0 unless the
        ///                       buffer filled up).
        /// @param buf Destination for 16-bit samples.
        /// @param n Maximum number of samples to write.
        /// @param interleave Stride between samples in @p buf, e.g. 2 to fill one channel of a
        ///                   stereo buffer.
        /// @return Number of samples written.
        int clock(CycleCount& deltaT, short* buf, int n, int interleave = 1);

        /// Resets the voices, filters and data bus to their power-on state.
        void reset();

        /// Reads a SID register.
        ///
        /// POTX/POTY (0x19, 0x1a), OSC3 (0x1b) and ENV3 (0x1c) are real reads. Every other
        /// (write-only) register returns the last byte written to any register, for 0x2000
        /// cycles; bit fading is not modeled.
        /// @param offset Register offset, 0x00-0x1f.
        /// @return Register value.
        Reg8 read(Reg8 offset);

        /// Writes a SID register.
        /// @param offset Register offset, 0x00-0x18; other offsets only update the data bus.
        /// @param value Value to write.
        void write(Reg8 offset, Reg8 value);

        /// Snapshot of the chip state, for saving and restoring with readState()/writeState().
        /// Per-voice arrays are indexed by voice number (0-2).
        class State
        {
        public:
            /// Constructs the power-on state.
            State();

            char m_sidRegister[0x20];                    ///< Register file, 0x00-0x1f.
            Reg8 m_busValue;                             ///< Last value on the data bus.
            CycleCount m_busValueTtl;                    ///< Cycles until the bus value fades to zero.
            Reg24 m_accumulator[3];                      ///< Oscillator phase accumulators.
            Reg24 m_shiftRegister[3];                    ///< Noise shift registers.
            Reg16 m_rateCounter[3];                      ///< Envelope rate counters.
            Reg16 m_rateCounterPeriod[3];                ///< Envelope rate counter periods.
            Reg16 m_exponentialCounter[3];               ///< Envelope exponential-decay counters.
            Reg16 m_exponentialCounterPeriod[3];         ///< Envelope exponential-decay periods.
            Reg8 m_envelopeCounter[3];                   ///< Envelope levels.
            EnvelopeGenerator::State m_envelopeState[3]; ///< Envelope phases.
            bool m_holdZero[3];                          ///< Whether each envelope is frozen at zero.
        };

        /// Captures the current chip state.
        /// @return Snapshot of registers, oscillators and envelopes.
        State readState();

        /// Restores a chip state captured by readState(), including its register writes.
        /// @param state Snapshot to restore.
        void writeState(const State& state);

        /// Feeds a sample to the external audio input (EXT IN). The signal should be resampled to
        /// the chip clock first to avoid sampling noise.
        /// @param sample 16-bit sample; the caller must keep it within 16 bits.
        void input(int sample);

        /// Reads the audio output (AUDIO OUT) at 16-bit resolution.
        /// @return Output sample, clamped to [-32768, 32767].
        int output();

        /// Reads the audio output at a chosen resolution.
        /// @param bits Output resolution in bits.
        /// @return Output sample, clamped to the signed @p bits range.
        int output(int bits);

    protected:
        static double i0(double x);
        inline int clockFast(CycleCount& deltaT, short* buf, int n, int interleave);
        inline int clockInterpolate(CycleCount& deltaT, short* buf, int n, int interleave);
        inline int clockResampleInterpolate(CycleCount& deltaT, short* buf, int n, int interleave);
        inline int clockResampleFast(CycleCount& deltaT, short* buf, int n, int interleave);

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
