/*

Module: cPMS7003.h

Function:
    The PMS7003 library

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

#ifndef _cPMS7003_h_
# define _cPMS7003_h_

#pragma once

#include <Arduino.h>
#include <Catena_FSM.h>
#include <Catena_PollableInterface.h>
#include <cstdint>

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

/****************************************************************************\
|
|   PMS7003 sensor
|
\****************************************************************************/

class cPMS7003 : public McciCatena::cPollableObject
    {
    //*******************************************
    // Forward references, etc.
    //*******************************************
private:
    typedef decltype(Serial1) cSerial;
    // get minimum reset time in millis.
    static constexpr std::uint32_t getTresetMin() { return 10; }

    //*******************************************
    // Constructor, etc.
    //*******************************************
public:
    cPMS7003(cSerial &port, cPMS7003Hal &hal)
        : m_port    (&port)
        , m_hal     (&hal)
        {};

    // neither copyable nor movable
    cPMS7003(const cPMS7003&) = delete;
    cPMS7003& operator=(const cPMS7003&) = delete;
    cPMS7003(const cPMS7003&&) = delete;
    cPMS7003& operator=(const cPMS7003&&) = delete;

    //*******************************************
    // States of the PMS7003 (and of our
    // tracking FSM)
    //*******************************************
    enum class State : std::uint8_t
        {
        stNoChange = 0, // this name must be present: indicates "no change of state"
        stInitial,      // this name must be present: it's the starting state.
        stInitialSetup,
        stOff,
        stRequestPowerOn,
        stReset,        // reset is asserted.
        stRequestPowerDown,
        stNormal,       // nornmal, idling
        stPassiveSendCmd,
        stPassive,
        stNormalSendCmd,
        stNormalHwSleep,
        stNormalSleepCmd,
        stNormalSwSleep,
        stNormalWakeCmd,
        stPassiveHwSleep,
        stPassiveMeasureCmd,
        stPassiveSleepCmd,
        stPassiveSwSleep,
        stPassiveWakeCmd,
        stFinal,        // this name must be present, it's the terminal state.
        };

    static constexpr char *getStateName(State s)
        {
        switch (s)
            {
        case State::stNoChange: return "stNoChange";
        case State::stInitial: return "stInitial";
        case State::stInitialSetup: return "stInitialSetup";
        case State::stOff: return "stOff";
        case State::stRequestPowerOn: return "stRequestPowerOn";
        case State::stReset: return "stReset";
        case State::stRequestPowerDown: return "stRequestPowerDown";
        case State::stNormal: return "stNormal";
        case State::stPassiveSendCmd: return "stPassiveSendCmd";
        case State::stPassive: return "stPassive";
        case State::stNormalSendCmd: return "stNormalSendCmd";
        case State::stNormalHwSleep: return "stNormalHwSleep";
        case State::stNormalSleepCmd: return "stNormalSleepCmd";
        case State::stNormalSwSleep: return "stNormalSwSleep";
        case State::stNormalWakeCmd: return "stNormalWakeCmd";
        case State::stPassiveHwSleep: return "stPassiveHwSleep";
        case State::stPassiveMeasureCmd: return "stPassiveMeasureCmd";
        case State::stPassiveSleepCmd: return "stPassiveSleepCmd";
        case State::stPassiveSwSleep: return "stPassiveSwSleep";
        case State::stPassiveWakeCmd: return "stPassiveWakeCmd";
        case State::stFinal: return "stFinal";
        default: return "<<unknown>>";
            }
        }

    //*******************************************
    // Debug flags
    //*******************************************
public:
    enum DebugFlags : std::uint32_t
        {
        kError      = 1 << 0,
        kWarning    = 1 << 1,
        kTrace      = 1 << 2,
        kInfo       = 1 << 3,
        };

    //*******************************************
    // Templates for the results of measurements
    //*******************************************

    // (defined as templates because we may be averaging
    // or aggregeating)

    // the PMx bins: 1.0, 2.5, and 10.
    template <typename T>
    struct PmBins
        {
        T   m1p0;   // PM1.0 concentration ug/m3
        T   m2p5;   // PM2.5 concentration ug/m3
        T   m10;    // PM10 concentration ug/m3
        };

    //
    // There is disagreement in the literature about the nature
    // of the bins.
    //
    // Plantower says >= 0.3um; aqicn.org says <= 0.3um, etc.
    // We follow Plantower in the documentation for now.
    //
    template <typename T>
    struct DustBins
        {
        T   m0p3;   // particles >= 0.3 um per 0.1L
        T   m0p5;   // particles >= 0.5 um per 0.1L
        T   m1p0;   // particles >= 1.0 um per 0.1L
        T   m2p5;   // particles >= 2.5 um per 0.1L
        T   m5;     // particles >= 5 um per 0.1L
        T   m10;    // particles >= 10 um per 0.1L
        };

    // the default measurement structure represents
    // all the measurements
    template <typename T>
    struct Measurements
        {
        PmBins<T>   cf1;

        // aqncn.org uses atm for their experiments.
        PmBins<T>   atm;

        // bins of dust.
        DustBins<T> dust;
        };

    //*******************************************
    // The wire-level packets
    //*******************************************
private:
    static constexpr std::uint8_t   kStart1 = 0x42;
    static constexpr std::uint8_t   kStart2 = 0x4D;

    struct WireData
        {
        std::uint8_t                    ucStart1;
        std::uint8_t                    ucStart2;
        std::uint8_t                    usLength[2];
        Measurements<std::uint8_t[2]>   Data;
        std::uint8_t                    Reserved[2];
        std::uint8_t                    usChecksum[2];

        static constexpr int expected(unsigned iChar)
            {
            switch (iChar)
                {
            case 0:     return kStart1;
            case 1:     return kStart2;
            case 2:     return 0;
            default:    return -1;
                }
            }
        std::uint8_t *  getBuffer()
            { return &this->ucStart1; }
        const std::uint8_t *  getBuffer() const
            { return &this->ucStart1; }
        };

    static_assert(sizeof(WireData) == 32);

    static constexpr std::uint16_t computeChecksum(const std::uint8_t *pData, size_t nData)
        {
        std::uint16_t sum = 0;

        for (; nData > 0; ++pData, --nData)
            sum += *pData;

        return sum;
        }

    static void writeChecksum(std::uint8_t *pResult, const std::uint8_t *pData, size_t nData)
        {
        auto sum = computeChecksum(pData, nData);
        pResult[0] = std::uint8_t(sum >> 8);
        pResult[1] = std::uint8_t(sum & 0xFF);
        }

    static std::uint16_t getUint16Be(const std::uint8_t *pBytes)
        {
        return (pBytes[0] << 8) | pBytes[1];
        }

    class WireCommand
        {
    public:
        static constexpr std::uint8_t   kReadPassive = 0xE2;
        static constexpr std::uint8_t   kChangeMode = 0xE1;
        static constexpr std::uint8_t   kSleep = 0xE4;

        WireCommand(std::uint8_t a_ucCommand)
            { WireCommand(a_ucCommand, 0); }

        WireCommand(std::uint8_t a_ucCommand, std::uint16_t a_usData)
            : ucCommand(a_ucCommand)
            {
            this->usData[0] = std::uint8_t(a_usData >> 8);
            this->usData[1] = std::uint8_t(a_usData & 0xFF);
            cPMS7003::writeChecksum(
                this->usChecksum,
                (const std::uint8_t *)this,
                sizeof(*this) - 2
                );
            }

        std::uint8_t    ucStart1 = kStart1;
        std::uint8_t    ucStart2 = kStart2;
        std::uint8_t    ucCommand;
        std::uint8_t    usData[2];
        std::uint8_t    usChecksum[2];

        std::uint8_t *  getBuffer()
            { return &this->ucStart1; }
        const std::uint8_t *  getBuffer() const
            { return &this->ucStart1; }
        };

    static_assert(sizeof(WireCommand) == 7);

    class WireCommandMeasure : public WireCommand
        {
    public:
        WireCommandMeasure()
            : WireCommand(kReadPassive)
            {}
        };

    class WireCommandActiveMode : public WireCommand
        {
    public:
        WireCommandActiveMode(bool fActive)
            : WireCommand(kChangeMode, fActive)
            {}
        };

    class WireCommandRunMode : public WireCommand
        {
    public:
        WireCommandRunMode(bool fAwake)
            : WireCommand(kSleep, fAwake)
            {}
        };

    //*******************************************
    // Statistics
    //*******************************************
public:
    struct RxStats
        {
        std::uint32_t   CharIn;
        std::uint32_t   CharDrops;
        std::uint32_t   MsgDrops;
        std::uint32_t   BadChecksum;
        std::uint32_t   GoodMsg;
        };

    //*******************************************
    // The public methods
    //*******************************************
public:
    // start the sensor. This turns on power to the sensor
    // and starts the power-up seqeunce.
    bool begin();

    // stop the sensor. 
    void end();

    typedef void MeasurementCb_t(void *pUserData, const Measurements<std::uint16_t> *pData);

    // Set the callback function
    bool setCallback(MeasurementCb_t *pFn, void *pUserData)
        {
        this->m_pMeasurementCb = pFn;
        this->m_pMeasurementUserData = pUserData;
        }

    virtual void poll(void) override;
    void suspend();
    void resume();

    void requestOff() { setRequest(Request::Off); }
    void requestReset() { setRequest(Request::Reset); }
    void requestHwSleep() { setRequest(Request::HwSleep); }
    void requestSleep() { setRequest(Request::Sleep); }
    void requestPassive() { setRequest(Request::Passive); }
    void requestNormal() { setRequest(Request::Normal); }
    void requestMeasure() { setRequest(Request::Measure); }

    void eventWake() { setEvent(Event::Wake); }
    RxStats getRxStats() { return this->m_RxStats; }

    //*******************************************
    // Event handling
    //*******************************************
private:
    // events
    enum class Event : std::uint32_t
        {
        Timer,
        TxDone,
        NewData,
        Wake,
        Max
        };

    static_assert(int(Event::Max) < 32);

    void resetEvent(Event e)
        {
        this->m_events &= ~evMask(e);
        }
    bool checkEvent(Event e)
        {
        const std::uint32_t m = evMask(e);

        if (this->m_events & m)
            {
            this->m_events &= ~m;
            return true;
            }
        else
            return false;
        }

    bool setEvent(Event e)
        {
        const std::uint32_t m = evMask(e);
        this->m_events |= m;
        this->m_fsm.eval();
        }

    static std::uint32_t evMask(Event e) { return 1 << std::uint32_t(e); }

    //*******************************************
    // Request handling
    //*******************************************
private:
    enum class Request : std::uint32_t
        {
        Off,
        Reset,
        HwSleep,
        Sleep,
        Passive,
        Normal,
        Measure,
        Max
        };

    static_assert(int(Request::Max) < 32);

    bool checkRequest(Request r)
        {
        const std::uint32_t m = rqMask(r);

        if (this->m_requests & (m - 1))
            return false;
        else if (this->m_requests & m)
            {
            this->m_requests = 0;
            return true;
            }
        else
            return false;
        }

    bool setRequest(Request r)
        {
        const std::uint32_t m = rqMask(r);
        this->m_requests |= m;
        this->m_fsm.eval();
        }

    void allowRequests(std::uint32_t rmask)
        {
        this->m_requests &= ~rmask;
        }

    void resetRequests()
        {
        this->m_requests = 0;
        }

    static std::uint32_t rqMask(Request r) { return 1 << std::uint32_t(r); }

    //*******************************************
    // The timer
    //*******************************************
private:
    void setTimer(std::uint32_t ms);

    //*******************************************
    // Internal utilities
    //*******************************************
private:
    // evaluate the control FSM.
    State fsmDispatch(State currentState, bool fEntry);

    // send a command.
    void sendCommand(const WireCommand &cmd);

    //*******************************************
    // The instance data
    //*******************************************
private:
    // the FSM
    McciCatena::cFSM <cPMS7003, State>
                            m_fsm;

    // the serial port
    cSerial *               m_port;

    // the HAL
    cPMS7003Hal *           m_hal;

    MeasurementCb_t *       m_pMeasurementCb;
    void *                  m_pMeasurementUserData;

    std::uint32_t           m_requests;
    std::uint32_t           m_events;

    WireData                m_rxBuffer;
    std::uint32_t           m_iRxData;
    RxStats                 m_RxStats;
    std::uint32_t           m_txempty_avail;

    std::uint32_t           m_timer_start;
    std::uint32_t           m_timer_delay;

    // flags
    union
        {
        uint32_t v;
        struct
            {
            bool Registered : 1;
            bool Running : 1;
            bool Exit : 1;
            bool RxTxEnabled : 1;
            bool TxActive: 1;
            bool TimerActive: 1;
            } b;
        }                   m_flags;
    };

#endif // defined _cPMS7003_h_
