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
// The methods in this file represent the complete external C interface for a Gobbledegook server.
//
// >>
// >>>  DISCUSSION
// >>
//
// Although Gobbledegook requires customization (see Server.cpp), it still maintains a public interface (this file.) This keeps the
// interface to the server simple and compact, while also allowing the server to be built as a library if the developer so chooses.
//
// As an alternative, it is also possible to publish customized Gobbledegook servers, allowing others to use the server through the
// public interface defined in this file.
//
// In addition, this interface is compatible with the C language, allowing non-C++ programs to interface with Gobbledegook, even
// though Gobbledgook is a C++ codebase. This should also simplify the use of this interface with other languages, such as Swift.
//
// The interface below has the following categories:
//
//     Log registration - used to register methods that accept all Gobbledegook logs
//     Update queue management - used for notifying the server that data has been updated
//     Server state - used to track the server's current running state and health
//     Server control - running and stopping the server
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <string.h>
#include <string>
#include <thread>
#include <memory>
#include <deque>
#include <mutex>

#include "Init.h"
#include "Logger.h"
#include "Server.h"

namespace ggk
{
	// During initialization, we'll check for complation at this interval
	static const int kMaxAsyncInitCheckIntervalMS = 10;

	// Our server thread
	static std::thread serverThread;

	// The current server state
	volatile static GGKServerRunState serverRunState = EUninitialized;

	// The current server health
	volatile static GGKServerHealth serverHealth = EOk;

	// We store the old GLib print handler and error print handler so we can restore if
	static GPrintFunc printHandlerGLib;
	static GPrintFunc printerrHandlerGLib;
	static GLogFunc logHandlerGLib;

	// Our update queue
	typedef std::tuple<std::string, std::string> QueueEntry;
	std::deque<QueueEntry> updateQueue;
	std::mutex updateQueueMutex;

	// Internal method to set the run state of the server
	void setServerRunState(GGKServerRunState newState)
	{
		Logger::status(SSTR << "** SERVER RUN STATE CHANGED: " << ggkGetServerRunStateString(serverRunState) << " -> " << ggkGetServerRunStateString(newState));
		serverRunState = newState;
	}

	// Internal method to set the health of the server
	void setServerHealth(GGKServerHealth newHealth)
	{
		Logger::status(SSTR << "** SERVER HEALTH CHANGED: " << ggkGetServerHealthString(serverHealth) << " -> " << ggkGetServerHealthString(newHealth));
		serverHealth = newHealth;
	}
}; // namespace ggk

using namespace ggk;

// ---------------------------------------------------------------------------------------------------------------------------------
//  _                                  _     _             _   _
// | |    ___   __ _    _ __ ___  __ _(_)___| |_ _ __ __ _| |_(_) ___  _ ___
// | |   / _ \ / _` |  | '__/ _ \/ _` | / __| __| '__/ _` | __| |/ _ \| '_  |
// | |__| (_) | (_| |  | | |  __/ (_| | \__ \ |_| | | (_| | |_| | (_) | | | |
// |_____\___/ \__, |  |_|  \___|\__, |_|___/\__|_|  \__,_|\__|_|\___/|_| |_|
//             |___/             |___/
//
// ---------------------------------------------------------------------------------------------------------------------------------

void ggkLogRegisterDebug(GGKLogReceiver receiver) { Logger::registerDebugReceiver(receiver); }
void ggkLogRegisterInfo(GGKLogReceiver receiver) { Logger::registerInfoReceiver(receiver); }
void ggkLogRegisterStatus(GGKLogReceiver receiver) { Logger::registerStatusReceiver(receiver); }
void ggkLogRegisterWarn(GGKLogReceiver receiver) { Logger::registerWarnReceiver(receiver); }
void ggkLogRegisterError(GGKLogReceiver receiver) { Logger::registerErrorReceiver(receiver); }
void ggkLogRegisterFatal(GGKLogReceiver receiver) { Logger::registerFatalReceiver(receiver); }
void ggkLogRegisterTrace(GGKLogReceiver receiver) { Logger::registerTraceReceiver(receiver); }
void ggkLogRegisterAlways(GGKLogReceiver receiver) { Logger::registerAlwaysReceiver(receiver); }

// ---------------------------------------------------------------------------------------------------------------------------------
//  _   _           _       _                                                                                                     _
// | | | |_ __   __| | __ _| |_ ___     __ _ _   _  ___ _   _  ___    _ __ ___   __ _ _ __   __ _  __ _  ___ _ __ ___   ___ _ __ | |_
// | | | | '_ \ / _` |/ _` | __/ _ \   / _` | | | |/ _ \ | | |/ _ \  | '_ ` _ \ / _` | '_ \ / _` |/ _` |/ _ \ '_ ` _ \ / _ \ '_ \| __|
// | |_| | |_) | (_| | (_| | ||  __/  | (_| | |_| |  __/ |_| |  __/  | | | | | | (_| | | | | (_| | (_| |  __/ | | | | |  __/ | | | |_
//  \___/| .__/ \__,_|\__,_|\__\___|   \__, |\__,_|\___|\__,_|\___|  |_| |_| |_|\__,_|_| |_|\__,_|\__, |\___|_| |_| |_|\___|_| |_|\__|
//       |_|                              |_|                                                     |___/
//
// Push/pop update notifications onto a queue. As these methods are where threads collide (i.e., this is how they communicate),
// these methods are thread-safe.
// ---------------------------------------------------------------------------------------------------------------------------------

// Adds an update to the front of the queue for a characteristic at the given object path
//
// Returns non-zero value on success or 0 on failure.
int ggkNofifyUpdatedCharacteristic(const char *pObjectPath)
{
	return ggkPushUpdateQueue(pObjectPath, "org.bluez.GattCharacteristic1") != 0;
}

// Adds an update to the front of the queue for a descriptor at the given object path
//
// Returns non-zero value on success or 0 on failure.
int ggkNofifyUpdatedDescriptor(const char *pObjectPath)
{
	return ggkPushUpdateQueue(pObjectPath, "org.bluez.GattDescriptor1") != 0;
}

// Adds a named update to the front of the queue. Generally, this routine should not be used directly. Instead, use the
// `ggkNofifyUpdatedCharacteristic()` instead.
//
// Returns non-zero value on success or 0 on failure.
int ggkPushUpdateQueue(const char *pObjectPath, const char *pInterfaceName)
{
	QueueEntry t(pObjectPath, pInterfaceName);

	std::lock_guard<std::mutex> guard(updateQueueMutex);
	updateQueue.push_front(t);
	return 1;
}

// Get the next update from the back of the queue and returns the element in `element` as a string in the format:
//
//     "com/object/path|com.interface.name"
//
// If the queue is empty, this method returns `0` and does nothing.
//
// `elementLen` is the size of the `element` buffer in bytes. If the resulting string (including the null terminator) will not
// fit within `elementLen` bytes, the method returns `-1` and does nothing.
//
// If `keep` is set to non-zero, the entry is not removed and will be retrieved again on the next call. Otherwise, the element
// is removed.
//
// Returns 1 on success, 0 if the queue is empty, -1 on error (such as the length too small to store the element)
int ggkPopUpdateQueue(char *pElementBuffer, int elementLen, int keep)
{
	std::string result;

	{
		std::lock_guard<std::mutex> guard(updateQueueMutex);

		// Check for an empty queue
		if (updateQueue.empty()) { return 0; }

		// Get the last element
		QueueEntry t = updateQueue.back();

		// Get the result string
		result = std::get<0>(t) + "|" + std::get<1>(t);

		// Ensure there's enough room for it
		if (result.length() + 1 > static_cast<size_t>(elementLen)) { return -1; }

		if (keep == 0)
		{
			updateQueue.pop_back();
		}
	}

	// Copy the element string
	memcpy(pElementBuffer, result.c_str(), result.length() + 1);

	return 1;
}

// Returns 1 if the queue is empty, otherwise 0
int ggkUpdateQueueIsEmpty()
{
	std::lock_guard<std::mutex> guard(updateQueueMutex);
	return updateQueue.empty() ? 1 : 0;
}

// Returns the number of entries waiting in the queue
int ggkUpdateQueueSize()
{
	std::lock_guard<std::mutex> guard(updateQueueMutex);
	return updateQueue.size();
}

// Removes all entries from the queue
void ggkUpdateQueueClear()
{
	std::lock_guard<std::mutex> guard(updateQueueMutex);
	updateQueue.clear();
}

// ---------------------------------------------------------------------------------------------------------------------------------
//  ____                     _        _
// |  _ \ _   _ _ __     ___| |_ __ _| |_ ___
// | |_) | | | | '_ \   / __| __/ _` | __/ _ )
// |  _ <| |_| | | | |  \__ \ || (_| | ||  __/
// |_| \_\\__,_|_| |_|  |___/\__\__,_|\__\___|
//
// Methods for maintaining and reporting the state of the server
// ---------------------------------------------------------------------------------------------------------------------------------

// Retrieve the current running state of the server
//
// See `GGKServerRunState` (enumeration) for more information.
GGKServerRunState ggkGetServerRunState()
{
	return serverRunState;
}

// Convert a `GGKServerRunState` into a human-readable string
const char *ggkGetServerRunStateString(GGKServerRunState state)
{
	switch(state)
	{
		case EUninitialized: return "Uninitialized";
		case EInitializing: return "Initializing";
		case ERunning: return "Running";
		case EStopping: return "Stopping";
		case EStopped: return "Stopped";
		default: return "Unknown";
	}
}

// Convenience method to check ServerRunState for a running server
int ggkIsServerRunning()
{
	return serverRunState <= ERunning ? 1 : 0;
}

// ---------------------------------------------------------------------------------------------------------------------------------
//  ____                              _                _ _   _
// / ___|  ___ _ ____   _____ _ __   | |__   ___  __ _| | |_| |___
// \___ \ / _ \ '__\ \ / / _ \ '__|  | '_ \ / _ \/ _` | | __| '_  |
//  ___) |  __/ |   \ V /  __/ |     | | | |  __/ (_| | | |_| | | |
// |____/ \___|_|    \_/ \___|_|     |_| |_|\___|\__,_|_|\__|_| |_|
//
// Methods for maintaining and reporting the health of the server
// ---------------------------------------------------------------------------------------------------------------------------------

// Retrieve the current health of the server
//
// See `GGKServerHealth` (enumeration) for more information.
GGKServerHealth ggkGetServerHealth()
{
	return serverHealth;
}

// Convert a `GGKServerHealth` into a human-readable string
const char *ggkGetServerHealthString(GGKServerHealth state)
{
	switch(state)
	{
		case EOk: return "Ok";
		case EFailedInit: return "Failed initialization";
		case EFailedRun: return "Failed run";
		default: return "Unknown";
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
//  ____  _                 _   _
// / ___|| |_ ___  _ __    | |_| |__   ___    ___  ___ _ ____   _____ _ __
// \___ \| __/ _ \| '_ \   | __| '_ \ / _ \  / __|/ _ \ '__\ \ / / _ \ '__|
//  ___) | || (_) | |_) |  | |_| | | |  __/  \__ \  __/ |   \ V /  __/ |
// |____/ \__\___/| .__/    \__|_| |_|\___|  |___/\___|_|    \_/ \___|_|
//                |_|
//
// ---------------------------------------------------------------------------------------------------------------------------------

// Tells the server to begin the shutdown process
//
// The shutdown process will interrupt any currently running asynchronous operation and prevent new operations from starting.
// Once the server has stabilized, its event processing loop is terminated and the server is cleaned up.
//
// `ggkGetServerRunState` will return EStopped when shutdown is complete. To block until the shutdown is complete, see
// `ggkWait()`.
//
// Alternatively, you can use `ggkShutdownAndWait()` to request the shutdown and block until the shutdown is complete.
void ggkTriggerShutdown()
{
	shutdown();
}

// Convenience method to trigger a shutdown (via `ggkTriggerShutdown()`) and also waits for shutdown to complete (via
// `ggkWait()`)
int ggkShutdownAndWait()
{
	if (ggkIsServerRunning() != 0)
	{
		// Tell the server to shut down
		ggkTriggerShutdown();
	}

	// Block until it has shut down completely
	return ggkWait();
}

// ---------------------------------------------------------------------------------------------------------------------------------
// __        __    _ _
// \ \      / /_ _(_) |_     ___  _ __     ___  ___ _ ____   _____ _ __
//  \ \ /\ / / _` | | __|   / _ \| '_ \   / __|/ _ \ '__\ \ / / _ \ '__|
//   \ V  V / (_| | | |_   | (_) | | | |  \__ \  __/ |   \ V /  __/ |
//    \_/\_/ \__,_|_|\__|   \___/|_| |_|  |___/\___|_|    \_/ \___|_|
//
// ---------------------------------------------------------------------------------------------------------------------------------

// Blocks for up to maxAsyncInitTimeoutMS milliseconds until the server shuts down.
//
// If shutdown is successful, this method will return a non-zero value. Otherwise, it will return 0.
//
// If the server fails to stop for some reason, the thread will be killed.
//
// Typically, a call to this method would follow `ggkTriggerShutdown()`.
int ggkWait()
{
	int result = 0;
	try
	{
		if (ggkGetServerRunState() <= ERunning)
		{
			Logger::info("Waiting for GGK server to stop");
		}

		if (serverThread.joinable())
		{
			serverThread.join();
		}

		result = 1;
	}
	catch(std::system_error &ex)
	{
		if (ex.code() == std::errc::invalid_argument)
		{
			Logger::warn(SSTR << "Server thread was not joinable during ggkWait(): " << ex.what());
		}
		else if (ex.code() == std::errc::no_such_process)
		{
			Logger::warn(SSTR << "Server thread was not valid during ggkWait(): " << ex.what());
		}
		else if (ex.code() == std::errc::resource_deadlock_would_occur)
		{
			Logger::warn(SSTR << "Deadlock avoided in call to ggkWait() (did the server thread try to stop itself?): " << ex.what());
		}
		else
		{
			Logger::warn(SSTR << "Unknown system_error code (" << ex.code() << ") during ggkWait(): " << ex.what());
		}
	}

	// Restore the GLib output functions
	g_set_print_handler(printHandlerGLib);
	g_set_printerr_handler(printerrHandlerGLib);
	g_log_set_default_handler(logHandlerGLib, nullptr);

	return result;
}

// ---------------------------------------------------------------------------------------------------------------------------------
//  _    _           _   _                 _
// | |  (_)_ _  __ _| |_(_)___ _ _    __ _(_)_ _  __ _
// | |__| | ' \/ _` |  _| / _ \ ' \  / _` | | ' \/ _` |
// |____|_|_||_\__,_|\__|_\___/_||_| \__,_|_|_||_\__, |
//                                               |___/
//
// New functions for explicit logging initialisation and server run
// ---------------------------------------------------------------------------------------------------------------------------------

/**
 * Initialise the logging subsystem.
 *
 * This function must be called before any other GGK function, typically at the
 * very start of your program. It sets up the internal logging infrastructure
 * (spdlog) and configures the default log levels and sinks.
 *
 * After calling this, you may register custom log receivers using the
 * ggkLogRegister* functions.
 */
void ggkInitLogging()
{
	// Redirect GLib output to this log method
	printHandlerGLib = g_set_print_handler([](const gchar *string)
	{
		Logger::info(string);
	});
	printerrHandlerGLib = g_set_printerr_handler([](const gchar *string)
	{
		Logger::error(string);
	});
	logHandlerGLib = g_log_set_default_handler([](const gchar *log_domain, GLogLevelFlags log_levels, const gchar *message, gpointer /*user_data*/)
	{
		std::string str = std::string(log_domain) + ": " + message;
		if ((log_levels & (G_LOG_FLAG_RECURSION|G_LOG_FLAG_FATAL)) != 0)
		{
			Logger::fatal(str);
		}
		else if ((log_levels & (G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_ERROR)) != 0)
		{
			Logger::error(str);
		}
		else if ((log_levels & G_LOG_LEVEL_WARNING) != 0)
		{
			Logger::warn(str);
		}
		else if ((log_levels & G_LOG_LEVEL_DEBUG) != 0)
		{
			Logger::debug(str);
		}
		else
		{
			Logger::info(str);
		}
	}, nullptr);
}

/**
 * Run the GATT server using an existing server instance.
 *
 * This function takes ownership of the provided Server instance (via shared_ptr),
 * registers it with BlueZ over D-Bus, starts the main event loop, and begins
 * advertising the configured services. It blocks until the server is fully
 * initialised or the timeout expires.
 *
 * @param pServer                Shared pointer to a Server instance (or a derived class).
 *                               Must not be null. The server should have all its
 *                               services, characteristics and descriptors already
 *                               described (typically in its constructor).
 * @param maxAsyncInitTimeoutMS  Maximum time (in milliseconds) to wait for
 *                               asynchronous initialisation to complete.
 *
 * @return  1 on success, 0 on failure.
 *
 * @see ggkInitLogging, ggkWait, ggkTriggerShutdown
 */
int ggkRun(std::shared_ptr<Server> pServer, int maxAsyncInitTimeoutMS)
{
	if (!pServer) {
		Logger::error("ggkRun() called with null Server pointer");
		return 0;
	}

	try
	{
		// Use the provided server instance
		TheServer = pServer;

		// Start our server thread
		try
		{
			serverThread = std::thread(runServerThread);
		}
		catch(std::system_error &ex)
		{
			Logger::error(SSTR << "Server thread was unable to start (code " << ex.code() << ") during ggkRun(): " << ex.what());

			setServerRunState(EStopped);
			return 0;
		}

		// Waits for the server to pass the EInitializing state
		int retryTimeMS = 0;
		while (retryTimeMS < maxAsyncInitTimeoutMS && ggkGetServerRunState() <= EInitializing)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(kMaxAsyncInitCheckIntervalMS));
			retryTimeMS += kMaxAsyncInitCheckIntervalMS;
		}

		// If something went wrong, shut down
		if (retryTimeMS >= maxAsyncInitTimeoutMS)
		{
			Logger::error("GGK server initialization timed out");

			setServerHealth(EFailedInit);

			shutdown();
		}

		// If something went wrong, shut down if we've not already done so
		if (ggkGetServerRunState() != ERunning)
		{
			if (!ggkWait())
			{
				Logger::warn(SSTR << "Unable to stop the server after an error in ggkRun()");
			}

			return 0;
		}

		// Everything looks good
		Logger::trace("GGK server has started");
		return 1;
	}
	catch(...)
	{
		Logger::error(SSTR << "Unknown exception during ggkRun()");
		return 0;
	}
}