#pragma once

#include "siddefs.h"

#include <cstdint>

namespace synthaxes::hw::engine::sid
{

    /// ADSR envelope generator of one voice.
    ///
    /// A 15 bit counter is used to implement the envelope rates, in effect dividing the clock to
    /// the envelope counter by the currently selected rate period. In addition, another counter is
    /// used to implement the exponential envelope decay, in effect further dividing the clock to
    /// the envelope counter. The period of this counter is set to 1, 2, 4, 8, 16, 30 at the
    /// envelope counter values 255, 93, 54, 26, 14, 6, respectively.
    class EnvelopeGenerator
    {
    public:
        /// Constructs an envelope in its reset state (release, counter frozen at zero).
        EnvelopeGenerator();

        /// Phase of the ADSR envelope.
        enum State : std::uint8_t
        {
            kAttack,       ///< Counting up towards 0xff at the attack rate; entered when the gate bit is set.
            kDecaySustain, ///< Counting down at the decay rate until the sustain level is reached.
            kRelease       ///< Counting down at the release rate; entered when the gate bit is cleared.
        };

        /// Advances the envelope by one cycle.
        RESID_INLINE void clock();

        /// Advances the envelope by several cycles.
        /// @param deltaT Number of cycles to advance.
        RESID_INLINE void clock(CycleCount deltaT);

        /// Resets the envelope to its power-on state.
        void reset();

        /// Writes the voice control register; only the gate bit (bit 0) is used here.
        /// Setting the gate starts the attack, clearing it starts the release.
        /// @param control Value written to the CONTROL register.
        void writeControlReg(Reg8 control);

        /// Writes the ATTACK/DECAY register.
        /// @param attackDecay Attack rate in the high nibble, decay rate in the low nibble.
        void writeAttackDecay(Reg8 attackDecay);

        /// Writes the SUSTAIN/RELEASE register.
        /// @param sustainRelease Sustain level in the high nibble, release rate in the low nibble.
        void writeSustainRelease(Reg8 sustainRelease);

        /// Reads the envelope as seen through the ENV3 register.
        /// @return Current 8-bit envelope value.
        Reg8 readENV();

        /// Current envelope level, used to scale the voice output.
        /// @return 8-bit envelope value.
        RESID_INLINE Reg8 output();

    protected:
        Reg16 m_rateCounter;
        Reg16 m_ratePeriod;
        Reg8 m_exponentialCounter;
        Reg8 m_exponentialCounterPeriod;
        Reg8 m_envelopeCounter;
        bool m_holdZero;

        Reg4 m_attack;
        Reg4 m_decay;
        Reg4 m_sustain;
        Reg4 m_release;

        Reg8 m_gate;

        State m_state;

        // Lookup table to convert from attack, decay, or release value to rate
        // counter period.
        static Reg16 rateCounterPeriod[];

        // The 16 selectable sustain levels.
        static Reg8 sustainLevel[];

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
    void EnvelopeGenerator::clock()
    {
        // Check for ADSR delay bug.
        // If the rate counter comparison value is set below the current value of the
        // rate counter, the counter will continue counting up until it wraps around
        // to zero at 2^15 = 0x8000, and then count rate_period - 1 before the
        // envelope can finally be stepped.
        // This has been verified by sampling ENV3.
        //
        if(++this->m_rateCounter & 0x8000)
        {
            ++this->m_rateCounter &= 0x7fff;
        }

        if(this->m_rateCounter != this->m_ratePeriod)
        {
            return;
        }

        this->m_rateCounter = 0;

        // The first envelope step in the attack state also resets the exponential
        // counter. This has been verified by sampling ENV3.
        //
        if(this->m_state == kAttack || ++this->m_exponentialCounter == this->m_exponentialCounterPeriod)
        {
            this->m_exponentialCounter = 0;

            // Check whether the envelope counter is frozen at zero.
            if(this->m_holdZero)
            {
                return;
            }

            switch(this->m_state)
            {
            case kAttack:
                // The envelope counter can flip from 0xff to 0x00 by changing state to
                // release, then to attack. The envelope counter is then frozen at
                // zero; to unlock this situation the state must be changed to release,
                // then to attack. This has been verified by sampling ENV3.
                //
                ++this->m_envelopeCounter &= 0xff;
                if(this->m_envelopeCounter == 0xff)
                {
                    this->m_state = kDecaySustain;
                    this->m_ratePeriod = EnvelopeGenerator::rateCounterPeriod[this->m_decay];
                }
                break;
            case kDecaySustain:
                if(this->m_envelopeCounter != EnvelopeGenerator::sustainLevel[this->m_sustain])
                {
                    --this->m_envelopeCounter;
                }
                break;
            case kRelease:
                // The envelope counter can flip from 0x00 to 0xff by changing state to
                // attack, then to release. The envelope counter will then continue
                // counting down in the release state.
                // This has been verified by sampling ENV3.
                // NB! The operation below requires two's complement integer.
                //
                --this->m_envelopeCounter &= 0xff;
                break;
            }

            // Check for change of exponential counter period.
            switch(this->m_envelopeCounter)
            {
            case 0xff:
                this->m_exponentialCounterPeriod = 1;
                break;
            case 0x5d:
                this->m_exponentialCounterPeriod = 2;
                break;
            case 0x36:
                this->m_exponentialCounterPeriod = 4;
                break;
            case 0x1a:
                this->m_exponentialCounterPeriod = 8;
                break;
            case 0x0e:
                this->m_exponentialCounterPeriod = 16;
                break;
            case 0x06:
                this->m_exponentialCounterPeriod = 30;
                break;
            case 0x00:
                this->m_exponentialCounterPeriod = 1;

                // When the envelope counter is changed to zero, it is frozen at zero.
                // This has been verified by sampling ENV3.
                this->m_holdZero = true;
                break;
            default:
                break;
            }
        }
    }

    // ----------------------------------------------------------------------------
    // SID clocking - delta_t cycles.
    // ----------------------------------------------------------------------------
    RESID_INLINE
    void EnvelopeGenerator::clock(CycleCount deltaT)
    {
        // Check for ADSR delay bug.
        // If the rate counter comparison value is set below the current value of the
        // rate counter, the counter will continue counting up until it wraps around
        // to zero at 2^15 = 0x8000, and then count rate_period - 1 before the
        // envelope can finally be stepped.
        // This has been verified by sampling ENV3.
        //

        // NB! This requires two's complement integer.
        int rateStep = static_cast<int>(this->m_ratePeriod - this->m_rateCounter);
        if(rateStep <= 0)
        {
            rateStep += 0x7fff;
        }

        while(deltaT)
        {
            if(deltaT < rateStep)
            {
                this->m_rateCounter += deltaT;
                if(this->m_rateCounter & 0x8000)
                {
                    ++this->m_rateCounter &= 0x7fff;
                }
                return;
            }

            this->m_rateCounter = 0;
            deltaT -= rateStep;

            // The first envelope step in the attack state also resets the exponential
            // counter. This has been verified by sampling ENV3.
            //
            if(this->m_state == kAttack || ++this->m_exponentialCounter == this->m_exponentialCounterPeriod)
            {
                this->m_exponentialCounter = 0;

                // Check whether the envelope counter is frozen at zero.
                if(this->m_holdZero)
                {
                    rateStep = static_cast<int>(this->m_ratePeriod);
                    continue;
                }

                switch(this->m_state)
                {
                case kAttack:
                    // The envelope counter can flip from 0xff to 0x00 by changing state to
                    // release, then to attack. The envelope counter is then frozen at
                    // zero; to unlock this situation the state must be changed to release,
                    // then to attack. This has been verified by sampling ENV3.
                    //
                    ++this->m_envelopeCounter &= 0xff;
                    if(this->m_envelopeCounter == 0xff)
                    {
                        this->m_state = kDecaySustain;
                        this->m_ratePeriod = EnvelopeGenerator::rateCounterPeriod[this->m_decay];
                    }
                    break;
                case kDecaySustain:
                    if(this->m_envelopeCounter != EnvelopeGenerator::sustainLevel[this->m_sustain])
                    {
                        --this->m_envelopeCounter;
                    }
                    break;
                case kRelease:
                    // The envelope counter can flip from 0x00 to 0xff by changing state to
                    // attack, then to release. The envelope counter will then continue
                    // counting down in the release state.
                    // This has been verified by sampling ENV3.
                    // NB! The operation below requires two's complement integer.
                    //
                    --this->m_envelopeCounter &= 0xff;
                    break;
                }

                // Check for change of exponential counter period.
                switch(this->m_envelopeCounter)
                {
                case 0xff:
                    this->m_exponentialCounterPeriod = 1;
                    break;
                case 0x5d:
                    this->m_exponentialCounterPeriod = 2;
                    break;
                case 0x36:
                    this->m_exponentialCounterPeriod = 4;
                    break;
                case 0x1a:
                    this->m_exponentialCounterPeriod = 8;
                    break;
                case 0x0e:
                    this->m_exponentialCounterPeriod = 16;
                    break;
                case 0x06:
                    this->m_exponentialCounterPeriod = 30;
                    break;
                case 0x00:
                    this->m_exponentialCounterPeriod = 1;

                    // When the envelope counter is changed to zero, it is frozen at zero.
                    // This has been verified by sampling ENV3.
                    this->m_holdZero = true;
                    break;
                default:
                    break;
                }
            }

            rateStep = static_cast<int>(this->m_ratePeriod);
        }
    }

    // ----------------------------------------------------------------------------
    // Read the envelope generator output.
    // ----------------------------------------------------------------------------
    RESID_INLINE
    Reg8 EnvelopeGenerator::output()
    {
        return this->m_envelopeCounter;
    }

} // namespace synthaxes::hw::engine::sid
