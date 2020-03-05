/*

Module: catena-pms7003-lora-cMeasurementLoop.h

Function:
    cMeasurementLoop definitions.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

#ifndef _cMeasurementLoop_h_
# define _cMeasurementLoop_h_

#pragma once


#include <Arduino.h>
#include <Wire.h>
#include <Catena.h>
#include <Catena_FSM.h>
#include <Catena_Led.h>
#include <Catena_Log.h>
#include <Catena_Mx25v8035f.h>
#include <Catena_PollableInterface.h>
#include <Catena_Timer.h>
#include <Catena_TxBuffer.h>
#include <Catena-SHT3x.h>
#include <Catena-PMS7003.h>
#include <Catena-PMS7003Hal-4630.h>
#include <mcciadk_baselib.h>
#include <stdlib.h>

#include <cstdint>

#ifndef ARDUINO_MCCI_CATENA_4630
# error "This sketch targets the MCCI Catena 4630"
#endif

extern McciCatena::Catena gCatena;
extern McciCatena::Catena::LoRaWAN gLoRaWAN;
extern McciCatena::StatusLed gLed;

/****************************************************************************\
|
|   An object to represent the uplink activity
|
\****************************************************************************/

class cMeasurementLoop : public McciCatena::cPollableObject
    {
public:
    // constructor
    cMeasurementLoop(
            McciCatenaPMS7003::cPMS7003& pms7003, 
            McciCatenaSht3x::cSHT3x& TempRh
            )
        : m_Pms7003(pms7003)
        , m_TempRh(TempRh)
        , m_txCycleSec_Permanent(6 * 60)    // default uplink interval
        , m_txCycleSec(30)                  // initial uplink interval
        , m_txCycleCount(10)                // initial count of fast uplinks
        {};

    // neither copyable nor movable
    cMeasurementLoop(const cMeasurementLoop&) = delete;
    cMeasurementLoop& operator=(const cMeasurementLoop&) = delete;
    cMeasurementLoop(const cMeasurementLoop&&) = delete;
    cMeasurementLoop& operator=(const cMeasurementLoop&&) = delete;

    enum class State : std::uint8_t
        {
        stNoChange = 0, // this name must be present: indicates "no change of state"
        stInitial,      // this name must be present: it's the starting state.
        stInactive,     // parked; not doing anything.
        stSleeping,     // active; sleeping between measurements
        stWakePms,      // wake the PM sensor
        stMeasurePms,   // make the PM measurements
        stSleepPms,     // sleep the PM sensor
        stTransmit,     // transmit data

        stFinal,        // this name must be present, it's the terminal state.
        };

    static constexpr const char *getStateName(State s)
        {
        switch (s)
            {
        case State::stNoChange: return "stNoChange";
        case State::stInitial: return "stInitial";
        case State::stInactive: return "stInactive";
        case State::stSleeping: return "stSleeping";
        case State::stWakePms: return "stWakePms";
        case State::stMeasurePms: return "stMeasurePms";
        case State::stSleepPms: return "stSleepPms";
        case State::stTransmit: return "stTransmit";
        case State::stFinal: return "stFinal";
        default: return "<<unknown>>";
            }
        }

    static constexpr uint8_t kUplinkPort = 1;
    static constexpr uint8_t kMessageFormat = 0x21;

    enum class Flags : uint8_t
            {
            Vbat = 1 << 0,
            Vcc = 1 << 1,
            Vbus = 1 << 2,  // Vbus input
            Boot = 1 << 3,
            TH = 1 << 4,    // temperature, humidity
            PM = 1 << 5,    // Particulate matter
            Dust = 1 << 6,  // Dust
            };

    static constexpr size_t kTxBufferSize = 36;
    using TxBuffer_t = McciCatena::AbstractTxBuffer_t<kTxBufferSize>;

    // initialize measurement FSM.
    void begin();
    void end();
    McciCatenaPMS7003::cPMS7003Hal *getHal() const
        {
        return this->m_Pms7003.getHal();
        }
    void setTxCycleTime(
        std::uint32_t txCycleSec,
        std::uint32_t txCycleCount
        )
        {
        this->m_txCycleSec = txCycleSec;
        this->m_txCycleCount = txCycleCount;

        this->m_UplinkTimer.setInterval(txCycleSec * 1000);
        if (this->m_UplinkTimer.peekTicks() != 0)
            this->m_fsm.eval();
        }
    std::uint32_t getTxCycleTime()
        {
        return this->m_txCycleSec;
        }
    virtual void poll() override;
    void setTempRh(bool fEnable)
        {
        this->m_fTempRh = fEnable; 
        }
    void setVbus(float Vbus)
        {
        this->m_fUsbPower = (Vbus > 3.0f);
        }

    // request that the measurement loop be active/inactive
    void requestActive(bool fEnable);

private:
    static constexpr unsigned kNumMeasurements = 10;

    // evaluate the control FSM.
    State fsmDispatch(State currentState, bool fEntry);

    // set the timer
    void setTimer(std::uint32_t ms)
        {
        this->m_timer_start = millis();
        this->m_timer_delay = ms;
        this->m_fTimerActive = true;
        this->m_fTimerEvent = false;
        }
    void clearTimer()
        {
        this->m_fTimerActive = false;
        this->m_fTimerEvent = false;
        }
    bool timedOut()
        {
        bool result = this->m_fTimerEvent;
        this->m_fTimerEvent = false;
        return result;
        }

    // sleep handling
    void sleep();
    bool checkDeepSleep();
    void doSleepAlert(bool fDeepSleep);
    void doDeepSleep();
    void deepSleepPrepare();
    void deepSleepRecovery();

    // measurement handling
    void resetMeasurement()
        {
        this->m_iMeasurement = 0;
        this->m_measurement_received = false;
        this->m_measurement_valid = false;
        }
    bool measurementAwake()
        {
        return this->m_measurement_received;
        }
    bool measurementComplete()
        {
        return this->m_iMeasurement >= kNumMeasurements;
        }
    static void measurementAvailable(
        void *pUserData,
        const McciCatenaPMS7003::cPMS7003::Measurements<std::uint16_t> *pData,
        bool fWarmedUp
        );
    void processMeasurement(
        const McciCatenaPMS7003::cPMS7003::Measurements<std::uint16_t> *pData,
        bool fWarmedUp
        );
    void processOneMeasurement(
        float &r,
        std::uint16_t *pv
        );
    bool postProcess(
        McciCatenaPMS7003::cPMS7003::Measurements<float> &results
        );
    void fillTxBuffer(TxBuffer_t &b);
    void startTransmission(TxBuffer_t &b);
    void sendBufferDone(bool fSuccess);
    bool txComplete()
        {
        return this->m_txcomplete;
        }
    static std::uint16_t particle2uf(float v)
        {
        return McciCatena::TxBuffer_t::f2uflt16(v);
        }

    void updateTxCycleTime();

    // instance data
    McciCatena::cFSM <cMeasurementLoop, State>
                        m_fsm;
    McciCatenaPMS7003::cPMS7003&
                        m_Pms7003;
    McciCatenaSht3x::cSHT3x&    m_TempRh;

    // true if object is registered for polling.
    bool                m_registered : 1;
    // true if object is running.
    bool                m_running : 1;
    // true to request exit
    bool                m_exit : 1;
    // true if in active uplink mode, false otehrwise.
    bool                m_active : 1;

    // set true to request transition to active uplink mode; cleared by FSM
    bool                m_rqActive : 1;
    // set true to request transition to inactive uplink mode; cleared by FSM
    bool                m_rqInactive : 1;

    // set true if event timer times out
    bool                m_fTimerEvent : 1;
    // set true while evenet timer is active.
    bool                m_fTimerActive : 1;
    // set true when a measurement is received
    bool                m_measurement_received : 1;
    // set true when sufficient valid measurements are received
    bool                m_measurement_valid : 1;
    // set true when first warmed-up measurement is received.
    bool                m_fWarmedUp : 1;
    // set true if USB power is present.
    bool                m_fUsbPower : 1;
    // set true if Temperature/RH sensor (SHT3x) is present
    bool                m_fTempRh : 1;
    // set true while a transmit is pending.
    bool                m_txpending : 1;
    // set true when a transmit completes.
    bool                m_txcomplete : 1;
    // set true when a transmit complete with an error.
    bool                m_txerr : 1;
    // set true when we've printed how we plan to sleep
    bool                m_fPrintedSleeping : 1;

    // index of next measurement in array.
    unsigned            m_iMeasurement;
    // the collection of arrays of PM measurements
    McciCatenaPMS7003::cPMS7003::PmBins<std::uint16_t[kNumMeasurements]> m_Pm;
    // the collection of arrays of dust measurements
    McciCatenaPMS7003::cPMS7003::DustBins<std::uint16_t[kNumMeasurements]> m_Dust;

    // uplink time control
    McciCatena::cTimer  m_UplinkTimer;
    std::uint32_t       m_txCycleSec;
    std::uint32_t       m_txCycleCount;
    std::uint32_t       m_txCycleSec_Permanent;

    // for simple internal timer.
    std::uint32_t           m_timer_start;
    std::uint32_t           m_timer_delay;
    };

static constexpr cMeasurementLoop::Flags operator| (const cMeasurementLoop::Flags lhs, const cMeasurementLoop::Flags rhs)
        {
        return cMeasurementLoop::Flags(uint8_t(lhs) | uint8_t(rhs));
        };

static cMeasurementLoop::Flags operator|= (cMeasurementLoop::Flags &lhs, const cMeasurementLoop::Flags &rhs)
        {
        lhs = lhs | rhs;
        return lhs;
        };

#endif /* _cMeasurementLoop_h_ */
