// Copyright Epic Games, Inc. All Rights Reserved.

#include "SignallingServerLegacy.h"

namespace UE::PixelStreamingServers
{

	void FSignallingServerLegacy::SendPlayerMessage(uint16 PlayerId, TSharedPtr<FJsonObject> JSONObj)
	{
		// legacy supports: answer, disconnectPlayer, iceCandidate, config

		const FString MessageString = Utils::ToString(JSONObj);
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Sending to player id=%d: %s"), PlayerId, *MessageString);

		FString MessageType;
		if (!JSONObj->TryGetStringField("type", MessageType))
		{
			UE_LOG(LogPixelStreamingServers, Error, TEXT("No message type on message sent to player %d"), PlayerId);
			return;
		}

		static const TSet<FString> ForwardMessages = {
			"offer",
			"answer",
			"iceCandidate",
			"config",
		};

		if (ForwardMessages.Contains(MessageType))
		{
			PlayersWS->Send(PlayerId, MessageString);
			return;
		}
		else if (MessageType == FString(TEXT("disconnectPlayer")))
		{
			PlayersWS->Close(PlayerId);
			return;
		}
		else
		{
			// Unsupported message type
			UE_LOG(LogPixelStreamingServers, Error, TEXT("Unsupported message type sent to player"));
			return;
		}
	}

	void FSignallingServerLegacy::SendStreamerMessage(uint16 StreamerId, TSharedPtr<FJsonObject> JSONObj)
	{
		// legacy supports: offer, stats, kick, iceCandidate

		const FString MessageString = Utils::ToString(JSONObj);
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Sending to streamer id=%d: %s"), StreamerId, *MessageString);

		FString MessageType;
		if (!JSONObj->TryGetStringField("type", MessageType))
		{
			UE_LOG(LogPixelStreamingServers, Error, TEXT("No message type on message sent to streamer %d"), StreamerId);
			return;
		}

		static const TSet<FString> ForwardMessages = {
			"offer",
			"answer",
			"iceCandidate",
			"config",
			"identify",
			"playerConnected",
			"playerDisconnected"
		};

		if (ForwardMessages.Contains(MessageType))
		{
			StreamersWS->Send(StreamerId, MessageString);
			return;
		}
		else if (MessageType == FString(TEXT("stats")))
		{
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Player stats = \n %s"), *MessageString);
			return;
		}
		else if (MessageType == FString(TEXT("kick")))
		{
			// do nothing, we removed kick from signalling
			return;
		}
		else
		{
			// Unsupported message type
			UE_LOG(LogPixelStreamingServers, Error, TEXT("Unsupported message type sent to streamer"));
			return;
		}
	}
} // namespace UE::PixelStreamingServers