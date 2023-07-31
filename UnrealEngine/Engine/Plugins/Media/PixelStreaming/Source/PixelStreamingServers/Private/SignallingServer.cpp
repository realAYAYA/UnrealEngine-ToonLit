// Copyright Epic Games, Inc. All Rights Reserved.

#include "SignallingServer.h"
#include "ServerUtils.h"
#include "PixelStreamingServersLog.h"
#include "Misc/Paths.h"

namespace UE::PixelStreamingServers
{

	FString FSignallingServer::GetPathOnDisk()
	{
		return FString();
	}

	void FSignallingServer::Stop()
	{
		if (PlayersWS)
		{
			PlayersWS->Stop();
			PlayersWS.Reset();
		}
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

		// Add /images
		FWebSocketHttpMount ImagesMount;
		ImagesMount.SetPathOnDisk(ServersDir / TEXT("images"));
		ImagesMount.SetWebPath(FString(TEXT("/images")));
		MountsArr.Add(ImagesMount);

		// Add /scripts
		FWebSocketHttpMount ScriptsMount;
		ScriptsMount.SetPathOnDisk(ServersDir / TEXT("scripts"));
		ScriptsMount.SetWebPath(FString(TEXT("/scripts")));
		MountsArr.Add(ScriptsMount);

		return MountsArr;
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

	void FSignallingServer::OnStreamerConnected(uint16 ConnectionId)
	{
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Streamer websocket connected, id=%d"), ConnectionId);

		// Send a config message to the streamer passing ICE servers to be used.
		FString JSONString = CreateConfigJSON();
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Sending to streamer id=%d: %s"), ConnectionId, *JSONString);
		StreamersWS->Send(ConnectionId, JSONString);
	}

	FString FSignallingServer::CreateConfigJSON() const
	{
		// Todo (Luke): Parse `iceServers` from the process args `--peerConnectionOptions`
		TArray<TSharedPtr<FJsonValue>> IceServersArr;

		TSharedPtr<FJsonObject> PeerConnectionOptionsJSON = MakeShared<FJsonObject>();
		PeerConnectionOptionsJSON->SetArrayField(FString(TEXT("iceServers")), IceServersArr);

		TSharedRef<FJsonObject> ConfigJSON = MakeShared<FJsonObject>();
		ConfigJSON->SetStringField(FString(TEXT("type")), FString(TEXT("config")));
		ConfigJSON->SetObjectField(FString(TEXT("peerConnectionOptions")), PeerConnectionOptionsJSON);
		return Utils::ToString(ConfigJSON);
	}

	TSharedPtr<FJsonObject> FSignallingServer::ParseToJSON(FString Message) const
	{
		TSharedPtr<FJsonObject> JSONObj = MakeShared<FJsonObject>();
		bool bSuccess = Utils::Jsonify(Message, JSONObj);
		if (!bSuccess)
		{
			UE_LOG(LogPixelStreamingServers, Warning, TEXT("Could not deserialize message into a JSON object - message=%s"), *Message);
		}
		return JSONObj;
	}

	bool FSignallingServer::GetMessageType(TSharedPtr<FJsonObject> JSONObj, FString& OutMessageType) const
	{
		if (!JSONObj->TryGetStringField(FString(TEXT("type")), OutMessageType))
		{
			FString Message;
			bool bSuccess = Utils::Jsonify(Message, JSONObj);
			UE_LOG(LogPixelStreamingServers, Warning, TEXT("Message did not contain a field called 'type' - message=%s"), *Message);
			return false;
		}
		return true;
	}

	void FSignallingServer::SendPlayerMessage(uint16 PlayerConnectionId, FString MessageType, FString Message)
	{
		// All other message types just get routed to the player that streamer wishes to recieve it
		bool bForwardStraightToPlayer =
			MessageType == FString(TEXT("offer")) || MessageType == FString(TEXT("answer")) || MessageType == FString(TEXT("iceCandidate"));

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

	void FSignallingServer::OnStreamerMessage(uint16 ConnectionId, TArrayView<uint8> Message)
	{
		FString Msg = Utils::ToString(Message);
		UE_LOG(LogPixelStreamingServers, Log, TEXT("From Streamer id=%d: %s"), ConnectionId, *Msg);

		TSharedPtr<FJsonObject> JSONObj = ParseToJSON(Msg);
		FString MsgType;
		bool bHasMsgType = GetMessageType(JSONObj, MsgType);
		if (!bHasMsgType)
		{
			return;
		}

		// Got ping, respond with pong ---  { "type" : "pong", "time" : "1657509114" }
		if (MsgType == FString(TEXT("ping")))
		{
			const double UnixTime = FDateTime::UtcNow().ToUnixTimestamp();
			TSharedRef<FJsonObject> PongJSON = MakeShared<FJsonObject>();
			PongJSON->SetStringField(FString(TEXT("type")), FString(TEXT("pong")));
			PongJSON->SetNumberField(FString(TEXT("time")), UnixTime);
			UE_LOG(LogPixelStreamingServers, Log, TEXT("To Streamer id=%d: %s"), ConnectionId, *Utils::ToString(PongJSON));
			StreamersWS->Send(ConnectionId, Utils::ToString(PongJSON));
			return;
		}

		// All other message types require a `playerId` field to be valid.
		FString PlayerId;
		if (!JSONObj->TryGetStringField(TEXT("playerId"), PlayerId))
		{
			UE_LOG(LogPixelStreamingServers, Warning, TEXT("Message did not contain a field called 'playerId' - message=%s"), *Msg);
			return;
		}

		// Convert PlayerId into a uint16 so we can look it up
		uint16 PlayerIdInt = FCString::Atoi(*PlayerId);

		// As message are going to the player they don't actually need the playerId field, the field exists only so we know who to send it to.
		JSONObj->RemoveField(TEXT("playerId"));

		SendPlayerMessage(PlayerIdInt, MsgType, Utils::ToString(JSONObj.ToSharedRef()));
	}

	void FSignallingServer::OnPlayerMessage(uint16 ConnectionId, TArrayView<uint8> Message)
	{
		uint16 OutStreamerId;
		if (!StreamersWS->GetFirstConnection(OutStreamerId))
		{
			UE_LOG(LogPixelStreamingServers, Warning, TEXT("Player message ignored because no streamer is yet connected to the signalling server."));
			return;
		}

		FString Msg = Utils::ToString(Message);
		UE_LOG(LogPixelStreamingServers, Log, TEXT("From Player id=%d: %s"), ConnectionId, *Msg);

		TSharedPtr<FJsonObject> JSONObj = ParseToJSON(Msg);
		FString MsgType;
		bool bHasMsgType = GetMessageType(JSONObj, MsgType);
		if (!bHasMsgType)
		{
			return;
		}

		// Add player id to any messages going to streamer so streamer knows who sent it
		JSONObj->SetStringField(FString(TEXT("playerId")), FString::FromInt(ConnectionId));

		SendStreamerMessage(OutStreamerId, MsgType, Utils::ToString(JSONObj.ToSharedRef()));
	}

	void FSignallingServer::SendStreamerMessage(uint16 StreamerConnectionId, FString MessageType, FString Message)
	{
		// clang-format off
		bool bForwardStraightToStreamer =
			MessageType == FString(TEXT("answer")) || 
			MessageType == FString(TEXT("offer")) || 
			MessageType == FString(TEXT("iceCandidate")) || 
			MessageType == FString(TEXT("dataChannelRequest")) || 
			MessageType == FString(TEXT("peerDataChannelsReady"));
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
		else
		{
			// Unsupported message type
			UE_LOG(LogPixelStreamingServers, Warning, TEXT("Unsupported message type receieved from player, message=%s"), *Message);
			return;
		}
	}

	void FSignallingServer::OnPlayerConnected(uint16 ConnectionId)
	{
		// Todo (Luke): If we did want to support multiple streamers we could grab streamer id from url params perhaps.
		// For now we just grab the first streamer websocket connection.

		uint16 OutStreamerId;
		if (StreamersWS->GetFirstConnection(OutStreamerId))
		{
			// Send "playerConnected" message to streamer which kicks off making a new RTC connection
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Player connected over websocket. PlayerId=%d"), ConnectionId);
			TSharedRef<FJsonObject> OnPlayerConnectedJSON = MakeShared<FJsonObject>();
			OnPlayerConnectedJSON->SetStringField(FString(TEXT("type")), FString(TEXT("playerConnected")));
			OnPlayerConnectedJSON->SetStringField(FString(TEXT("playerId")), FString::FromInt(ConnectionId));
			OnPlayerConnectedJSON->SetBoolField(FString(TEXT("dataChannel")), true);
			OnPlayerConnectedJSON->SetBoolField(FString(TEXT("sfu")), false);
			FString JSONString = Utils::ToString(OnPlayerConnectedJSON);

			UE_LOG(LogPixelStreamingServers, Log, TEXT("Sending to streamer: %s"), *JSONString);
			StreamersWS->Send(OutStreamerId, JSONString);

			// Send config to newly connected player, which kicks off making a new RTC connection
			FString ConfigJSON = CreateConfigJSON();
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Sending to player %d: %s"), ConnectionId, *ConfigJSON);
			PlayersWS->Send(ConnectionId, ConfigJSON);
		}
		else
		{
			UE_LOG(LogPixelStreamingServers, Warning, TEXT("Player connection dropped because no streamer is yet connected to the signalling server."));
			return;
		}
	}

} // namespace UE::PixelStreamingServers
