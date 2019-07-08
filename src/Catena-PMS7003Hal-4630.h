/*

Module: Catena-PMS7003Hal-4630.h

Function:
    The PMS7003 library

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

#ifndef _Catena_PMS7003Hal_4630_h_
# define _Catena_PMS7003Hal_4630_h_

# ifdef ARDUINO_MCCI_CATENA_4630

#pragma once

#include <Catena4630.h>
#include <Catena-PMS7003Hal.h>
#include <Catena-PMS7003.h>
#include <Catena_FSM.h>
#include <Catena_PollableInterface.h>

/****************************************************************************\
|
|   HAL for PMS7003 sensor
|
\****************************************************************************/

class cPMS7003Hal_4630 : public cPMS7003Hal
    {
private:
    static constexpr int kVddPin = D12;
    static constexpr int kResetPin = D10;
    static constexpr int kModePin = D11;
    static constexpr std::uint32_t kPowerUpDelayMs = 500;
    static constexpr std::uint32_t kPowerDownDelayMs = 100;

    // internal routine, set pin true or false, checking for
    // valid pin number first.
    static inline void pinWrite(int pin, bool value)
        {
        if (pin >= 0)
            digitalWrite(pin, value);
        }

    // internal routine, set pin operating mode, checking for
    // valid pin number first.
    static inline void setPinMode(int pin, int mode)
        {
        if (pin >= 0)
            pinMode(pin, mode);
        }

public:
    // constructor
    cPMS7003Hal_4630(McciCatena::Catena4630 &aCatena, std::uint32_t debugMask)
        : m_Catena(aCatena)
        , m_debugMask(debugMask)
        {}

    virtual bool begin() override
        {
        if (this->isEnabled(cPMS7003::DebugFlags::kTrace))
            this->printf("hal begin\n");
        pinWrite(kVddPin, 0);
        setPinMode(kVddPin, OUTPUT);

        this->setReset(PinState::HighZ);
        this->setMode(PinState::HighZ);
        return true;
        }

    virtual void end() override
        {
        if (this->isEnabled(cPMS7003::DebugFlags::kTrace))
            this->printf("hal end\n");
        pinWrite(kVddPin, 0);
        setPinMode(kVddPin, INPUT);
        this->setReset(PinState::HighZ);
        this->setMode(PinState::HighZ);
        }

    virtual std::uint32_t set5v(bool fEnable) override
        {
        if (this->m_f5vState == fEnable)
            return 0;

        if (this->isEnabled(cPMS7003::DebugFlags::kTrace))
            this->printf("set5v: %u\n", unsigned(fEnable));

        this->m_f5vState = fEnable;
        pinWrite(kVddPin, fEnable);
        if (! fEnable)
            {
            this->setReset(PinState::HighZ);
            this->setMode(PinState::HighZ);
            }
        else
            {
            this->setReset(PinState::Zero);
            this->setMode(PinState::HighZ);
            }
        return fEnable ? kPowerUpDelayMs : kPowerDownDelayMs;
        }

    virtual bool get5v() override
        {
        return this->m_f5vState;
        }

    virtual void suspend() override
        {
        this->set5v(false);
        }

    virtual std::uint32_t resume() override
        {
        return this->set5v(true);
        }

    static constexpr char pinStateName(PinState v)
        {
        switch (v)
            {
        case PinState::Zero:    return '0';
        case PinState::One:     return '1';
        case PinState::HighZ:   return 'Z';
        default:                return '?';
            }
        }
    virtual void setReset(PinState v) override
        {
        PinState const old = this->m_reset;

        if (old == v)
            return;

        if (this->isEnabled(cPMS7003::DebugFlags::kTrace))
            this->printf("setReset: %c\n", pinStateName(v));

        this->m_reset = v;
        updatePin(v, old);
        }

    virtual PinState getReset() override
        {
        return this->m_reset;
        }

    virtual void setMode(PinState v) override
        {
        PinState const old = this->m_mode;

        if (old == v)
            return;

        if (this->isEnabled(cPMS7003::DebugFlags::kTrace))
            this->printf("setMode: %c\n", pinStateName(v));

        this->m_mode = v;
        updatePin(v, old);
        }
    virtual PinState getMode() override
        {
        return this->m_mode;
        }

    virtual void registerPollableObject(McciCatena::cPollableObject *pObject) override
        {
        this->m_Catena.registerObject(pObject);
        }

    virtual void printf(const char *pFmt, ...) override
        {
        std::va_list ap;
        char buf[128];
        if (! Serial)
            return;

        va_start(ap, pFmt);
        vsnprintf(buf, sizeof(buf)-1, pFmt, ap);
        buf[sizeof(buf)-1] = 0;

        this->m_Catena.SafePrintf("%s", buf);
        }

    virtual bool isEnabled(std::uint32_t mask) const override
        {
        return (mask == 0) || (mask & this->m_debugMask);
        }

    void setDebugFlags(std::uint32_t mask)
        {
        this->m_debugMask = mask;
        }

    std::uint32_t getDebugFlags() const
        {
        return this->m_debugMask;
        }

private:
    static void updatePin(PinState v, PinState old)
        {
        if (v == PinState::HighZ)
            setPinMode(kResetPin, INPUT);
        else
            {
            pinWrite(kResetPin, v == PinState::One);
            if (old == PinState::HighZ)
                {
                setPinMode(kResetPin, OUTPUT);
                }
            }
        }

private:
    McciCatena::Catena4630 &m_Catena;
    std::uint32_t m_debugMask;
    bool        m_f5vState;
    PinState    m_reset;
    PinState    m_mode;
    };

# endif // defined ARDUINO_MCCI_CATENA_4630
#endif // defined _Catena_PMS7003Hal_4630_h_
