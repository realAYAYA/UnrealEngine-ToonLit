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
		if (!JSONObj->TryGetStringField(TEXT("type"), MessageType))
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
		if (!JSONObj->TryGetStringField(TEXT("type"), MessageType))
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

	void FSignallingServerLegacy::OnStreamerConnected(uint16 ConnectionId)
	{
		if(StreamersWS->GetConnections().Num() > 1)
		{
			UE_LOG(LogPixelStreamingServers, Warning, TEXT("Streamer (id=%d) attempted to connect to the signalling server, but we already have a streamer connected"), ConnectionId);	
			return;
		}

		UE_LOG(LogPixelStreamingServers, Log, TEXT("Streamer websocket connected, id=%d"), ConnectionId);

		// Send a config message to the streamer passing ICE servers to be used.
		TSharedPtr<FJsonObject> JSONObj = CreateConfigJSON();
		SendStreamerMessage(ConnectionId, JSONObj);

		// request the streamer id
		TSharedRef<FJsonObject> idJSON = MakeShared<FJsonObject>();
		idJSON->SetStringField("type", "identify");
		SendStreamerMessage(ConnectionId, idJSON);

		StreamersWS->NameConnection(ConnectionId, TEXT("_LEGACY_"));
	}

	void FSignallingServerLegacy::OnPlayerMessage(uint16 ConnectionId, TArrayView<uint8> Message) 
	{
		const FString Msg = Utils::ToString(Message);
		UE_LOG(LogPixelStreamingServers, Log, TEXT("From Player id=%d: %s"), ConnectionId, *Msg);

		FString MsgType;
		TSharedPtr<FJsonObject> JSONObj = ParseMessage(Msg, MsgType);
		if (!JSONObj)
		{
			UE_LOG(LogPixelStreamingServers, Error, TEXT("Failed to parse incoming player message."));
			return;
		}

		if (auto* Handler = PlayerMessageHandlers.Find(MsgType))
		{
			Handler->Execute(ConnectionId, JSONObj);
		}
		else
		{
			TArray<FString> StreamerConnections = StreamersWS->GetConnectionNames();
			if(StreamerConnections.Num() == 0)
			{
				UE_LOG(LogPixelStreamingServers, Error, TEXT("Player %d sent a message, but no streamers were connected"), ConnectionId);
				return;
			}

			uint16 StreamerConnectionId = INDEX_NONE;
			if (StreamersWS->GetNamedConnection(StreamerConnections[0], StreamerConnectionId))
			{
				// Add player id to any messages going to streamer so streamer knows who sent it
				JSONObj->SetStringField(TEXT("playerId"), FString::FromInt(ConnectionId));
				SendStreamerMessage(StreamerConnectionId, JSONObj);
			}
		}
	}
} // namespace UE::PixelStreamingServers