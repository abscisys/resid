#include <ExternalFilter.h>

namespace synthaxes::hw::engine::sid
{

    // ----------------------------------------------------------------------------
    // Constructor.
    // ----------------------------------------------------------------------------
    ExternalFilter::ExternalFilter()
    {
        this->reset();
        this->enableFilter(true);
        this->setChipModel(kMos6581);

        // Low-pass:  R = 10kOhm, C = 1000pF; w0l = 1/RC = 1/(1e4*1e-9) = 100000
        // High-pass: R =  1kOhm, C =   10uF; w0h = 1/RC = 1/(1e3*1e-5) =    100
        // Multiply with 1.048576 to facilitate division by 1 000 000 by right-
        // shifting 20 times (2 ^ 20 = 1048576).

        this->m_w0lp = 104858;
        this->m_w0hp = 105;
    }

    // ----------------------------------------------------------------------------
    // Enable filter.
    // ----------------------------------------------------------------------------
    void ExternalFilter::enableFilter(bool enable)
    {
        this->m_enabled = enable;
    }

    // ----------------------------------------------------------------------------
    // Set chip model.
    // ----------------------------------------------------------------------------
    void ExternalFilter::setChipModel(ChipModel model)
    {
        if(model == kMos6581)
        {
            // Maximum mixer DC output level; to be removed if the external
            // filter is turned off: ((wave DC + voice DC)*voices + mixer DC)*volume
            // See voice.cc and filter.cc for an explanation of the values.
            this->m_mixerDc = ((((0x800 - 0x380) + 0x800) * 0xff * 3 - 0xfff * 0xff / 18) >> 7) * 0x0f;
        }
        else
        {
            // No DC offsets in the MOS8580.
            this->m_mixerDc = 0;
        }
    }

    // ----------------------------------------------------------------------------
    // SID reset.
    // ----------------------------------------------------------------------------
    void ExternalFilter::reset()
    {
        // State of filter.
        this->m_vlp = 0;
        this->m_vhp = 0;
        this->m_vo = 0;
    }

} // namespace synthaxes::hw::engine::sid
