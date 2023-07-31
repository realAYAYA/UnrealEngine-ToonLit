// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ServerBase.h"
#include "WebSocketProbe.h"
#include "WebSocketServerWrapper.h"
#include "Templates/UniquePtr.h"
#include "Templates/SharedPointer.h"
#include "Dom/JsonObject.h"

namespace UE::PixelStreamingServers
{

	/**
	 * A native C++ implementation of a Pixel Streaming signalling server (similar to cirrus.js).
	 * This implementation is missing some features such as SFU, matchmaker, and Webserver support.
	 * The purpose of this implementation is to have signalling server callable from UE, without bundling Cirrus itself.
	 **/
	class FSignallingServer : public FServerBase
	{

	public:
		virtual ~FSignallingServer() = default;

		// Begin FServerBase
		virtual void Stop() override;
		virtual bool TestConnection() override;
		virtual bool LaunchImpl(FLaunchArgs& InLaunchArgs, TMap<EEndpoint, FURL>& OutEndpoints) override;
		virtual FString GetPathOnDisk() override;
		// End FServerBase

	protected:
		void OnStreamerConnected(uint16 ConnectionId);
		void OnStreamerMessage(uint16 ConnectionId, TArrayView<uint8> Message);
		virtual void SendPlayerMessage(uint16 PlayerConnectionId, FString MessageType, FString Message);

		void OnPlayerMessage(uint16 ConnectionId, TArrayView<uint8> Message);
		virtual void SendStreamerMessage(uint16 StreamerConnectionId, FString MessageType, FString Message);

		virtual void OnPlayerConnected(uint16 ConnectionId);

		TArray<FWebSocketHttpMount> GenerateDirectoriesToServe() const;
		FString CreateConfigJSON() const;
		TSharedPtr<FJsonObject> ParseToJSON(FString Message) const;
		bool GetMessageType(TSharedPtr<FJsonObject> JSONObj, FString& OutMessageType) const;

	protected:
		TUniquePtr<FWebSocketProbe> Probe;
		TUniquePtr<FWebSocketServerWrapper> StreamersWS;
		TUniquePtr<FWebSocketServerWrapper> PlayersWS;
	};

} // namespace UE::PixelStreamingServers