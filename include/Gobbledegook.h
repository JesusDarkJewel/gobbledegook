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
// This file represents the complete interface to Gobbledegook from a stand-alone application.
//
// >>
// >>>  DISCUSSION
// >>
//
// The interface to Gobbledegook is rather simple. It consists of the following categories of functionality:
//
//     * Logging
//
//       The server defers all logging to the application. The application registers a set of logging delegates (one for each
//       log level) so it can manage the logs however it wants (syslog, console, file, an external logging service, etc.)
//
//     * Managing updates to server data
//
//       The application is required to implement two delegates (`GGKServerDataGetter` and `GGKServerDataSetter`) for sharing data
//       with the server. See standalone.cpp for an example of how this is done.
//
//       In addition, the server provides a thread-safe queue for notifications of data updates to the server. Generally, the only
//       methods an application will need to call are `ggkNofifyUpdatedCharacteristic` and `ggkNofifyUpdatedDescriptor`. The other
//       methods are provided in case an application requires extended functionality.
//
//     * Server control
//
//       A small set of methods for starting and stopping the server.
//
//     * Server state
//
//       These routines allow the application to query the server's current state. The server runs through these states during its
//       lifecycle:
//
//           EUninitialized -> EInitializing -> ERunning -> EStopping -> EStopped
//
//     * Server health
//
//       The server maintains its own health information. The states are:
//
//           EOk         - the server is A-OK
//           EFailedInit - the server had a failure prior to the ERunning state
//           EFailedRun  - the server had a failure during the ERunning state
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#pragma once

#include <memory>

#ifdef __cplusplus
extern "C"
{
#endif //__cplusplus

	// -----------------------------------------------------------------------------------------------------------------------------
	// LOGGING
	// -----------------------------------------------------------------------------------------------------------------------------

	// Type definition for callback delegates that receive log messages
	typedef void (*GGKLogReceiver)(const char *pMessage);

	// Each of these methods registers a log receiver method. Receivers are set when registered. To unregister a log receiver,
	// simply register with `nullptr`.
	void ggkLogRegisterDebug(GGKLogReceiver receiver);
	void ggkLogRegisterInfo(GGKLogReceiver receiver);
	void ggkLogRegisterStatus(GGKLogReceiver receiver);
	void ggkLogRegisterWarn(GGKLogReceiver receiver);
	void ggkLogRegisterError(GGKLogReceiver receiver);
	void ggkLogRegisterFatal(GGKLogReceiver receiver);
	void ggkLogRegisterAlways(GGKLogReceiver receiver);
	void ggkLogRegisterTrace(GGKLogReceiver receiver);

	// -----------------------------------------------------------------------------------------------------------------------------
	// SERVER DATA
	// -----------------------------------------------------------------------------------------------------------------------------

	// Type definition for a delegate that the server will use when it needs to receive data from the host application
	//
	// IMPORTANT:
	//
	// This will be called from the server's thread. Be careful to ensure your implementation is thread safe.
	//
	// Similarly, the pointer to data returned to the server should point to non-volatile memory so that the server can use it
	// safely for an indefinite period of time.
	typedef const void *(*GGKServerDataGetter)(const char *pName);

	// Type definition for a delegate that the server will use when it needs to notify the host application that data has changed
	//
	// IMPORTANT:
	//
	// This will be called from the server's thread. Be careful to ensure your implementation is thread safe.
	//
	// The data setter uses void* types to allow receipt of unknown data types from the server. Ensure that you do not store these
	// pointers. Copy the data before returning from your getter delegate.
	//
	// This method returns a non-zero value on success or 0 on failure.
	//
	// Possible failures:
	//
	//   * pName is null
	//   * pData is null
	//   * pName is not a supported value to store
	//   * Any other failure, as deemed by the delegate handler
	typedef int (*GGKServerDataSetter)(const char *pName, const void *pData);

	// -----------------------------------------------------------------------------------------------------------------------------
	// SERVER DATA UPDATE MANAGEMENT
	// -----------------------------------------------------------------------------------------------------------------------------

	// Adds an update to the front of the queue for a characteristic at the given object path
	//
	// Returns non-zero value on success or 0 on failure.
	int ggkNofifyUpdatedCharacteristic(const char *pObjectPath);

	// Adds an update to the front of the queue for a descriptor at the given object path
	//
	// Returns non-zero value on success or 0 on failure.
	int ggkNofifyUpdatedDescriptor(const char *pObjectPath);

	// Adds a named update to the front of the queue. Generally, this routine should not be used directly. Instead, use the
	// `ggkNofifyUpdatedCharacteristic()` instead.
	//
	// Returns non-zero value on success or 0 on failure.
	int ggkPushUpdateQueue(const char *pObjectPath, const char *pInterfaceName);

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
	int ggkPopUpdateQueue(char *pElement, int elementLen, int keep);

	// Returns 1 if the queue is empty, otherwise 0
	int ggkUpdateQueueIsEmpty();

	// Returns the number of entries waiting in the queue
	int ggkUpdateQueueSize();

	// Removes all entries from the queue
	void ggkUpdateQueueClear();

	// -----------------------------------------------------------------------------------------------------------------------------
	// SERVER CONTROL
	// -----------------------------------------------------------------------------------------------------------------------------

	// Blocks for up to maxAsyncInitTimeoutMS milliseconds until the server shuts down.
	//
	// If shutdown is successful, this method will return a non-zero value. Otherwise, it will return 0.
	//
	// If the server fails to stop for some reason, the thread will be killed.
	//
	// Typically, a call to this method would follow `ggkTriggerShutdown()`.
	int ggkWait();

	// Tells the server to begin the shutdown process
	//
	// The shutdown process will interrupt any currently running asynchronous operation and prevent new operations from starting.
	// Once the server has stabilized, its event processing loop is terminated and the server is cleaned up.
	//
	// `ggkGetServerRunState` will return EStopped when shutdown is complete. To block until the shutdown is complete, see
	// `ggkWait()`.
	//
	// Alternatively, you can use `ggkShutdownAndWait()` to request the shutdown and block until the shutdown is complete.
	void ggkTriggerShutdown();

	// Convenience method to trigger a shutdown (via `ggkTriggerShutdown()`) and also waits for shutdown to complete (via
	// `ggkWait()`)
	int ggkShutdownAndWait();

	// -----------------------------------------------------------------------------------------------------------------------------
	// SERVER STATE
	// -----------------------------------------------------------------------------------------------------------------------------

	// Current state of the server
	//
	// States should progress through states in this order:
	//
	//     EUninitialized -> EInitializing -> ERunning -> EStopping -> EStopped
	//
	// Note that in some cases, a server may skip one or more states, as is the case of a failed initialization where the server
	// will progress from EInitializing directly to EStopped.
	//
	// Use `ggkGetServerRunState` to retrieve the state and `ggkGetServerRunStateString` to convert a `GGKServerRunState` into a
	// human-readable string.
	enum GGKServerRunState
	{
		EUninitialized,
		EInitializing,
		ERunning,
		EStopping,
		EStopped
	};

	// Retrieve the current running state of the server
	//
	// See `GGKServerRunState` (enumeration) for more information.
	enum GGKServerRunState ggkGetServerRunState();

	// Convert a `GGKServerRunState` into a human-readable string
	const char *ggkGetServerRunStateString(enum GGKServerRunState state);

	// Convenience method to check ServerRunState for a running server
	int ggkIsServerRunning();

	// -----------------------------------------------------------------------------------------------------------------------------
	// SERVER HEALTH
	// -----------------------------------------------------------------------------------------------------------------------------

	// The current health of the server
	//
	// A running server's health will always be EOk, therefore it is only necessary to check the health status after the server
	// has shut down to determine if it was shut down due to an unhealthy condition.
	//
	// Use `ggkGetServerHealth` to retrieve the health and `ggkGetServerHealthString` to convert a `GGKServerHealth` into a
	// human-readable string.
	enum GGKServerHealth
	{
		EOk,
		EFailedInit,
		EFailedRun
	};

	// Retrieve the current health of the server
	//
	// See `GGKServerHealth` (enumeration) for more information.
	enum GGKServerHealth ggkGetServerHealth();

	// Convert a `GGKServerHealth` into a human-readable string
	const char *ggkGetServerHealthString(enum GGKServerHealth state);

#ifdef __cplusplus
}
#endif //__cplusplus

// -----------------------------------------------------------------------------------------------------------------------------
// NEW FUNCTIONS (C++ only, outside extern "C")
// -----------------------------------------------------------------------------------------------------------------------------

namespace ggk {
    class Server;
}

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
void ggkInitLogging();

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
int ggkRun(std::shared_ptr<ggk::Server> pServer, int maxAsyncInitTimeoutMS);
