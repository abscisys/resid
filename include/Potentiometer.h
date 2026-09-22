#pragma once

#include "siddefs.h"

namespace synthaxes::hw::engine::sid
{

    /// A paddle input (POTX/POTY). Not modeled: the A/D converter is not emulated.
    class Potentiometer
    {
    public:
        /// Reads the POTX/POTY register.
        /// @return Always 0xff, since the potentiometer is not modeled.
        Reg8 readPOT();
    };

} // namespace synthaxes::hw::engine::sid
