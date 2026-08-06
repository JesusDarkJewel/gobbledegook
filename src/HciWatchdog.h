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

#pragma once

#include <atomic>
#include <thread>
#include <cstdint>

namespace ggk {

class HciWatchdog
{
public:
    // Constructor
    HciWatchdog();

    // Destructor
    ~HciWatchdog();

    // Start the watchdog thread
    void start();

    // Stop the watchdog thread
    void stop();

    // Check if the watchdog is running
    bool isRunning() const { return watchdogThread.joinable(); }

private:
    // The watchdog thread function
    void run();

    // Stop flag
    std::atomic<bool> stopFlag{false};

    // The watchdog thread
    std::thread watchdogThread;
};

}; // namespace ggk
