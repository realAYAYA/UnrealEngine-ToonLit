// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXProtocolTypes.h"
#include "Library/DMXEntity.h"

#include "DMXEntityController.generated.h"

struct FDMXBuffer;



// DEPRECATED 4.27, can't be flagged as such to retain upgrade path, some nodes could not be compiled anymore. All members are deprecated.
UCLASS()
class DMXRUNTIME_API UDMXEntityUniverseManaged
	: public UDMXEntity
{
	GENERATED_BODY()
public:

	UE_DEPRECATED(4.27, "UDMXEntityUniverseManaged is deprecated. Use Ports instead.")
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Controllers are no longer in use. Use Ports instead."))
	FDMXProtocolName DeviceProtocol;
};


// DEPRECATED 4.27, can't be flagged as such to retain upgrade path, some nodes could not be compiled anymore. All members are deprecated.
UCLASS()
class DMXRUNTIME_API UDMXEntityController
	: public UDMXEntityUniverseManaged
{
	GENERATED_BODY()

public:
	UE_DEPRECATED(4.27, "UDMXEntityController is deprecated. Use Ports instead.")
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Controllers are no longer in use. Use Ports instead."))
	EDMXCommunicationType CommunicationMode;
	
	UE_DEPRECATED(4.27, "UDMXEntityController is deprecated. Use Ports instead.")
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Controllers are no longer in use. Use Ports instead."))
	int32 UniverseLocalStart;

	UE_DEPRECATED(4.27, "UDMXEntityController is deprecated. Use Ports instead.")
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Controllers are no longer in use. Use Ports instead."))
	int32 UniverseLocalNum;

	UE_DEPRECATED(4.27, "UDMXEntityController is deprecated. Use Ports instead.")
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Controllers are no longer in use. Use Ports instead."))
	int32 UniverseLocalEnd;

	/**
	 * Offsets the Universe IDs range on this Controller before communication with other devices.
	 * Useful to solve conflicts with Universe IDs from other devices on the same network.
	 *
	 * All other DMX Library settings use the normal Universe IDs range.
	 * This allows the user to change all Universe IDs used by the Fixture Patches and
	 * avoid conflicts with other devices by updating only the Controller's Remote Offset.
	 */
	UE_DEPRECATED(4.27, "UDMXEntityController is deprecated. Use Ports instead.")
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Controllers are no longer in use. Use Ports instead."))
	int32 RemoteOffset;

	/**
	 * First Universe ID on this Controller's range that is sent over the network.
	 * Universe Start + Remote Offset
	 */
	UE_DEPRECATED(4.27, "UDMXEntityController is deprecated. Use Ports instead.")
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Controllers are no longer in use. Use Ports instead."))
	int32 UniverseRemoteStart;

	/**
	 * Last Universe ID in this Controller's range that is sent over the network.
	 * Universe End + Remote Offset
	 */
	UE_DEPRECATED(4.27, "UDMXEntityController is deprecated. Use Ports instead.")
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Controllers are no longer in use. Use Ports instead."))
	int32 UniverseRemoteEnd;

	UE_DEPRECATED(4.27, "UDMXEntityController is deprecated. Use Ports instead.")
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Controllers are no longer in use. Use Ports instead."))
	TArray<FString> AdditionalUnicastIPs;
};
