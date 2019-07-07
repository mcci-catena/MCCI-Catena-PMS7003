/*

Module: catena4630-test1.ino

Function:
    Basic sketch for explorign the function of the Catena4630 and PMS7003.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

#include <Arduino.h>
#include <Wire.h>
//#include <Catena.h>
#include <Catena4630.h>
#include <Catena_Log.h>
#include <Catena_Mx25v8035f.h>
#include <Catena_PollableInterface.h>
#include <Adafruit_BME280.h>
#include <cPMS7003.h>

#include <cstdint>

extern McciCatena::Catena4630 gCatena;

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
    cPMS7003Hal_4630(std::uint32_t debugMask)
        : m_debugMask(debugMask)
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
        gCatena.registerObject(pObject);
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

        gCatena.SafePrintf("%s", buf);
        }

    virtual bool isEnabled(std::uint32_t mask) const override
        {
        return (mask == 0) || (mask & this->m_debugMask);
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
    std::uint32_t m_debugMask;
    bool        m_f5vState;
    PinState    m_reset;
    PinState    m_mode;
    };


/****************************************************************************\
|
|   A simple timer -- this uses cPollableObject because it's easier
|
\****************************************************************************/

class cTimer : public McciCatena::cPollableObject
    {
public:
    // constructor
    cTimer() {}

    // neither copyable nor movable
    cTimer(const cTimer&) = delete;
    cTimer& operator=(const cTimer&) = delete;
    cTimer(const cTimer&&) = delete;
    cTimer& operator=(const cTimer&&) = delete;

    // initialze to fire every nMillis
    bool begin(std::uint32_t nMillis);

    // stop operation
    void end();

    // poll function (updates data)
    virtual void poll() override;

    bool isready();
    std::uint32_t readTicks();
    std::uint32_t peekTicks() const;

    void debugDisplay() const
        {
        Serial.print("time="); Serial.print(this->m_time);
        Serial.print(" interval="); Serial.print(this->m_interval);
        Serial.print(" events="); Serial.print(this->m_events);
        Serial.print(" overrun="); Serial.println(this->m_overrun);
        }

private:
    std::uint32_t   m_time;
    std::uint32_t   m_interval;
    std::uint32_t   m_events;
    std::uint32_t   m_overrun;
    };

bool cTimer::begin(std::uint32_t nMillis)
    {
    this->m_interval = nMillis;
    this->m_time = millis();
    this->m_events = 0;

    // set up for polling.
    gCatena.registerObject(this);

    return true;
    }

void cTimer::poll() /* override */
    {
    auto const tNow = millis();

    if (tNow - this->m_time >= this->m_interval)
        {
        this->m_time += this->m_interval;
        ++this->m_events;

        /* if this->m_time is now in the future, we're done */
        if (std::int32_t(tNow - this->m_time) < std::int32_t(this->m_interval))
            return;

        // rarely, we need to do arithmetic. time and events are in sync.
        // arrange for m_time to be greater than tNow, and adjust m_events
        // accordingly.
        std::uint32_t const tDiff = tNow - this->m_time;
        std::uint32_t const nTicks = tDiff / this->m_interval;
        this->m_events += nTicks;
        this->m_time += nTicks * this->m_interval;
        this->m_overrun += nTicks;
        }
    }

bool cTimer::isready()
    {
    return this->readTicks() != 0;
    }

std::uint32_t cTimer::readTicks()
    {
    auto const result = this->m_events;
    this->m_events = 0;
    return result;
    }

std::uint32_t cTimer::peekTicks() const
    {
    return this->m_events;
    }

/****************************************************************************\
|
|   Variables.
|
\****************************************************************************/

using namespace McciCatena;
using Catena = Catena4630;

Catena gCatena;
Catena::LoRaWAN gLoRaWAN;

cTimer ledTimer;

SPIClass gSPI2(
    Catena::PIN_SPI2_MOSI,
    Catena::PIN_SPI2_MISO,
    Catena::PIN_SPI2_SCK
    );

//   The flash
Catena_Mx25v8035f gFlash;
bool gfFlash;

cPMS7003Hal_4630 gPmsHal { cPMS7003::DebugFlags::kError | cPMS7003::DebugFlags::kTrace };
cPMS7003 gPms7003 { Serial2, gPmsHal };

// forward reference to the command functions
cCommandStream::CommandFn cmdBegin;
cCommandStream::CommandFn cmdEnd;
cCommandStream::CommandFn cmdOff;
cCommandStream::CommandFn cmdReset;
cCommandStream::CommandFn cmdHwSleep;
cCommandStream::CommandFn cmdSleep;
cCommandStream::CommandFn cmdPassive;
cCommandStream::CommandFn cmdNormal;
cCommandStream::CommandFn cmdMeasure;
cCommandStream::CommandFn cmdWake;
cCommandStream::CommandFn cmdStats;

// the measurement callback.
cPMS7003::MeasurementCb_t measurementAvailable;

// the individual commmands are put in this table
static const cCommandStream::cEntry sMyExtraCommmands[] =
        {
        { "begin", cmdBegin },
        { "end", cmdEnd },
        { "off", cmdOff },
        { "reset", cmdReset },
        { "hwsleep", cmdHwSleep },
        { "sleep", cmdSleep },
        { "passive", cmdPassive },
        { "normal", cmdNormal },
        { "measure", cmdMeasure },
        { "wake", cmdWake },
        { "stats", cmdStats },
        // other commands go here....
        };

/* a top-level structure wraps the above and connects to the system table */
/* it optionally includes a "first word" so you can for sure avoid name clashes */
static cCommandStream::cDispatch
sMyExtraCommands_top(
        sMyExtraCommmands,          /* this is the pointer to the table */
        sizeof(sMyExtraCommmands),  /* this is the size of the table */
        nullptr                     /* this is no "first word" for all the commands in this table */
        );

/****************************************************************************\
|
|   Setup
|
\****************************************************************************/

void setup()
    {
    Serial.begin(115200);
    while (! Serial)
        delay(1);

    gCatena.begin();
    setup_printSignOn();

    setup_flash();
    setup_radio();
    setup_pms7003();
    setup_commands();
    }

void setup_flash(void)
    {
    if (gFlash.begin(&gSPI2, Catena::PIN_SPI2_FLASH_SS))
        {
        gfFlash = true;
        gFlash.powerDown();
        gCatena.SafePrintf("FLASH found, put power down\n");
        }
    else
        {
        gfFlash = false;
        gFlash.end();
        gSPI2.end();
        gCatena.SafePrintf("No FLASH found: check hardware\n");
        }
    }

static constexpr const char *filebasename(const char *s)
    {
    const char *pName = s;

    for (auto p = s; *p != '\0'; ++p)
        {
        if (*p == '/' || *p == '\\')
            pName = p + 1;
        }
    return pName;
    }

void setup_printSignOn()
    {
    static const char dashes[] = "------------------------------------";

    gCatena.SafePrintf("\n%s%s\n", dashes, dashes);

    gCatena.SafePrintf("This is %s.\n", filebasename(__FILE__));
    gCatena.SafePrintf("System clock rate is %u.%03u MHz\n",
        ((unsigned)gCatena.GetSystemClockRate() / (1000*1000)),
        ((unsigned)gCatena.GetSystemClockRate() / 1000 % 1000)
        );
    gCatena.SafePrintf("Enter 'help' for a list of commands.\n");
    gCatena.SafePrintf("(remember to select 'Line Ending: Newline' at the bottom of the monitor window.)\n");

    gCatena.SafePrintf("%s%s\n" "\n", dashes, dashes);
    }

void setup_radio()
    {
    gLoRaWAN.begin(&gCatena);
    gCatena.registerObject(&gLoRaWAN);
    }

void setup_pms7003()
    {
    gPms7003.begin();
    gPms7003.setCallback(measurementAvailable, nullptr);
    }

void setup_commands()
    {
    /* add our application-specific commands */
    gCatena.addCommands(
        /* name of app dispatch table, passed by reference */
        sMyExtraCommands_top,
        /*
        || optionally a context pointer using static_cast<void *>().
        || normally only libraries (needing to be reentrant) need
        || to use the context pointer.
        */
        nullptr
        );
    }

/****************************************************************************\
|
|   Loop
|
\****************************************************************************/

void loop()
    {
    gCatena.poll();
    }

/****************************************************************************\
|
|   Got a measurement
|
\****************************************************************************/

void measurementAvailable(
    void *pUserData,
    const cPMS7003::Measurements<std::uint16_t> *pData
    )
    {
    gCatena.SafePrintf(
        "CF1 pm 1.0=%-5u 2.5=%-5u 10=%-5u ",
        pData->cf1.m1p0, pData->cf1.m2p5, pData->cf1.m10
        );

    gCatena.SafePrintf(
        "ATM pm 1.0=%-5u 2.5=%-5u 10=%-5u ",
        pData->atm.m1p0, pData->atm.m2p5, pData->atm.m10
        );

    gCatena.SafePrintf(
        "Dust .3=%-5u .5=%-5u 1.0=%-5u 2.5=%-5u 5=%-5u 10=%-5u\n",
        pData->dust.m0p3, pData->dust.m0p5, pData->dust.m1p0,
          pData->dust.m2p5, pData->dust.m5, pData->dust.m10
        );
    }

/****************************************************************************\
|
|   The commands -- called automatically from the framework after receiving
|   and parsing a command from the Serial console.
|
\****************************************************************************/

/* process "begin" -- args are ignored */
// argv[0] is the matched command name.
// argv[1..argc-1] are the (ignored) arguments
cCommandStream::CommandStatus cmdBegin(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s: ", argv[0]);
        fResult = gPms7003.begin();

        pThis->printf("%s\n", fResult ? "success" : "failed");

        return cCommandStream::CommandStatus::kSuccess;
        }

/* process "end" -- args are ignored */
// argv[0] is the matched command name.
// argv[1..argc-1] are the (ignored) arguments
cCommandStream::CommandStatus cmdEnd(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s\n", argv[0]);
        gPms7003.end();

        return cCommandStream::CommandStatus::kSuccess;
        }

/* process "off" -- args are ignored */
// argv[0] is the matched command name.
// argv[1..argc-1] are the (ignored) arguments
cCommandStream::CommandStatus cmdOff(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s\n", argv[0]);
        gPms7003.requestOff();

        return cCommandStream::CommandStatus::kSuccess;
        }

/* process "reset" -- args are ignored */
// argv[0] is the matched command name.
// argv[1..argc-1] are the (ignored) arguments
cCommandStream::CommandStatus cmdReset(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s\n", argv[0]);
        gPms7003.requestReset();

        return cCommandStream::CommandStatus::kSuccess;
        }

/* process "hwsleep" -- args are ignored */
// argv[0] is the matched command name.
// argv[1..argc-1] are the (ignored) arguments
cCommandStream::CommandStatus cmdHwSleep(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s\n", argv[0]);
        gPms7003.requestHwSleep();

        return cCommandStream::CommandStatus::kSuccess;
        }

/* process "sleep" -- args are ignored */
// argv[0] is the matched command name.
// argv[1..argc-1] are the (ignored) arguments
cCommandStream::CommandStatus cmdSleep(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s\n", argv[0]);
        gPms7003.requestSleep();

        return cCommandStream::CommandStatus::kSuccess;
        }

/* process "passive" -- args are ignored */
// argv[0] is the matched command name.
// argv[1..argc-1] are the (ignored) arguments
cCommandStream::CommandStatus cmdPassive(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s\n", argv[0]);
        gPms7003.requestPassive();

        return cCommandStream::CommandStatus::kSuccess;
        }

/* process "normal" -- args are ignored */
// argv[0] is the matched command name.
// argv[1..argc-1] are the (ignored) arguments
cCommandStream::CommandStatus cmdNormal(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s\n", argv[0]);
        gPms7003.requestNormal();

        return cCommandStream::CommandStatus::kSuccess;
        }

/* process "measure" -- args are ignored */
// argv[0] is the matched command name.
// argv[1..argc-1] are the (ignored) arguments
cCommandStream::CommandStatus cmdMeasure(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s\n", argv[0]);
        gPms7003.requestMeasure();

        return cCommandStream::CommandStatus::kSuccess;
        }

/* process "wake" -- args are ignored */
// argv[0] is the matched command name.
// argv[1..argc-1] are the (ignored) arguments
cCommandStream::CommandStatus cmdWake(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s\n", argv[0]);
        gPms7003.eventWake();

        return cCommandStream::CommandStatus::kSuccess;
        }

/* process "stats" -- args are ignored */
// argv[0] is the matched command name.
// argv[1..argc-1] are the (ignored) arguments
cCommandStream::CommandStatus cmdStats(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;
        const auto stats = gPms7003.getRxStats();
     
        pThis->printf("%s\n", argv[0]);
        pThis->printf("BYTES: In=%u Drops=%u  MSG: Drops=%u CsErr=%u Good=%u\n",
            stats.CharIn, stats.CharDrops, stats.MsgDrops, stats.BadChecksum, stats.GoodMsg
            );

        return cCommandStream::CommandStatus::kSuccess;
        }
