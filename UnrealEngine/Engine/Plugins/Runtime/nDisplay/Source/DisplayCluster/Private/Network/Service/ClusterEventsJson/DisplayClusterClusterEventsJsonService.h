// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Network/Service/DisplayClusterService.h"
#include "Network/Session/IDisplayClusterSessionPacketHandler.h"
#include "Network/Packet/DisplayClusterPacketJson.h"
#include "Network/Protocol/IDisplayClusterProtocolEventsJson.h"

#include "Dom/JsonObject.h"

/**
 * JSON cluster events server
 */
class FDisplayClusterClusterEventsJsonService
	: public    FDisplayClusterService
	, public    IDisplayClusterSessionPacketHandler<FDisplayClusterPacketJson, false>
	, protected IDisplayClusterProtocolEventsJson
{
public:
	FDisplayClusterClusterEventsJsonService();
	virtual ~FDisplayClusterClusterEventsJsonService();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterServer
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual FString GetProtocolName() const override;

public:
	enum class EDisplayClusterJsonError : uint8
	{
		Ok = 0,
		NotSupported = 1,
		MissedMandatoryFields = 2,
		UnknownError = 255
	};

protected:
	// Creates session instance for this service
	virtual TSharedPtr<IDisplayClusterSession> CreateSession(FDisplayClusterSessionInfo& SessionInfo) override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterSessionPacketHandler
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual typename IDisplayClusterSessionPacketHandler<FDisplayClusterPacketJson, false>::ReturnType ProcessPacket(const TSharedPtr<FDisplayClusterPacketJson>& Request, const FDisplayClusterSessionInfo& SessionInfo) override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolEventsJson
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event) override;

protected:
	// Callback when a session is closed
	void ProcessSessionClosed(const FDisplayClusterSessionInfo& SessionInfo);

private:
	FDisplayClusterClusterEventJson BuildClusterEventFromJson(const FString& EventName, const FString& EventType, const FString& EventCategory, const TSharedPtr<FJsonObject>& JsonPacket) const;
};
