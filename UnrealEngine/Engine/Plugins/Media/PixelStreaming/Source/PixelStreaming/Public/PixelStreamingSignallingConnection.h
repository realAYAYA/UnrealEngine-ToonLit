// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreamingWebRTCIncludes.h"
#include "Dom/JsonObject.h"
#include "Engine/EngineTypes.h"
#include "PixelStreamingPlayerId.h"
#include "IPixelStreamingSignallingConnectionObserver.h"

class IWebSocket;

/**
 * A specialized signalling server connection object for Pixel Streaming signalling servers.
 */
class PIXELSTREAMING_API FPixelStreamingSignallingConnection final
{
public:
	using FWebSocketFactory = TFunction<TSharedPtr<IWebSocket>(const FString&)>;

	FPixelStreamingSignallingConnection(const FWebSocketFactory& InWebSocketFactory, IPixelStreamingSignallingConnectionObserver& Observer, FString InStreamerId = "");
	~FPixelStreamingSignallingConnection();

	/**
	 * Connect to a specified signalling server at ths given URL
	 * @param URL The url of the destination signalling server.
	 */
	void TryConnect(FString URL);

	/**
	 * Disconnects from the signalling server. Safe to call even when not connected.
	 */
	void Disconnect();

	/**
	 * Checks if the connection is established.
	 * @returns True when the signalling server is connected.
	 */
	bool IsConnected() const;

	/**
	 * [Streamer only] Sends an offer to the specified player.
	 * @param PlayerId The Id of the destination player.
	 * @param SDP The offer session description for the player.
	 */
	void SendOffer(FPixelStreamingPlayerId PlayerId, const webrtc::SessionDescriptionInterface& SDP);

	/**
	 * [Streamer only] Sends an answer to the specified player.
	 * @param PlayerId The Id of the destination player.
	 * @param SDP The answer session description for the player.
	 */
	void SendAnswer(FPixelStreamingPlayerId PlayerId, const webrtc::SessionDescriptionInterface& SDP);

	/**
	 * [Streamer only] Sends ice candidate information to the specified player.
	 * @param PlayerId The Id of the destination player.
	 * @param IceCandidate The ice candidate information to send.
	 */
	void SendIceCandidate(FPixelStreamingPlayerId PlayerId, const webrtc::IceCandidateInterface& IceCandidate);

	/**
	 * [Streamer only] Send a disconnect message to the specified player.
	 * @param PlayerId The Id of the destination player.
	 * @param Reason An optional reason for the disconnection.
	 */
	void SendDisconnectPlayer(FPixelStreamingPlayerId PlayerId, const FString& Reason);

	/**
	 * [Player only] Sends an offer to the streamer.
	 * @param SDP The local description offer.
	 */
	void SendOffer(const webrtc::SessionDescriptionInterface& SDP);

	/**
	 * [Player only] Sends an answer back to the streamer after receiving an offer.
	 * @param SDP The local description answer.
	 */
	void SendAnswer(const webrtc::SessionDescriptionInterface& SDP);

	/**
	 * [Player only] Sends ICE candidate information to the streamer.
	 * @param IceCandidate The ICE candidate information.
	 */
	void SendIceCandidate(const webrtc::IceCandidateInterface& IceCandidate);

	/**
	 * Enables or disables the keep alive pings on this connection. Receiving connections
	 * should disable the keep alive since the reference signalling server will reject
	 * pings from players and close the connection.
	 * @param bKeepAlive True to enable keep alive pings. False to disable.
	 */
	void SetKeepAlive(bool bKeepAlive);

	/**
	 * Toggles automatic reconnecting when websocket is closed or unreachable.
	 * @param bAutoReconnect True to enable auto reconnect. False to disable.
	 */
	void SetAutoReconnect(bool bAutoReconnect);

private:
	void Connect(FString Url, bool bIsReconnect);
	void KeepAlive();
	void OnConnected();
	void OnConnectionError(const FString& Error);
	void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnMessage(const FString& Msg);

	using FJsonObjectPtr = TSharedPtr<FJsonObject>;
	void RegisterHandler(const FString& messageType, const TFunction<void(FJsonObjectPtr)>& handler);

	void OnIdRequested();
	void OnConfig(const FJsonObjectPtr& Json);
	void OnSessionDescription(const FJsonObjectPtr& Json);
	void OnIceCandidate(const FJsonObjectPtr& Json);
	void OnPlayerCount(const FJsonObjectPtr& Json);
	void OnPlayerConnected(const FJsonObjectPtr& Json);
	void OnPlayerDisconnected(const FJsonObjectPtr& Json);
	void OnSFUPeerDataChannels(const FJsonObjectPtr& Json);
	void OnPeerDataChannels(const FJsonObjectPtr& Json);
	void SetPlayerIdJson(FJsonObjectPtr& JsonObject, FPixelStreamingPlayerId PlayerId);
	bool GetPlayerIdJson(const FJsonObjectPtr& Json, FPixelStreamingPlayerId& OutPlayerId, const FString& FieldId = TEXT("playerId"));

	void StartKeepAliveTimer();
	void StopKeepAliveTimer();

	void StartReconnectTimer();
	void StopReconnectTimer();

	void SendMessage(const FString& Msg);

	template <typename FmtType, typename... T>
	void PlayerError(FPixelStreamingPlayerId PlayerId, const FmtType& Msg, T... args)
	{
		const FString FormattedMsg = FString::Printf(Msg, args...);
		PlayerError(PlayerId, FormattedMsg);
	}
	void PlayerError(FPixelStreamingPlayerId PlayerId, const FString& Msg);

private:
	FWebSocketFactory WebSocketFactory;
	IPixelStreamingSignallingConnectionObserver& Observer;
	FString StreamerId;
	FString Url;

	TSharedPtr<IWebSocket> WebSocket;

	FDelegateHandle OnConnectedHandle;
	FDelegateHandle OnConnectionErrorHandle;
	FDelegateHandle OnClosedHandle;
	FDelegateHandle OnMessageHandle;

	bool bAutoReconnectEnabled = true;
	bool bKeepAliveEnabled = true;
	/** Handle for efficient management of KeepAlive timer */
	FTimerHandle TimerHandle_KeepAlive;
	FTimerHandle TimerHandle_Reconnect;
	const float KEEP_ALIVE_INTERVAL = 60.0f;

	TMap<FString, TFunction<void(FJsonObjectPtr)>> MessageHandlers;
};
