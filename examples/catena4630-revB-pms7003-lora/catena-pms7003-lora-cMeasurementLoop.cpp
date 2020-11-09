/*

Module: catena-pms7003-lora-cMeasurementLoop.cpp

Function:
    Sensor sketch measuring and transmitting air-quality info.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

#include "catena-pms7003-lora-cMeasurementLoop.h"

#ifndef ARDUINO_MCCI_CATENA_4630
# error "This sketch targets the MCCI Catena 4630"
#endif

extern SPIClass gSPI2;
extern bool gfFlash;

/****************************************************************************\
|
|   An object to represent the uplink activity
|
\****************************************************************************/

void cMeasurementLoop::begin()
    {
    // register for polling.
    if (! this->m_registered)
        {
        this->m_registered = true;

        gCatena.registerObject(this);

        this->m_Pms7003.setCallback(measurementAvailable, this);

        this->m_UplinkTimer.begin(this->m_txCycleSec * 1000);
        }

    if (! this->m_running)
        {
        this->m_exit = false;
        this->m_fsm.init(*this, &cMeasurementLoop::fsmDispatch);
        }
    }

void cMeasurementLoop::end()
    {
    if (this->m_running)
        {
        this->m_exit = true;
        this->m_fsm.eval();
        }
    }

void cMeasurementLoop::requestActive(bool fEnable)
    {
    if (fEnable)
        this->m_rqActive = true;
    else
        this->m_rqInactive = true;

    this->m_fsm.eval();
    }

cMeasurementLoop::State cMeasurementLoop::fsmDispatch(
    cMeasurementLoop::State currentState,
    bool fEntry
    )
    {
    State newState = State::stNoChange;
    auto const pHal = this->getHal();

    if (fEntry && pHal->isEnabled(this->m_Pms7003.DebugFlags::kTrace))
        {
        this->getHal()->printf("cMeasurementLoop::fsmDispatch: enter %s\n",
                this->getStateName(currentState)
                );
        }

    switch (currentState)
        {
    case State::stInitial:
        newState = State::stInactive;
        break;

    case State::stInactive:
        if (fEntry)
            {
            this->m_Pms7003.requestOff();
            }
        if (this->m_rqActive)
            {
            this->m_rqActive = this->m_rqInactive = false;
            this->m_active = true;
            this->m_UplinkTimer.retrigger();
            newState = State::stWakePms;
            }
        break;

    case State::stSleeping:
        if (fEntry)
            {
            this->m_Pms7003.requestOff();
            gLed.Set(McciCatena::LedPattern::Sleeping);
            }

        if (this->m_rqInactive)
            {
            this->m_rqActive = this->m_rqInactive = false;
            this->m_active = false;
            newState = State::stInactive;
            }
        else if (this->m_UplinkTimer.isready())
            newState = State::stWakePms;
        else if (this->m_UplinkTimer.getRemaining() > 1500)
            this->sleep();
        break;

    case State::stWakePms:
        if (fEntry)
            {
            this->m_Pms7003.eventWake();
            this->setTimer(2 * 60 * 1000);
            this->resetMeasurement();
            }
        if (this->timedOut())
            newState = State::stSleepPms;
        else if (this->measurementAwake())
            {
            this->clearTimer();
            newState = State::stMeasurePms;
            }
        break;

    case State::stMeasurePms:
        if (fEntry)
            {
            this->setTimer(this->kNumMeasurements * 2 * 1000);
            }
        if (this->timedOut())
            {
            newState = State::stSleepPms;
            }
        else if (this->measurementComplete())
            {
            this->clearTimer();
            this->m_measurement_valid = true;
            newState = State::stSleepPms;
            }
        break;

    case State::stSleepPms:
        if (fEntry)
            {
            this->m_Pms7003.requestOff();
            this->setTimer(10);
            }
        if (this->timedOut())
            newState = State::stTransmit;
        break;

    case State::stTransmit:
        if (fEntry)
            {
            TxBuffer_t b;
            this->fillTxBuffer(b);
            this->startTransmission(b);
            }
        if (this->txComplete())
            {
            newState = State::stSleeping;

            // calculate the new sleep interval.
            this->updateTxCycleTime();
            }
        break;

    case State::stFinal:
        break;

    default:
        break;
        }
    
    return newState;
    }

/****************************************************************************\
|
|   Got a measurement
|
\****************************************************************************/

void cMeasurementLoop::measurementAvailable(
    void *pUserData,
    const McciCatenaPMS7003::cPMS7003::Measurements<std::uint16_t> *pData,
    bool fWarmedUp
    )
    {
    cMeasurementLoop * const pThis = (cMeasurementLoop *)pUserData;

//    gCatena.SafePrintf(
//        "CF1 pm 1.0=%-5u 2.5=%-5u 10=%-5u ",
//        pData->cf1.m1p0, pData->cf1.m2p5, pData->cf1.m10
//        );

    pThis->processMeasurement(pData, fWarmedUp);
    }

/****************************************************************************\
|
|   Put a measurement into the list
|
\****************************************************************************/

void cMeasurementLoop::processMeasurement(
    const McciCatenaPMS7003::cPMS7003::Measurements<std::uint16_t> *pData,
    bool fWarmedUp
    )
    {
    gCatena.SafePrintf(
        "ATM pm 1.0=%-5u 2.5=%-5u 10=%-5u ",
        pData->atm.m1p0, pData->atm.m2p5, pData->atm.m10
        );

    gCatena.SafePrintf(
        "Dust .3=%-5u .5=%-5u 1.0=%-5u 2.5=%-5u 5=%-5u 10=%-5u%s\n",
        pData->dust.m0p3, pData->dust.m0p5, pData->dust.m1p0,
          pData->dust.m2p5, pData->dust.m5, pData->dust.m10,
        fWarmedUp ? "" : " (warmup)"
        );

    bool fEvent = false;
    if (! this->m_measurement_received)
        {
        fEvent = true;
        this->m_measurement_received = true;
        }

    if (fWarmedUp)
        {

        if (this->m_iMeasurement == 0)
            {
            this->m_fWarmedUp = true;
            fEvent = true;
            }
        const unsigned i = this->m_iMeasurement;

        if (i < kNumMeasurements)
            {
            this->m_Pm.m1p0[i] = pData->atm.m1p0;
            this->m_Pm.m2p5[i] = pData->atm.m2p5;
            this->m_Pm.m10[i] = pData->atm.m10;
            this->m_Dust.m0p3[i] = pData->dust.m0p3;
            this->m_Dust.m0p5[i] = pData->dust.m0p5;
            this->m_Dust.m1p0[i] = pData->dust.m1p0;
            this->m_Dust.m2p5[i] = pData->dust.m2p5;
            this->m_Dust.m5[i] = pData->dust.m5;
            this->m_Dust.m10[i] = pData->dust.m10;

            this->m_iMeasurement = i + 1;
            if (i + 1 == kNumMeasurements)
                fEvent = true;
            }
        }

    if (fEvent)
        this->m_fsm.eval();
    }

/****************************************************************************\
|
|   Prepare a buffer to be transmitted.
|
\****************************************************************************/

void cMeasurementLoop::fillTxBuffer(cMeasurementLoop::TxBuffer_t& b)
    {
    auto const savedLed = gLed.Set(McciCatena::LedPattern::Measuring);

    b.begin();
    Flags flag;

    flag = Flags(0);

    // insert format byte
    b.put(kMessageFormat);

    // insert a byte that will become flags later.
    std::uint8_t * const pFlag = b.getp();
    b.put(std::uint8_t(flag));

    // send Vbat
    float Vbat = gCatena.ReadVbat();
    gCatena.SafePrintf("Vbat:    %d mV\n", (int) (Vbat * 1000.0f));
    b.putV(Vbat);
    flag |= Flags::Vbat;

    // send Vdd if we can measure it.

    // vBus is sent as 5000 * v
    float Vbus = gCatena.ReadVbus();
    gCatena.SafePrintf("Vbus:    %d mV\n", (int) (Vbus * 1000.0f));
    this->setVbus(Vbus);
    b.putV(Vbus);
    flag |= Flags::Vbus;

    // send boot count
    uint32_t bootCount;
    if (gCatena.getBootCount(bootCount))
        {
        b.putBootCountLsb(bootCount);
        flag |= Flags::Boot;
        }

    if (this->m_fTempRh)
        {
        using cTempRh = decltype(this->m_TempRh);

        McciCatenaSht3x::cSHT3x::Measurements m;
        
        if (! this->m_TempRh.getTemperatureHumidity(m));

        // temperature is 2 bytes from -0x80.00 to +0x7F.FF degrees C
        // pressure is 2 bytes, hPa * 10.
        // humidity is two bytes, where 0 == 0/65535 and 0xFFFFF == 65535/65535 = 100%.
        gCatena.SafePrintf(
                "SHT3x:  T: %d RH: %d\n",
                (int) m.Temperature,
                (int) m.Humidity
                );
        b.putT(m.Temperature);
        // no method for 2-byte RH, directly encode it.
        b.put2uf((m.Humidity / 100.0f) * 65535.0f);

        flag |= Flags::TH;
        }

    // sort and process
    if (this->m_measurement_valid)
        {
        McciCatenaPMS7003::cPMS7003::Measurements<float> results;
        if (this->postProcess(results) && this->m_fSgpc3)
            {
            uint16_t tvoc = -1;
            // we'll start by triggering a measurement of the VOC sensor;
            // it's important to do this first to make sure sleep timing is
            // correct. If the command succeeds, the local variables will
            // be set to the values we just read; if it fails, they'll be -1
            if (this->m_Sgpc3Sensor.measureIAQ() != 0) {
                gCatena.SafePrintf("Error while measuring IAQ: %s\n",
                        this->m_Sgpc3Sensor.getError());
            } else {
                tvoc = this->m_Sgpc3Sensor.getTVOC();
            }
            // get the baseline value that shuold be stored in non volatile memory
            if (this->m_Sgpc3Sensor.getBaseline() != 0) {
                gCatena.SafePrintf("Error while getting Baseline: %s\n",
                        this->m_Sgpc3Sensor.getError());
            } else {
                gCatena.SafePrintf("Baseline value: %d\n",
                        this->m_Sgpc3Sensor.getBaselineValue());
            }

            // finally, let's print those to the serial console
            gCatena.SafePrintf("TVOC: %d ppb\n", tvoc);
            // and then, we'll use remainingWaitTimeMS() to ensure the correct
            // Measurement rate
            delay(this->m_Sgpc3Sensor.remainingWaitTimeMS());

            b.putLux(tvoc);
            flag |= Flags::TvocPM | Flags::Dust;
            
            b.put2uf(this->particle2uf(results.atm.m1p0));
            b.put2uf(this->particle2uf(results.atm.m2p5));
            b.put2uf(this->particle2uf(results.atm.m10));
            b.put2uf(this->particle2uf(results.dust.m0p3));
            b.put2uf(this->particle2uf(results.dust.m0p5));
            b.put2uf(this->particle2uf(results.dust.m1p0));
            b.put2uf(this->particle2uf(results.dust.m2p5));
            b.put2uf(this->particle2uf(results.dust.m5));
            b.put2uf(this->particle2uf(results.dust.m10));
            }
        }

    *pFlag = std::uint8_t(flag);

    gLed.Set(savedLed);
    }

/****************************************************************************\
|
|   Reduce a single data set
|
\****************************************************************************/

extern "C" {
  static int compare16(const void *pLeft, const void *pRight);
}

static int compare16(const void *pLeft, const void *pRight)
    {
    auto p = (const std::uint16_t *)pLeft;
    auto q = (const std::uint16_t *)pRight;

    return (int)*p - (int)*q;
    }

void cMeasurementLoop::processOneMeasurement(
    float &r,
    std::uint16_t *pv
    )
    {
    // set pointers to q1 and q3 cells by counting in symmetrically from the ends of the vector.
    // For example if kNumMeasurements is 10, then we call the q1 value pv[2], and the
    // q3 value is pv[7]; values [0],[1] are below q1, and [8],[9] are above q3.
    const std::uint16_t * const pq1 = pv + kNumMeasurements / 4;
    const std::uint16_t * const pq3 = pv + kNumMeasurements - (kNumMeasurements / 4) - 1;

    // sort pv in place.
    qsort(pv, kNumMeasurements, sizeof(pv[0]), compare16);

    // calculate IQR = q3 - q1
    std::int32_t iqr = *pq3 - *pq1;

    // calculate 1.5 IRQ. It's positive, so >> is well defined.
    std::int32_t iqr15 = (3 * iqr) >> 1;

    // calcluate the low and high limits.
    std::int32_t lowlim = pq1[0] - iqr15;
    std::int32_t highlim = pq3[0] + iqr15;

    // define left and right pointers for summation.
    const std::uint16_t *p1;
    const std::uint16_t *p2;

    // scan from left to find first value to accumulate.
    for (p1 = pv; p1 < pq1 && *p1 < lowlim; ++p1)
        ;

    // scan from right to find last value to accumulate.
    for (p2 = pv + kNumMeasurements - 1; pq3 < p2 && *p2 > highlim; --p2)
        ;

    // sum the values.
    std::uint32_t sum = 0;
    for (auto p = p1; p <= p2; ++p)
        sum += *p;

    // divide by n * 65536.0
    float const div = (p2 - p1 + 1) * 65535.0f;

    // store into r.
    r = sum / div;
    }

/****************************************************************************\
|
|   Reduce all the data
|
\****************************************************************************/

bool cMeasurementLoop::postProcess(
    McciCatenaPMS7003::cPMS7003::Measurements<float> &results
    )
    {
    std::uint16_t m[kNumMeasurements];

    processOneMeasurement(results.atm.m1p0, this->m_Pm.m1p0);
    processOneMeasurement(results.atm.m2p5, this->m_Pm.m2p5);
    processOneMeasurement(results.atm.m10,  this->m_Pm.m10);

    processOneMeasurement(results.dust.m0p3, this->m_Dust.m0p3);
    processOneMeasurement(results.dust.m0p5, this->m_Dust.m0p5);
    processOneMeasurement(results.dust.m1p0, this->m_Dust.m1p0);
    processOneMeasurement(results.dust.m2p5, this->m_Dust.m2p5);
    processOneMeasurement(results.dust.m5,   this->m_Dust.m5);
    processOneMeasurement(results.dust.m10,  this->m_Dust.m10);

    return true;
    }

/****************************************************************************\
|
|   Start uplink of data
|
\****************************************************************************/

void cMeasurementLoop::startTransmission(
    cMeasurementLoop::TxBuffer_t &b
    )
    {
    auto const savedLed = gLed.Set(McciCatena::LedPattern::Sending);

    // by using a lambda, we can access the private contents
    auto sendBufferDoneCb =
        [](void *pClientData, bool fSuccess)
            {
            auto const pThis = (cMeasurementLoop *)pClientData;
            pThis->m_txpending = false;
            pThis->m_txcomplete = true;
            pThis->m_txerr = ! fSuccess;
            pThis->m_fsm.eval();
            };

    bool fConfirmed = false;
    if (gCatena.GetOperatingFlags() &
        static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fConfirmedUplink))
        {
        gCatena.SafePrintf("requesting confirmed tx\n");
        fConfirmed = true;
        }

    this->m_txpending = true;
    this->m_txcomplete = this->m_txerr = false;

    if (! gLoRaWAN.SendBuffer(b.getbase(), b.getn(), sendBufferDoneCb, (void *)this, fConfirmed, kUplinkPort))
        {
        // uplink wasn't launched.
        this->m_txcomplete = true;
        this->m_txerr = true;
        this->m_fsm.eval();
        }
    }

void cMeasurementLoop::sendBufferDone(bool fSuccess)
    {
    this->m_txpending = false;
    this->m_txcomplete = true;
    this->m_txerr = ! fSuccess;
    this->m_fsm.eval();
    }

/****************************************************************************\
|
|   The Polling function -- 
|
\****************************************************************************/

void cMeasurementLoop::poll()
    {
    bool fEvent;

    // no need to evaluate unless something happens.
    fEvent = false;

    // if we're not active, and no request, nothing to do.
    if (! this->m_active)
        {
        if (! this->m_rqActive)
            return;

        // we're asked to go active. We'll want to eval.
        fEvent = true;
        }

    if (this->m_fTimerActive)
        {
        if ((millis() - this->m_timer_start) >= this->m_timer_delay)
            {
            this->m_fTimerActive = false;
            this->m_fTimerEvent = true;
            fEvent = true;
            }
        }

    // check the transmit time.
    if (this->m_UplinkTimer.peekTicks() != 0)
        {
        fEvent = true;
        }

    if (fEvent)
        this->m_fsm.eval();
    }

/****************************************************************************\
|
|   Update the TxCycle count. 
|
\****************************************************************************/

void cMeasurementLoop::updateTxCycleTime()
    {
    auto txCycleCount = this->m_txCycleCount;

    // update the sleep parameters
    if (txCycleCount > 1)
            {
            // values greater than one are decremented and ultimately reset to default.
            this->m_txCycleCount = txCycleCount - 1;
            }
    else if (txCycleCount == 1)
            {
            // it's now one (otherwise we couldn't be here.)
            gCatena.SafePrintf("resetting tx cycle to default: %u\n", this->m_txCycleSec_Permanent);

            this->setTxCycleTime(this->m_txCycleSec_Permanent, 0);
            }
    else
            {
            // it's zero. Leave it alone.
            }
    }

/****************************************************************************\
|
|   Handle sleep between measurements 
|
\****************************************************************************/

void cMeasurementLoop::sleep()
    {
    const bool fDeepSleep = checkDeepSleep();

    if (! this->m_fPrintedSleeping)
            this->doSleepAlert(fDeepSleep);

    if (fDeepSleep)
            this->doDeepSleep();
    }

bool cMeasurementLoop::checkDeepSleep()
    {
    bool const fDeepSleepTest = gCatena.GetOperatingFlags() &
                    static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDeepSleepTest);
    bool fDeepSleep;
    std::uint32_t const sleepInterval = this->m_UplinkTimer.getRemaining() / 1000;

    if (sleepInterval < 2)
            fDeepSleep = false;
    else if (fDeepSleepTest)
            {
            fDeepSleep = true;
            }
#ifdef USBCON
    else if (Serial.dtr())
            {
            fDeepSleep = false;
            }
#endif
    else if (gCatena.GetOperatingFlags() &
                    static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDisableDeepSleep))
            {
            fDeepSleep = false;
            }
    else if ((gCatena.GetOperatingFlags() &
                    static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fUnattended)) != 0)
            {
            fDeepSleep = true;
            }
    else
            {
            fDeepSleep = false;
            }

    return fDeepSleep;
    }

void cMeasurementLoop::doSleepAlert(bool fDeepSleep)
    {
    this->m_fPrintedSleeping = true;

    if (fDeepSleep)
        {
        bool const fDeepSleepTest = gCatena.GetOperatingFlags() &
                        static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDeepSleepTest);
        const uint32_t deepSleepDelay = fDeepSleepTest ? 10 : 30;

        gCatena.SafePrintf("using deep sleep in %u secs"
#ifdef USBCON
                            " (USB will disconnect while asleep)"
#endif
                            ": ",
                            deepSleepDelay
                            );

        // sleep and print
        gLed.Set(McciCatena::LedPattern::TwoShort);

        for (auto n = deepSleepDelay; n > 0; --n)
            {
            uint32_t tNow = millis();

            while (uint32_t(millis() - tNow) < 1000)
                {
                gCatena.poll();
                yield();
                }
            gCatena.SafePrintf(".");
            }
        gCatena.SafePrintf("\nStarting deep sleep.\n");
        uint32_t tNow = millis();
        while (uint32_t(millis() - tNow) < 100)
            {
            gCatena.poll();
            yield();
            }
        }
    else
        gCatena.SafePrintf("using light sleep\n");
    }

void cMeasurementLoop::doDeepSleep()
    {
    // bool const fDeepSleepTest = gCatena.GetOperatingFlags() &
    //                         static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDeepSleepTest);
    std::uint32_t const sleepInterval = this->m_UplinkTimer.getRemaining() / 1000;

    if (sleepInterval == 0)
        return;

    /* ok... now it's time for a deep sleep */
    gLed.Set(McciCatena::LedPattern::Off);
    this->deepSleepPrepare();

    /* sleep */
    gCatena.Sleep(sleepInterval);

    /* recover from sleep */
    this->deepSleepRecovery();

    /* and now... we're awake again. trigger another measurement */
    this->m_fsm.eval();
    }

void cMeasurementLoop::deepSleepPrepare(void)
    {
    this->m_Pms7003.end();
    Serial.end();
    Wire.end();
    SPI.end();
    if (gfFlash)
        gSPI2.end();
    }

void cMeasurementLoop::deepSleepRecovery(void)
    {
    Serial.begin();
    Wire.begin();
    SPI.begin();
    if (gfFlash)
            gSPI2.begin();
    this->m_Pms7003.begin();
    }
