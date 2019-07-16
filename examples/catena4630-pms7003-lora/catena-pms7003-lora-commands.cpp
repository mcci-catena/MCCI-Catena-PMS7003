/*

Module: catena-pms7003-lora-commands.cpp

Function:
    Command dispatch functions; separate compile due to bug in Arduino env.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

#include <Arduino.h>
#include "catena-pms7003-lora-cMeasurementLoop.h"

#include <Wire.h>
#include <Catena.h>
#include <Catena_Led.h>
#include <Catena_Log.h>
#include <Catena_Mx25v8035f.h>
#include <Catena_PollableInterface.h>
#include <Catena_TxBuffer.h>
#include <Adafruit_BME280.h>
#include <Catena-PMS7003.h>
#include <Catena-PMS7003Hal-4630.h>
#include <mcciadk_baselib.h>
#include <stdlib.h>

#include <cstdint>
using namespace McciCatena;
using namespace McciCatenaPMS7003;

extern cPMS7003 gPms7003;
extern cPMS7003Hal_4630 gPmsHal;
extern cMeasurementLoop gMeasurementLoop;

/****************************************************************************\
|
|   The commands -- called automatically from the framework after receiving
|   and parsing a command from the Serial console.
|
\****************************************************************************/

/* process "debugmask" -- args are ignored */
// argv[0] is the matched command name.
// argv[1] if present is the new mask
cCommandStream::CommandStatus cmdDebugMask(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s\n", argv[0]);
        fResult = true;
        if (argc < 2)
            pThis->printf("debug mask: 0x%08x\n", gPmsHal.getDebugFlags());
        else if (argc > 2)
            {
            fResult = false;
            pThis->printf("too many args\n");
            }
        else
            {
            std::uint32_t newMask;
            bool fOverflow;
            size_t const nArg = std::strlen(argv[1]);

            if (nArg != McciAdkLib_BufferToUint32(
                                argv[1], nArg,
                                0,
                                &newMask, &fOverflow
                                ) || fOverflow)
                {
                pThis->printf("invalid mask: %s\n", argv[1]);
                fResult = false;
                }
            else
                {
                gPmsHal.setDebugFlags(newMask);
                pThis->printf("mask is now 0x%08x\n", gPmsHal.getDebugFlags());
                }
            }

        return fResult ? cCommandStream::CommandStatus::kSuccess
                       : cCommandStream::CommandStatus::kInvalidParameter
                       ;
        }

/* process "run" or "stop" -- args are ignored */
// argv[0] is the matched command name.
cCommandStream::CommandStatus cmdRunStop(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fEnable;

        pThis->printf("%s measurement loop\n", argv[0]);

        fEnable = argv[0][0] == 'r';
        gMeasurementLoop.requestActive(fEnable);
 
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

