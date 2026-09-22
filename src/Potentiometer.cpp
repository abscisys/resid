#include <Potentiometer.h>

namespace synthaxes::hw::engine::sid
{

    Reg8 Potentiometer::readPOT()
    {
        // NB! Not modeled.
        return 0xff;
    }

} // namespace synthaxes::hw::engine::sid
