// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Network/Packet/IDisplayClusterPacket.h"
#include "Dom/JsonObject.h"


/**
 * JSON based packet type for display cluster networking
 */
class FDisplayClusterPacketJson
	: public IDisplayClusterPacket
{
public:
	FDisplayClusterPacketJson()
		: JsonData(new FJsonObject)
	{ }

public:
	inline const TSharedPtr<FJsonObject> GetJsonData() const
	{
		return JsonData;
	}

	inline void SetJsonData(TSharedPtr<FJsonObject> InJsonData)
	{
		JsonData = InJsonData;
	}

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterPacket
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool SendPacket(FDisplayClusterSocketOperations& SocketOps) override;
	virtual bool RecvPacket(FDisplayClusterSocketOperations& SocketOps) override;
	virtual FString ToLogString(bool bDetailed = false) const override;

private:
	struct FPacketHeader
	{
		uint32 PacketBodyLength;

		FString ToString()
		{
			return FString::Printf(TEXT("<length=%u>"), PacketBodyLength);
		}
	};

private:
	TSharedPtr<FJsonObject> JsonData;
};
