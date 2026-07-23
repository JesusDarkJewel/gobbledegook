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
// This file contains various functions for interacting with Bluetooth Management interface, which provides adapter configuration.
//
// >>
// >>>  DISCUSSION
// >>
//
// We only cover the basics here. If there are configuration features you need that aren't supported (such as configuring BR/EDR),
// then this would be a good place for them.
//
// Note that this class relies on the `HciAdapter`, which is a very primitive implementation. Use with caution.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <string.h>

#include "Mgmt.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

// Construct the Mgmt device
//
// Set `controllerIndex` to the zero-based index of the device as recognized by the OS. If this parameter is omitted, the index
// of the first device (0) will be used.
Mgmt::Mgmt(uint16_t controllerIndex)
: controllerIndex(controllerIndex)
{
	HciAdapter::getInstance().sync(controllerIndex);
}

// Set the adapter name and short name
//
// The inputs `name` and `shortName` may be truncated prior to setting them on the adapter. To ensure that `name` and
// `shortName` conform to length specifications prior to calling this method, see the constants `kMaxAdvertisingNameLength` and
// `kMaxAdvertisingShortNameLength`. In addition, the static methods `truncateName()` and `truncateShortName()` may be helpful.
//
// Returns true on success, otherwise false
bool Mgmt::setName(std::string name, std::string shortName)
{
	// Ensure their lengths are okay
	name = truncateName(name);
	shortName = truncateShortName(shortName);

	struct SRequest : HciAdapter::HciHeader
	{
		char name[249];
		char shortName[11];
	} __attribute__((packed));

	SRequest request;
	request.code = Mgmt::ESetLocalNameCommand;
	request.controllerId = controllerIndex;
	request.dataSize = sizeof(SRequest) - sizeof(HciAdapter::HciHeader);

	memset(request.name, 0, sizeof(request.name));
	snprintf(request.name, sizeof(request.name), "%s", name.c_str());

	memset(request.shortName, 0, sizeof(request.shortName));
	snprintf(request.shortName, sizeof(request.shortName), "%s", shortName.c_str());

	if (!HciAdapter::getInstance().sendCommand(request))
	{
		Logger::warn(SSTR << "  + Failed to set name");
		return false;
	}

	return true;
}

// Sets discoverable mode
// 0x00 disables discoverable
// 0x01 enables general discoverable
// 0x02 enables limited discoverable
// Timeout is the time in seconds. For 0x02, the timeout value is required.
bool Mgmt::setDiscoverable(uint8_t disc, uint16_t timeout)
{
	struct SRequest : HciAdapter::HciHeader
	{
		uint8_t disc;
		uint16_t timeout;
	} __attribute__((packed));

	SRequest request;
	request.code = Mgmt::ESetDiscoverableCommand;
	request.controllerId = controllerIndex;
	request.dataSize = sizeof(SRequest) - sizeof(HciAdapter::HciHeader);
	request.disc = disc;
	request.timeout = timeout;

	if (!HciAdapter::getInstance().sendCommand(request))
	{
		Logger::warn(SSTR << "  + Failed to set discoverable");
		return false;
	}

	return true;
}

// Set a setting state to 'newState'
//
// Many settings are set the same way, this is just a convenience routine to handle them all
//
// Returns true on success, otherwise false
bool Mgmt::setState(uint16_t commandCode, uint16_t controllerId, uint8_t newState)
{
	struct SRequest : HciAdapter::HciHeader
	{
		uint8_t state;
	} __attribute__((packed));

	SRequest request;
	request.code = commandCode;
	request.controllerId = controllerId;
	request.dataSize = sizeof(SRequest) - sizeof(HciAdapter::HciHeader);
	request.state = newState;

	if (!HciAdapter::getInstance().sendCommand(request))
	{
		Logger::warn(SSTR << "  + Failed to set " << HciAdapter::getCommandCodeName(commandCode) << " state to: " << static_cast<int>(newState));
		return false;
	}

	return true;
}

// bool Mgmt::addAdvertisingTemp()
// {
//     struct SRequest : HciAdapter::HciHeader
//     {
//         uint8_t ttt[16];
//     } __attribute__((packed));

//     SRequest request;
//     request.code = Mgmt::EAddAdvertisingCommand;
//     request.controllerId = 0;
//     request.dataSize = sizeof(SRequest) - sizeof(HciAdapter::HciHeader);

//     // Hardcoded packet: instance=1, flags=0x01, duration=0, timeout=0,
//     // adLen=5, scanRspLen=0, adData=04 FF FF FF 00
//     // This matches the successful test: 3E 00 00 00 10 00 01 01 00 00 00 00 00 00 00 05 00 04 FF FF FF 00
//     static const uint8_t raw[] = {0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x00,0x04,0xFF,0xFF,0xFF,0x00};
//     memcpy(request.ttt, raw, sizeof(raw));

//     if (!HciAdapter::getInstance().sendCommand(request)) {
//         Logger::error("Failed to send Add Advertising (temp) command");
//         return false;
//     }
//     return true;
// }

bool Mgmt::addAdvertising(const std::vector<uint8_t>& adData, const std::vector<uint8_t>& scanResponse)
{
    if (adData.size() > 31 || scanResponse.size() > 31) {
        Logger::error("AD or Scan Response data exceeds 31 bytes");
        return false;
    }

    // Fixed part of the MGMT_OP_ADD_ADVERTISING parameters
    struct SRequestHeader : HciAdapter::HciHeader
    {
        uint8_t instanceId;
        uint8_t flags[4];
        uint16_t duration;
        uint16_t timeout;
        uint8_t adLen;
        uint8_t scanRspLen;
    } __attribute__((packed));

    // Full structure with variable data (AD + Scan Response)
    struct SRequest : SRequestHeader
    {
        uint8_t data[62]; // maximum 31+31 bytes
    } __attribute__((packed));

    SRequest request;
    request.code = Mgmt::EAddAdvertisingCommand;
    request.controllerId = 0; // primary controller (hci0)

    // Parameter size = size of fixed part (without HciHeader) + actual data length
    request.dataSize = sizeof(SRequestHeader) - sizeof(HciAdapter::HciHeader) + adData.size() + scanResponse.size();

    // Fill fixed fields
    request.instanceId = 1; // fixed instance ID (can be made configurable)
    memset(request.flags, 0, 4); // no flags (0x01=connectable, 0x02=discoverable, etc.)
		request.flags[0] |= 0x01; // Set connectable flag
		request.flags[0] |= 0x02; // Set discoverable flag
    request.duration = 0;    // infinite
    request.timeout = 0;     // no timeout
    request.adLen = static_cast<uint8_t>(adData.size());
    request.scanRspLen = static_cast<uint8_t>(scanResponse.size());

    // Copy data sequentially: first AD data, then Scan Response data
    uint8_t* p = request.data;
    memcpy(p, adData.data(), adData.size());
    p += adData.size();
    memcpy(p, scanResponse.data(), scanResponse.size());

    if (!HciAdapter::getInstance().sendCommand(request)) {
        Logger::error("Failed to send Add Advertising command");
        return false;
    }
    return true;
}

bool Mgmt::removeAdvertising()
{
    struct SRequest : HciAdapter::HciHeader
    {
        uint8_t instanceId;
    } __attribute__((packed));

    SRequest request;
    request.code = Mgmt::ERemoveAdvertisingCommand;
    request.controllerId = 0;
    request.dataSize = sizeof(SRequest) - sizeof(HciAdapter::HciHeader);
    request.instanceId = advertiseInstanceId;

    if (!HciAdapter::getInstance().sendCommand(request)) {
        // It's okay if the instance didn't exist – we just log a debug message
        Logger::debug(SSTR << "Remove Advertising (instance " << (int)advertiseInstanceId << ") failed (probably not present)");
        return false;
    }
    return true;
}


bool Mgmt::setAdvertisingData(const std::vector<uint8_t>& adData, const std::vector<uint8_t>& scanResponse)
{
    // Remove any existing instance with this ID (ignore failure)
    removeAdvertising();

    // Add the new instance
    if (!addAdvertising(adData, scanResponse)) {
        Logger::error("Failed to add new advertising instance");
        return false;
    }

    // Enable advertising if it's not already enabled (optional, but we do it here)
    // Note: This is already handled by configureAdapter, but can be called here for safety.
    // To avoid duplication, we might skip this and rely on the global adFlag in configureAdapter.
    // But for direct updates, we should enable it.
    setAdvertising(1); // enable advertising for instance 0? Actually, setAdvertising toggles global advertising.
    // We need to set the advertising flag to 1 to start advertising.
    // However, setAdvertising(1) will enable advertising globally. Since we have an instance,
    // it will advertise that instance.

    return true;
}

// bool Mgmt::setAdvertisingData(const std::vector<uint8_t>& data)
// {
// 	Logger::info("JESUS Setting advertising data");
// 	// Maximum advertising data length per Bluetooth specification
// 	if (data.size() > 31) {
// 		Logger::error("Advertising data exceeds 31 bytes");
// 		return false;
// 	}

// 	// HCI command: LE Set Advertising Data (OGF 0x08, OCF 0x0008)
// 	// Opcode = (OGF << 10) | OCF = (0x08 << 10) | 0x0008 = 0x2008
// 	struct SRequest : HciAdapter::HciHeader
// 	{
// 		uint8_t advertisingData[31]; // Max 31 bytes of data
// 	} __attribute__((packed));

// 	SRequest request;
// 	request.code = ESetAdvertisingDataCommand;
// 	request.controllerId = 0; // Primary controller (instance 0)
// 	// dataSize: 1 byte for length + actual data size
// 	request.dataSize = static_cast<uint8_t>(1 + data.size());

// 	// First byte is the length of the data (as per HCI spec)
// 	request.advertisingData[0] = static_cast<uint8_t>(data.size());
// 	// Copy the actual data after the length byte
// 	if (!data.empty()) {
// 		memcpy(&request.advertisingData[1], data.data(), data.size());
// 	}

// 	if (!HciAdapter::getInstance().sendCommand(request)) {
// 		Logger::error("Failed to send LE Set Advertising Data command");
// 		return false;
// 	}

// 	return true;
// }

// Set the powered state to `newState` (true = powered on, false = powered off)
//
// Returns true on success, otherwise false
bool Mgmt::setPowered(bool newState)
{
	return setState(Mgmt::ESetPoweredCommand, controllerIndex, newState ? 1 : 0);
}

// Set the BR/EDR state to `newState` (true = enabled, false = disabled)
//
// Returns true on success, otherwise false
bool Mgmt::setBredr(bool newState)
{
	return setState(Mgmt::ESetBREDRCommand, controllerIndex, newState ? 1 : 0);
}

// Set the Secure Connection state (0 = disabled, 1 = enabled, 2 = secure connections only mode)
//
// Returns true on success, otherwise false
bool Mgmt::setSecureConnections(uint8_t newState)
{
	return setState(Mgmt::ESetSecureConnectionsCommand, controllerIndex, newState);
}

// Set the bondable state to `newState` (true = enabled, false = disabled)
//
// Returns true on success, otherwise false
bool Mgmt::setBondable(bool newState)
{
	return setState(Mgmt::ESetBondableCommand, controllerIndex, newState ? 1 : 0);
}

// Set the connectable state to `newState` (true = enabled, false = disabled)
//
// Returns true on success, otherwise false
bool Mgmt::setConnectable(bool newState)
{
	return setState(Mgmt::ESetConnectableCommand, controllerIndex, newState ? 1 : 0);
}

// Set the LE state to `newState` (true = enabled, false = disabled)
//
// Returns true on success, otherwise false
bool Mgmt::setLE(bool newState)
{
	return setState(Mgmt::ESetLowEnergyCommand, controllerIndex, newState ? 1 : 0);
}

// Set the advertising state to `newState` (0 = disabled, 1 = enabled (with consideration towards the connectable setting),
// 2 = enabled in connectable mode).
//
// Returns true on success, otherwise false
bool Mgmt::setAdvertising(uint8_t newState)
{
	return setState(Mgmt::ESetAdvertisingCommand, controllerIndex, newState);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Utilitarian
// ---------------------------------------------------------------------------------------------------------------------------------

// Truncates the string `name` to the maximum allowed length for an adapter name. If `name` needs no truncation, a copy of
// `name` is returned.
std::string Mgmt::truncateName(const std::string &name)
{
	if (name.length() <= kMaxAdvertisingNameLength)
	{
		return name;
	}

	return name.substr(0, kMaxAdvertisingNameLength);
}

// Truncates the string `name` to the maximum allowed length for an adapter short-name. If `name` needs no truncation, a copy
// of `name` is returned.
std::string Mgmt::truncateShortName(const std::string &name)
{
	if (name.length() <= kMaxAdvertisingShortNameLength)
	{
		return name;
	}

	return name.substr(0, kMaxAdvertisingShortNameLength);
}

}; // namespace ggk
