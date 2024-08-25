// Copyright Epic Games, Inc. All Rights Reserved.

#include "SignallingServer.h"
#include "ServerUtils.h"
#include "PixelStreamingServersLog.h"
#include "Misc/Paths.h"

namespace UE::PixelStreamingServers
{
	static const FString LEGACY_NAME = "_LEGACY_";

	FSignallingServer::FSignallingServer()
	{
		StreamerMessageHandlers.Add("endpointId").BindRaw(this, &FSignallingServer::OnStreamerIdMessage);
		StreamerMessageHandlers.Add("ping").BindRaw(this, &FSignallingServer::OnStreamerPingMessage);
		StreamerMessageHandlers.Add("disconnectPlayer").BindRaw(this, &FSignallingServer::OnStreamerDisconnectMessage);

		PlayerMessageHandlers.Add("listStreamers").BindRaw(this, &FSignallingServer::OnPlayerListStreamersMessage);
		PlayerMessageHandlers.Add("subscribe").BindRaw(this, &FSignallingServer::OnPlayerSubscribeMessage);
		PlayerMessageHandlers.Add("unsubscribe").BindRaw(this, &FSignallingServer::OnPlayerUnsubscribeMessage);
		PlayerMessageHandlers.Add("stats").BindRaw(this, &FSignallingServer::OnPlayerStatsMessage);
	}

	void FSignallingServer::Stop()
	{
		if (StreamersWS)
		{
			StreamersWS->Stop();
			StreamersWS.Reset();
		}
		if (Probe)
		{
			Probe.Reset();
		}
	}

	bool FSignallingServer::TestConnection()
	{
		if (bIsReady)
		{
			return true;
		}
		else
		{
			bool bConnected = Probe->Probe();
			if (bConnected)
			{
				// Close the websocket connection so others can use it
				Probe.Reset();
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	bool FSignallingServer::LaunchImpl(FLaunchArgs& InLaunchArgs, TMap<EEndpoint, FURL>& OutEndpoints)
	{
		Utils::PopulateCirrusEndPoints(InLaunchArgs, OutEndpoints);
		FURL PlayersURL = OutEndpoints[EEndpoint::Signalling_Players];
		FURL StreamerURL = OutEndpoints[EEndpoint::Signalling_Streamer];

		/*
		 * --------------- Streamers websocket server ---------------
		 */
		StreamersWS = MakeUnique<FWebSocketServerWrapper>();
		StreamersWS->OnMessage.AddRaw(this, &FSignallingServer::OnStreamerMessage);
		StreamersWS->OnOpenConnection.AddRaw(this, &FSignallingServer::OnStreamerConnected);
		StreamersWS->OnClosedConnection.AddRaw(this, &FSignallingServer::OnStreamerDisconnected);
		bool bLaunchedStreamerServer = StreamersWS->Launch(StreamerURL.Port);

		if (!bLaunchedStreamerServer)
		{
			UE_LOG(LogPixelStreamingServers, Error, TEXT("Failed to launch websocket server for streamers on port=%d"), StreamerURL.Port);
			return false;
		}

		/*
		 * --------------- Players websocket server ---------------
		 */
		PlayersWS = MakeUnique<FWebSocketServerWrapper>();
		PlayersWS->EnableWebServer(GenerateDirectoriesToServe());
		PlayersWS->OnMessage.AddRaw(this, &FSignallingServer::OnPlayerMessage);
		PlayersWS->OnOpenConnection.AddRaw(this, &FSignallingServer::OnPlayerConnected);
		PlayersWS->OnClosedConnection.AddRaw(this, &FSignallingServer::OnPlayerDisconnected);
		bool bLaunchedPlayerServer = PlayersWS->Launch(PlayersURL.Port);

		if (!bLaunchedPlayerServer)
		{
			UE_LOG(LogPixelStreamingServers, Error, TEXT("Failed to launch websocket server for players on port=%d"), PlayersURL.Port);
			return false;
		}

		/*
		 * --------------- Websocket probe ---------------
		 */

		if (bPollUntilReady)
		{
			TArray<FString> Protocols;
			Protocols.Add(FString(TEXT("binary")));
			Probe = MakeUnique<FWebSocketProbe>(StreamerURL, Protocols);
		}

		return true;
	}

	FString FSignallingServer::GetPathOnDisk()
	{
		return FString();
	}

	TArray<FWebSocketHttpMount> FSignallingServer::GenerateDirectoriesToServe() const
	{
		FString ServersDir;
		bool bServersDirExists = Utils::GetWebServersDir(ServersDir);
		if (bServersDirExists)
		{
			ServersDir = ServersDir / TEXT("SignallingWebServer");
			bServersDirExists = FPaths::DirectoryExists(ServersDir);
		}

		TArray<FWebSocketHttpMount> MountsArr;

#if WITH_EDITOR
		// If server directory doesn't exist we will serve a known directory that gives the user a message
		// telling them to run the `get_ps_servers` script.
		if (!bServersDirExists)
		{
			FString OutResourcesDir;
			bool bResourcesDirExists = Utils::GetResourcesDir(OutResourcesDir);
			FString NotFoundDir = OutResourcesDir / TEXT("NotFound");

			if (bResourcesDirExists && FPaths::DirectoryExists(NotFoundDir))
			{
				FWebSocketHttpMount Mount;
				Mount.SetPathOnDisk(NotFoundDir);
				Mount.SetWebPath(FString(TEXT("/")));
				Mount.SetDefaultFile(FString(TEXT("not_found.html")));
				MountsArr.Add(Mount);
				return MountsArr;
			}
		}
#endif // WITH_EDITOR

		// Add /Public
		FWebSocketHttpMount PublicMount;
		PublicMount.SetPathOnDisk(ServersDir / TEXT("Public"));
		PublicMount.SetWebPath(FString(TEXT("/")));
		PublicMount.SetDefaultFile(FString(TEXT("player.html")));
		MountsArr.Add(PublicMount);

		// Todo (Luke.Bermingham): Expose way for user to specify what directories to serve.

		return MountsArr;
	}

	TSharedRef<FJsonObject> FSignallingServer::CreateConfigJSON() const
	{
		// Todo (Luke): Parse `iceServers` from the process args `--peerConnectionOptions`
		TArray<TSharedPtr<FJsonValue>> IceServersArr;

		TSharedPtr<FJsonObject> PeerConnectionOptionsJSON = MakeShared<FJsonObject>();
		PeerConnectionOptionsJSON->SetArrayField(FString(TEXT("iceServers")), IceServersArr);

		TSharedRef<FJsonObject> ConfigJSON = MakeShared<FJsonObject>();
		ConfigJSON->SetStringField(FString(TEXT("type")), FString(TEXT("config")));
		ConfigJSON->SetObjectField(FString(TEXT("peerConnectionOptions")), PeerConnectionOptionsJSON);
		return ConfigJSON;
	}

	TSharedPtr<FJsonObject> FSignallingServer::ParseMessage(const FString& InMessage, FString& OutMessageType) const
	{
		TSharedPtr<FJsonObject> JSONObj = Utils::ToJSON(InMessage);
		if (!JSONObj)
		{
			UE_LOG(LogPixelStreamingServers, Error, TEXT("Failed to parse message: %s"), *InMessage);
			return nullptr;
		}

		if (!JSONObj->TryGetStringField(TEXT("type"), OutMessageType))
		{
			UE_LOG(LogPixelStreamingServers, Warning, TEXT("Incoming message did not contain a 'type' field: %s"), *InMessage);
			return nullptr;
		}

		return JSONObj;
	}

	void FSignallingServer::SubscribePlayer(uint16 PlayerConnectionId, const FString& StreamerName)
	{
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Subscribing player %d to streamer %s"), PlayerConnectionId, *StreamerName);

		uint16 StreamerConnectionId;
		if (!StreamersWS->GetNamedConnection(StreamerName, StreamerConnectionId))
		{
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Streamer name %s does not exist"), *StreamerName);
			return;
		}

		if (!StreamersWS->GetConnections().Contains(StreamerConnectionId))
		{
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Streamer %d does not exist"), StreamerConnectionId);
			return;
		}

		if (PlayerSubscriptions.Contains(PlayerConnectionId))
		{
			// unsubscribe first
			UnsubscribePlayer(PlayerConnectionId);
		}

		// We don't want to make the connections shared to prevent someone accidentally holding on to it. So we use it raw here
		FWebSocketConnection* PlayerWS = (*PlayersWS->GetConnections().Find(PlayerConnectionId)).Get();
		bool bUESendsOffer = !PlayerWS->GetUrlArgs().Contains(TEXT("OfferToReceive=true"));

		// Send "playerConnected" message to streamer which kicks off making a new RTC connection
		TSharedRef<FJsonObject> OnPlayerConnectedJSON = MakeShared<FJsonObject>();
		OnPlayerConnectedJSON->SetStringField("type", "playerConnected");
		OnPlayerConnectedJSON->SetStringField("playerId", FString::FromInt(PlayerConnectionId));
		OnPlayerConnectedJSON->SetBoolField("dataChannel", true);
		OnPlayerConnectedJSON->SetBoolField("sfu", false);
		OnPlayerConnectedJSON->SetBoolField("sendOffer", bUESendsOffer);
		SendStreamerMessage(StreamerConnectionId, OnPlayerConnectedJSON);

		PlayerSubscriptions.Add(PlayerConnectionId, StreamerConnectionId);
	}

	void FSignallingServer::UnsubscribePlayer(uint16 PlayerConnectionId)
	{
		if (PlayerSubscriptions.Contains(PlayerConnectionId))
		{
			const uint16 StreamerConnectionId = PlayerSubscriptions[PlayerConnectionId];
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Unsubscribing player %d from streamer %d"), PlayerConnectionId, StreamerConnectionId);

			// Send "playerDisconnected" message to streamer
			TSharedRef<FJsonObject> OnPlayerDisconnectedJSON = MakeShared<FJsonObject>();
			OnPlayerDisconnectedJSON->SetStringField("type", "playerDisconnected");
			OnPlayerDisconnectedJSON->SetStringField("playerId", FString::FromInt(PlayerConnectionId));
			SendStreamerMessage(StreamerConnectionId, OnPlayerDisconnectedJSON);

			PlayerSubscriptions.Remove(PlayerConnectionId);
		}
	}

	void FSignallingServer::SendPlayerMessage(uint16 PlayerId, TSharedPtr<FJsonObject> JSONObj)
	{
		const FString MessageString = Utils::ToString(JSONObj);
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Sending to player id=%d: %s"), PlayerId, *MessageString);
		PlayersWS->Send(PlayerId, MessageString);
	}

	void FSignallingServer::SendStreamerMessage(uint16 StreamerId, TSharedPtr<FJsonObject> JSONObj)
	{
		const FString MessageString = Utils::ToString(JSONObj);
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Sending to streamer id=%d: %s"), StreamerId, *MessageString);
		StreamersWS->Send(StreamerId, MessageString);
	}

	void FSignallingServer::OnStreamerConnected(uint16 ConnectionId)
	{
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Streamer websocket connected, id=%d"), ConnectionId);

		// Send a config message to the streamer passing ICE servers to be used.
		TSharedPtr<FJsonObject> JSONObj = CreateConfigJSON();
		SendStreamerMessage(ConnectionId, JSONObj);

		// request the streamer id
		TSharedRef<FJsonObject> idJSON = MakeShared<FJsonObject>();
		idJSON->SetStringField("type", "identify");
		SendStreamerMessage(ConnectionId, idJSON);

		StreamersWS->NameConnection(ConnectionId, LEGACY_NAME);
	}

	void FSignallingServer::OnStreamerDisconnected(uint16 ConnectionId)
	{
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Streamer websocket disconnected, id=%d"), ConnectionId);

		for (auto& ConnectionPair : PlayerSubscriptions)
		{
			const uint16 StreamerConnectionId = ConnectionPair.Key;
			const uint16 PlayerConnectionId = ConnectionPair.Value;
			if (StreamerConnectionId == ConnectionId)
			{
				UnsubscribePlayer(PlayerConnectionId);
			}
		}
	}

	void FSignallingServer::OnStreamerMessage(uint16 ConnectionId, TArrayView<uint8> Message)
	{
		const FString Msg = Utils::ToString(Message);
		UE_LOG(LogPixelStreamingServers, Log, TEXT("From Streamer id=%d: %s"), ConnectionId, *Msg);

		FString MsgType;
		TSharedPtr<FJsonObject> JSONObj = ParseMessage(Msg, MsgType);
		if (!JSONObj)
		{
			UE_LOG(LogPixelStreamingServers, Error, TEXT("Failed to parse incoming streamer message."));
			return;
		}

		if (auto* Handler = StreamerMessageHandlers.Find(MsgType))
		{
			Handler->Execute(ConnectionId, JSONObj);
		}
		else
		{
			// All other message types require a `playerId` field to be valid.
			uint16 PlayerConnectionId;
			if (!JSONObj->TryGetNumberField(TEXT("playerId"), PlayerConnectionId))
			{
				UE_LOG(LogPixelStreamingServers, Warning, TEXT("Message did not contain a field called 'playerId' - message=%s"), *Msg);
				return;
			}

			// As message are going to the player they don't actually need the playerId field, the field exists only so we know who to send it to.
			JSONObj->RemoveField(TEXT("playerId"));

			SendPlayerMessage(PlayerConnectionId, JSONObj);
		}
	}

	void FSignallingServer::OnPlayerConnected(uint16 ConnectionId)
	{
		// Send config to newly connected player, which kicks off making a new RTC connection
		TSharedPtr<FJsonObject> ConfigJSON = CreateConfigJSON();
		SendPlayerMessage(ConnectionId, ConfigJSON);
	}

	void FSignallingServer::OnPlayerDisconnected(uint16 ConnectionId)
	{
		UnsubscribePlayer(ConnectionId);
	}

	void FSignallingServer::OnPlayerMessage(uint16 ConnectionId, TArrayView<uint8> Message)
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
			if (!PlayerSubscriptions.Contains(ConnectionId))
			{
				TArray<FString> StreamerConnections = StreamersWS->GetConnectionNames();
				if(StreamerConnections.Num() == 0)
				{
					UE_LOG(LogPixelStreamingServers, Error, TEXT("Player %d sent a message, but no streamers were connected"), ConnectionId);
					return;
				}

				UE_LOG(LogPixelStreamingServers, Log, TEXT("Player %d attempted to send an outgoing message without having subscribed first. Defaulting to %s"), ConnectionId, *StreamerConnections[0]);
				SubscribePlayer(ConnectionId, StreamerConnections[0]);
			}

			// Add player id to any messages going to streamer so streamer knows who sent it
			JSONObj->SetStringField(TEXT("playerId"), FString::FromInt(ConnectionId));
			SendStreamerMessage(PlayerSubscriptions[ConnectionId], JSONObj);
		}
	}

	void FSignallingServer::OnStreamerIdMessage(uint16 ConnectionId, TSharedPtr<FJsonObject> JSONObj)
	{
		FString StreamerName;
		if (JSONObj->TryGetStringField(TEXT("id"), StreamerName))
		{
			StreamersWS->NameConnection(ConnectionId, StreamerName);
			StreamersWS->RemoveName(LEGACY_NAME);
		}
	}

	void FSignallingServer::OnStreamerPingMessage(uint16 ConnectionId, TSharedPtr<FJsonObject> JSONObj)
	{
		const double UnixTime = FDateTime::UtcNow().ToUnixTimestamp();
		TSharedRef<FJsonObject> PongJSON = MakeShared<FJsonObject>();
		PongJSON->SetStringField("type", "pong");
		PongJSON->SetNumberField("time", UnixTime);
		SendStreamerMessage(ConnectionId, PongJSON);
	}

	void FSignallingServer::OnStreamerDisconnectMessage(uint16 ConnectionId, TSharedPtr<FJsonObject> JSONObj)
	{
		uint16 PlayerConnectionId;
		if (!JSONObj->TryGetNumberField(TEXT("playerId"), PlayerConnectionId))
		{
			UE_LOG(LogPixelStreamingServers, Warning, TEXT("Disconnect message did not contain a field called 'playerId'"));
			return;
		}

		// TODO this might get called anyway from OnClosedConnection
		UnsubscribePlayer(PlayerConnectionId);
		PlayersWS->Close(PlayerConnectionId);
	}

	void FSignallingServer::OnPlayerListStreamersMessage(uint16 ConnectionId, TSharedPtr<FJsonObject> JSONObj)
	{
		TSharedRef<FJsonObject> listJSON = MakeShared<FJsonObject>();
		const TArray<FString> Names = StreamersWS->GetConnectionNames();
		TArray<TSharedPtr<FJsonValue>> JsonNames;
		for (const FString& Name : Names)
		{
			JsonNames.Add(MakeShared<FJsonValueString>(Name));
		}
		listJSON->SetStringField(TEXT("type"), TEXT("streamerList"));
		listJSON->SetArrayField(TEXT("ids"), JsonNames);
		SendPlayerMessage(ConnectionId, listJSON);
	}

	void FSignallingServer::OnPlayerSubscribeMessage(uint16 ConnectionId, TSharedPtr<FJsonObject> JSONObj)
	{
		FString StreamerName;
		if (!JSONObj->TryGetStringField(TEXT("streamerId"), StreamerName))
		{
			UE_LOG(LogPixelStreamingServers, Error, TEXT("Player %d subscribe message missing streamerId."), ConnectionId);
		}
		else
		{
			SubscribePlayer(ConnectionId, StreamerName);
		}
	}

	void FSignallingServer::OnPlayerUnsubscribeMessage(uint16 ConnectionId, TSharedPtr<FJsonObject> JSONObj)
	{
		UnsubscribePlayer(ConnectionId);
	}

	void FSignallingServer::OnPlayerStatsMessage(uint16 ConnectionId, TSharedPtr<FJsonObject> JSONObj)
	{
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Player %d stats = \n %s"), ConnectionId, *Utils::ToString(JSONObj.ToSharedRef()));
	}

	void FSignallingServer::GetNumStreamers(TFunction<void(uint16)> OnNumStreamersReceived)
	{
		if(StreamersWS)
		{
			OnNumStreamersReceived(StreamersWS->Count());
		}
		else
		{
			// Streamers websocket server went out of scope, so we can assume no streamers are connected.
			OnNumStreamersReceived(0);
		}
	}

} // namespace UE::PixelStreamingServers
