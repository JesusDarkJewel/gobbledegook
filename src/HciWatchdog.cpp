// Copyright 2017-2019 Paul Nettle
//
// This file is part of Gobbledegook.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  INSIDE THIS FILE
// >>
//
// HCI Watchdog - monitors the health of the HCI socket connection
//
// >>
// >>>  DISCUSSION
// >>
//
// The watchdog periodically sends a command to the HCI adapter to verify it is still responsive.
// If the adapter fails to respond after multiple attempts, the watchdog terminates the process
// to allow systemd to restart it.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <chrono>
#include <thread>
#include <cstdlib>

#include "HciWatchdog.h"
#include "HciAdapter.h"
#include "Mgmt.h"
#include "Logger.h"

namespace ggk {

HciWatchdog::HciWatchdog() = default;

HciWatchdog::~HciWatchdog()
{
    stop();
}

void HciWatchdog::start()
{
    if (isRunning())
    {
        Logger::warn("HciWatchdog is already running");
        return;
    }

    stopFlag = false;
    watchdogThread = std::thread(&HciWatchdog::run, this);
}

void HciWatchdog::stop()
{
    if (!isRunning())
    {
        return;
    }

    stopFlag = true;
    if (watchdogThread.joinable())
    {
        watchdogThread.join();
    }
}

void HciWatchdog::run()
{
    int consecutiveFailures = 0;

    while (!stopFlag)
    {
        // Sleep for 30 seconds between checks (in 1-second increments to allow for clean shutdown)
        for (int i = 0; i < 30 && !stopFlag; ++i)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (stopFlag)
        {
            break;
        }

        // Send a Read Controller Information command to check if the adapter is responsive
        HciAdapter::HciHeader request;
        request.code = Mgmt::EReadControllerInformationCommand;
        request.controllerId = 0;
        request.dataSize = 0;

        if (HciAdapter::getInstance().sendCommand(request))
        {
            if (consecutiveFailures != 0)
            {
                Logger::info("HCI watchdog recovered");
            }
            consecutiveFailures = 0;
            Logger::debug("HCI watchdog check passed");
        }
        else
        {
            ++consecutiveFailures;
            Logger::error(SSTR << "HCI watchdog check failed (" << consecutiveFailures << "/3)");

            if (consecutiveFailures >= 3)
            {
                Logger::error("HCI watchdog is terminating BusOTS; systemd will restart it");
                std::_Exit(1);
            }
        }
    }
}

}; // namespace ggk
