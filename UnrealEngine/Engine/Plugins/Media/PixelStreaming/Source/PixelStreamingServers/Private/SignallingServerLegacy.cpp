// Copyright Epic Games, Inc. All Rights Reserved.

#include "SignallingServerLegacy.h"

namespace UE::PixelStreamingServers
{

	void FSignallingServerLegacy::SendPlayerMessage(uint16 PlayerConnectionId, FString MessageType, FString Message)
	{
		// legacy supports: answer, disconnectPlayer, iceCandidate

		// All other message types just get routed to the player that streamer wishes to recieve it
		bool bForwardStraightToPlayer = MessageType == FString(TEXT("answer")) || MessageType == FString(TEXT("iceCandidate"));

		if (bForwardStraightToPlayer)
		{
			PlayersWS->Send(PlayerConnectionId, Message);
			return;
		}
		else if (MessageType == FString(TEXT("disconnectPlayer")))
		{
			PlayersWS->Close(PlayerConnectionId);
			return;
		}
		else
		{
			// Unsupported message type
			UE_LOG(LogPixelStreamingServers, Warning, TEXT("Unsupported message type receieved from streamer, message=%s"), *Message);
			return;
		}
	}

	void FSignallingServerLegacy::SendStreamerMessage(uint16 StreamerConnectionId, FString MessageType, FString Message)
	{
		// legacy supports: offer, stats, kick, iceCandidate

		// clang-format off
		bool bForwardStraightToStreamer = MessageType == FString(TEXT("offer")) || MessageType == FString(TEXT("iceCandidate"));
		// clang-format on

		if (bForwardStraightToStreamer)
		{
			StreamersWS->Send(StreamerConnectionId, Message);
			return;
		}
		else if (MessageType == FString(TEXT("stats")))
		{
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Player stats = \n %s"), *Message);
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
			UE_LOG(LogPixelStreamingServers, Warning, TEXT("Unsupported message type receieved from player, message=%s"), *Message);
			return;
		}
	}

	void FSignallingServerLegacy::OnPlayerConnected(uint16 PlayerConnectionId)
	{
		// Legacy version only sends "config" message to player when player connects
		FString ConfigJSON = CreateConfigJSON();
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Sending to player %d: %s"), PlayerConnectionId, *ConfigJSON);
		PlayersWS->Send(PlayerConnectionId, ConfigJSON);
	}

} // namespace UE::PixelStreamingServers