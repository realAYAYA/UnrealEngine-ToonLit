// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FDisplayClusterClusterEventBinary;
struct FDisplayClusterClusterEventJson;
class FDisplayClusterPacketBinary;
class FDisplayClusterPacketJson;
class FDisplayClusterPacketInternal;


// Some helpers that simplify export/import of internal non-trivial data types
namespace DisplayClusterNetworkDataConversion
{
	// Internal packet helpers
	void JsonEventsFromInternalPacket(const TSharedPtr<FDisplayClusterPacketInternal>& Packet, TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& JsonEvents);
	void JsonEventsToInternalPacket(const TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& JsonEvents, TSharedPtr<FDisplayClusterPacketInternal>& Packet);
	void BinaryEventsFromInternalPacket(const TSharedPtr<FDisplayClusterPacketInternal>& Packet, TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& BinaryEvents);
	void BinaryEventsToInternalPacket(const TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& BinaryEvents, TSharedPtr<FDisplayClusterPacketInternal>& Packet);

	// Json events conversion
	bool JsonPacketToJsonEvent(const TSharedPtr<FDisplayClusterPacketJson>& Packet, FDisplayClusterClusterEventJson& OutBinaryEvent);
	TSharedPtr<FDisplayClusterPacketJson> JsonEventToJsonPacket(const FDisplayClusterClusterEventJson& BinaryEvent);

	// Binary events conversion
	bool BinaryPacketToBinaryEvent(const TSharedPtr<FDisplayClusterPacketBinary>& Packet, FDisplayClusterClusterEventBinary& OutBinaryEvent);
	TSharedPtr<FDisplayClusterPacketBinary> BinaryEventToBinaryPacket(const FDisplayClusterClusterEventBinary& BinaryEvent);
}
