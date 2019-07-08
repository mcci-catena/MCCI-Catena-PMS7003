/*

Module: Catena-PMS7003Hal.h

Function:
    The PMS7003 library: cPMS7003Hal

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

#ifndef _Catena_PMS7003Hal_h_
# define _Catena_PMS7003Hal_h_

#pragma once

#include <Arduino.h>
#include <Catena_PollableInterface.h>
#include <cstdint>

namespace McciCatenaPMS7003 {

/****************************************************************************\
|
|   HAL for PMS7003 sensor
|
\****************************************************************************/

class cPMS7003Hal
    {
public:
    cPMS7003Hal() {};
    enum class PinState
            {
            Zero = 0,
            One = 1,
            HighZ = -1
            };
    static constexpr bool isLowZ(PinState v)
        {
        return v == PinState::Zero || v == PinState::One;
        }

    // start operation
    virtual bool begin() = 0;

    // end operation
    virtual void end() = 0;

    // set 5v and return number of millis() to delay before it's effective.
    virtual std::uint32_t set5v(bool fEnable) = 0;

    // get 5v enable state.
    virtual bool get5v(void) = 0;

    // suspend
    virtual void suspend() = 0;

    // resume -- returns number of millis before can start operating.
    virtual std::uint32_t resume() = 0;

    // set reset pin to state (if possible)
    virtual void setReset(PinState) = 0;

    // return current state of reset pin
    virtual PinState getReset() = 0;

    // set mode pin to state (if possible)
    virtual void setMode(PinState) = 0;

    // return current state of mode pin
    virtual PinState getMode() = 0;

    // register an object to be polled
    virtual void registerPollableObject(McciCatena::cPollableObject *);

    // print a message
    virtual void printf(const char *fmt, ...)
            /* `this` counts as as arg 1, so `fmt` is arg 2 */
            __attribute__((__format__(__printf__, 2, 3))) = 0;

    // determine whether a print is enabled
    virtual bool isEnabled(std::uint32_t mask) const = 0;
    };

} // namespace McciCatenaPMS7003

#endif // defined _Catena_PMS7003Hal_h_
