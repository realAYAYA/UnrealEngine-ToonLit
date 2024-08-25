// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"

class FString;

/**
 * Network related helpers
 *
 * Main purpose of this class is to return correct information about network settings from UStormSyncTransportSettings
 * and Socket Subsystem.
 */
class STORMSYNCTRANSPORTCORE_API FStormSyncTransportNetworkUtils
{
public:
	/** Returns either the configured Server Name in settings, or falls back to platform computer name */
	static FString GetServerName();
	
	/** Parses and return configured server endpoint in settings, or an empty string if setting is invalid  */
	static FString GetTcpEndpointAddress();
	
	/**
	 * Returns the list of local adapter addresses as returned by the socket subsystem, with port from server
	 * endpoint settings appended to the address list.
	 */
	static TArray<FString> GetLocalAdapterAddresses();
};
