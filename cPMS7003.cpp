/*

Module: cPMS7003.cpp

Function:
    Implementation of PMS7003 library.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

#include <cPMS7003.h>

// see http://aqicn.org/sensor/pms5003-7003/ for some useful
// info and guidance on how to use this sensor.
// For info on AQI: https://stimulatedemissions.wordpress.com/2013/04/10/how-is-the-air-quality-index-aqi-calculated/
// http://www.epa.gov/ttn/oarpg/t1/memoranda/rg701.pdf
// https://archive.epa.gov/ttn/ozone/web/pdf/rg701.pdf
//
// The EPA defines a piecewise linear AQI.
//  PM2.5           PM10        AQI lower, delta
//  0:15.4          0:54        0:50
//  15.5:40.4       55:154      51:50
//  40.5:65.4       155:254     101:50
//  65.5:150.4      255:354     151:50
//  150.5:250.4     355:424     201:100
//  250.5:350.4     425:504     300:100
//  350.5:500.4     505:604     400:100

/****************************************************************************\
|
|   Code
|
\****************************************************************************/

bool cPMS7003::begin()
    {
    if (! this->m_flags.b.Registered)
        {
        this->m_hal->registerPollableObject(this);
        this->m_flags.b.Registered = true;
        }

    if (! this->m_flags.b.Running)
        {
        // start the FSM
        this->m_flags.b.RxTxEnabled = false;
        this->m_flags.b.Exit = false;
        this->m_fsm.init(*this, &cPMS7003::fsmDispatch);
        }

    return true;
    }

void cPMS7003::end()
    {
    if (this->m_flags.b.Running)
        {
        this->m_flags.b.Exit = true;
        while (this->m_flags.b.Running)
            this->m_fsm.eval();
        }
    }

cPMS7003::State cPMS7003::fsmDispatch(
    cPMS7003::State currentState,
    bool fEntry
    )
    {
    State newState = State::stNoChange;

    if (fEntry && this->m_hal->isEnabled(DebugFlags::kTrace))
        {
        this->m_hal->printf("cPMS7003::fsmDispatch: enter %s\n",
                this->getStateName(currentState)
                );
        }

    // first we try to handle the outer states.
    switch (currentState)
        {
    case State::stInitial:
        newState = State::stInitialSetup;
        break;

    case State::stInitialSetup:
        if (fEntry)
            {
            // set up the HAL
            this->m_hal->begin();
            this->setTimer(this->m_hal->set5v(false));
            }
        if (this->checkEvent(Event::Timer))
            newState = State::stOff;
        break;

    case State::stOff:
        if (fEntry)
            {
            /* nothing */
            }

        if (this->m_flags.b.Exit)
            newState = State::stFinal;
        else if (this->checkEvent(Event::Wake))
            newState = State::stRequestPowerOn;
        break;

    case State::stRequestPowerOn:
        if (fEntry)
            {
            this->setTimer(this->m_hal->set5v(true));
            }
        if (this->checkEvent(Event::Timer))
            {
            newState = State::stReset;
            this->m_port->begin(9600);
            this->m_flags.b.RxTxEnabled = true;
            this->m_iRxData = 0;
            this->m_txempty_avail = this->m_port->availableForWrite();
            }
        break;

    case State::stReset:
        if (fEntry)
            {
            this->m_hal->setReset(cPMS7003Hal::PinState::Zero);
            this->setTimer(getTresetMin());

            // bring up the serial port.
            }

        if (this->checkEvent(Event::Timer))
            {
            this->m_hal->setReset(cPMS7003Hal::PinState::One);
            newState = State::stNormal;
            }
        break;

    case State::stRequestPowerDown:
        if (fEntry)
            {
            this->m_hal->setReset(cPMS7003Hal::PinState::Zero);
            this->m_hal->setMode(cPMS7003Hal::PinState::HighZ);
            this->m_port->end();
            this->m_flags.b.RxTxEnabled = false;
            this->setTimer(this->m_hal->set5v(false));
            }

        if (this->checkEvent(Event::Timer))
            newState = State::stOff;
        break;

    case State::stFinal:
        this->m_hal->end();
        this->m_flags.b.Running = false;
        break;

    // the remining State::states all have to look for evReset, evOff, fExit.
    default:
        if (this->checkRequest(Request::Off) || this->m_flags.b.Exit)
            newState = State::stRequestPowerDown;
        else if (this->checkRequest(Request::Reset))
            newState = State::stReset;
        else
            {
            switch (currentState)
                {
            case State::stNormal:
                {
                if (fEntry)
                    {
                    this->m_hal->setMode(cPMS7003Hal::PinState::One);
                    }
                auto const oldRequests = this->m_requests;

                this->allowRequests(
                        rqMask(Request::HwSleep) |
                        rqMask(Request::Sleep) |
                        rqMask(Request::Passive)
                        );

                if (oldRequests != 0 && this->m_hal->isEnabled(DebugFlags::kTrace))
                    {
                    this->m_hal->printf(
                            "%s: oldRequests: 0x%x m_requests 0x%x\n",
                            __func__,
                            oldRequests,
                            this->m_requests
                            );
                    }

                if (this->checkRequest(Request::HwSleep))
                    {
                    newState = State::stNormalHwSleep;
                    }
                else if (this->checkRequest(Request::Sleep))
                    {
                    newState = State::stNormalSleepCmd;
                    }
                else if (this->checkRequest(Request::Passive))
                    {
                    newState = State::stPassiveSendCmd;
                    }
                }
                break;

            case State::stPassiveSendCmd:
                if (fEntry)
                    {
                    const WireCommandActiveMode cmd {false};

                    this->sendCommand(cmd);
                    }
                if (this->checkEvent(Event::TxDone))
                    newState = State::stPassive;
                break;

            case State::stPassive:
                if (fEntry)
                    {
                    this->m_hal->setMode(cPMS7003Hal::PinState::One);
                    }
                this->allowRequests(
                        rqMask(Request::HwSleep) |
                        rqMask(Request::Sleep) |
                        rqMask(Request::Normal) |
                        rqMask(Request::Measure)
                        );

                if (this->checkRequest(Request::HwSleep))
                    {
                    newState = State::stPassiveHwSleep;
                    }
                else if (this->checkRequest(Request::Sleep))
                    {
                    newState = State::stPassiveSleepCmd;
                    }
                else if (this->checkRequest(Request::Normal))
                    {
                    newState = State::stNormalSendCmd;
                    }
                else if (this->checkRequest(Request::Measure))
                    {
                    newState = State::stPassiveMeasureCmd;
                    }
                break;

            case State::stNormalSendCmd:
                if (fEntry)
                    {
                    const WireCommandActiveMode cmd {true};

                    this->sendCommand(cmd);
                    }
                if (this->checkEvent(Event::TxDone))
                    newState = State::stNormal;
                break;

            case State::stNormalHwSleep:
                if (fEntry)
                    {
                    this->m_hal->setMode(cPMS7003Hal::PinState::Zero);
                    }
                if (this->checkEvent(Event::Wake))
                    newState = State::stNormal;
                break;

            case State::stNormalSleepCmd:
                if (fEntry)
                    {
                    const WireCommandRunMode cmd {false};

                    this->sendCommand(cmd);
                    }
                if (this->checkEvent(Event::TxDone))
                    newState = State::stNormalSwSleep;
                break;

            case State::stNormalSwSleep:
                if (this->checkEvent(Event::Wake))
                    newState = State::stNormalWakeCmd;
                break;

            case State::stNormalWakeCmd:
                if (fEntry)
                    {
                    const WireCommandRunMode cmd { true };

                    this->sendCommand(cmd);
                    }
                if (this->checkEvent(Event::TxDone))
                    newState = State::stNormal;
                break;

            case State::stPassiveHwSleep:
                if (fEntry)
                    {
                    this->m_hal->setMode(cPMS7003Hal::PinState::Zero);
                    }
                if (this->checkEvent(Event::Wake))
                    newState = State::stPassive;
                break;

            case State::stPassiveMeasureCmd:
                if (fEntry)
                    {
                    const WireCommandMeasure cmd;

                    this->sendCommand(cmd);
                    this->resetEvent(Event::NewData);
                    this->setTimer(1000);
                    }
                if (this->checkEvent(Event::NewData))
                    newState = State::stPassive;
                else if (this->checkEvent(Event::Timer))
                    {
                    newState = State::stPassive;
                    }
                break;

            case State::stPassiveSleepCmd:
                if (fEntry)
                    {
                    const WireCommandRunMode cmd {false};

                    this->sendCommand(cmd);
                    }
                if (this->checkEvent(Event::TxDone))
                    newState = State::stPassiveSwSleep;
                break;

            case State::stPassiveSwSleep:
                if (this->checkEvent(Event::Wake))
                    newState = State::stPassiveWakeCmd;
                break;

            case State::stPassiveWakeCmd:
                if (fEntry)
                    {
                    const WireCommandRunMode cmd { true };

                    this->sendCommand(cmd);
                    }
                if (this->checkEvent(Event::TxDone))
                    newState = State::stPassive;
                break;

            default:
                if (this->m_hal->isEnabled(DebugFlags::kError))
                    {
                    this->m_hal->printf(
                            "%s: unknown state %s (%u)\n",
                            __func__,
                            this->getStateName(currentState),
                            unsigned(currentState)
                            );
                    }
                break;
                }
            }
        break;
        } // end switch.

    return newState;
    }

void cPMS7003::sendCommand(const WireCommand &cmd)
    {
    this->m_flags.b.TxActive = true;
    this->resetEvent(Event::TxDone);
    this->m_port->write(cmd.getBuffer(), sizeof(cmd));

    if (this->m_hal->isEnabled(DebugFlags::kTxData))
        {
        this->m_hal->printf("TX:");
        auto p = cmd.getBuffer();
        for (auto n = sizeof(cmd); n > 0; ++p, --n)
            {
            this->m_hal->printf(" %02x", *p);
            }
        this->m_hal->printf("\n");
        }
    }

void cPMS7003::poll(void)
    {
    if (this->m_flags.b.RxTxEnabled)
        {
        // handle serial receives
        auto const nRx = this->m_port->available();
        for (auto i = nRx; i > 0; --i)
            {
            auto iBuffer = this->m_iRxData;
            auto pBuffer = this->m_rxBuffer.getBuffer();
            std::uint8_t c = this->m_port->read();
            auto const expected = WireData::expected(iBuffer);

            if (expected >= 0 && c != expected)
                {
                if (this->m_hal->isEnabled(DebugFlags::kRxDiscard))
                    this->m_hal->printf("%02x ", c);
                this->m_iRxData = 0;
                this->m_RxStats.CharDrops += iBuffer + 1;
                if (iBuffer > 0)
                    ++this->m_RxStats.MsgDrops;
                }
            else
                {
                pBuffer[iBuffer] = c;
                if (iBuffer == sizeof(this->m_rxBuffer) - 1)
                    {
                    auto cs = computeChecksum(pBuffer, sizeof(this->m_rxBuffer) - 2);
                    std::uint16_t rxCs = getUint16Be(this->m_rxBuffer.usChecksum);
                    if (cs != rxCs)
                        {
                        ++this->m_RxStats.BadChecksum;
                        }
                    else
                        {
                        ++this->m_RxStats.GoodMsg;
                        this->setEvent(Event::NewData);
                        if (this->m_pMeasurementCb != nullptr)
                            {
                            Measurements<std::uint16_t> m;
                            Measurements<std::uint8_t[2]> &r = this->m_rxBuffer.Data;

                            // convert to internal form
                            m.cf1.m1p0  = getUint16Be(r.cf1.m1p0);
                            m.cf1.m2p5  = getUint16Be(r.cf1.m2p5);
                            m.cf1.m10   = getUint16Be(r.cf1.m10);
                            m.atm.m1p0  = getUint16Be(r.atm.m1p0);
                            m.atm.m2p5  = getUint16Be(r.atm.m2p5);
                            m.atm.m10   = getUint16Be(r.atm.m10);
                            m.dust.m0p3 = getUint16Be(r.dust.m0p3);
                            m.dust.m0p5 = getUint16Be(r.dust.m0p5);
                            m.dust.m1p0 = getUint16Be(r.dust.m1p0);
                            m.dust.m2p5 = getUint16Be(r.dust.m2p5);
                            m.dust.m5   = getUint16Be(r.dust.m5  );
                            m.dust.m10  = getUint16Be(r.dust.m10 );

                            (this->m_pMeasurementCb)(
                                this->m_pMeasurementUserData,
                                &m
                                );
                            }
                        }
                    this->m_iRxData = 0;
                    }
                else
                    {
                    this->m_iRxData = iBuffer + 1;
                    }
                }
            }

        // handle serial transmit completions
        if (this->m_flags.b.TxActive)
            {
            if (this->m_port->availableForWrite() >= this->m_txempty_avail)
                {
                this->m_flags.b.TxActive = false;
                this->setEvent(Event::TxDone);
                }
            }
        }

    // handle timer
    if (this->m_flags.b.TimerActive)
        {
        if ((millis() - this->m_timer_start) >= this->m_timer_delay)
            {
            this->m_flags.b.TimerActive = false;
            this->setEvent(Event::Timer);
            }
        }
    }

void cPMS7003::setTimer(std::uint32_t ms)
    {
    this->m_timer_start = millis();
    this->m_timer_delay = ms;
    this->m_flags.b.TimerActive = true;
    }