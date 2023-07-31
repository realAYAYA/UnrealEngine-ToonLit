// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DMXProtocolTypes.h"
#include "DMXProtocolSACNConstants.h"

#include "Serialization/ArrayReader.h"


class FDMXProtocolSACNUtils
{
public:
	/** Returns the SACN specific IP for a Universe ID */
	static uint32 GetIpForUniverseID(uint16 InUniverseID);

	// Can't instantiate this class
	FDMXProtocolSACNUtils() = delete;
};
