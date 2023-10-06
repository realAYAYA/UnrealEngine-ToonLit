// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "PixelStreamingSignallingConnection.h"
#include "Serialization/JsonSerializer.h"
#include "IWebSocket.h"
#include "Utils.h"
#include "ToStringExtensions.h"
#include "Dom/JsonObject.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMockPixelStreamingSS, Log, VeryVerbose);
DEFINE_LOG_CATEGORY(LogMockPixelStreamingSS);

namespace
{
	// using namespace UE::PixelStreaming;

	// For validating callbacks from the signalling server connection
	class FTestSSConnectionObserver : public IPixelStreamingSignallingConnectionObserver
	{
	public:
		virtual ~FTestSSConnectionObserver() = default;

		virtual void OnSignallingConnected() override {}
		virtual void OnSignallingDisconnected(int32 StatusCode, const FString& Reason, bool bWasClean) override {}
		virtual void OnSignallingError(const FString& ErrorMsg) override {}

		virtual void OnSignallingConfig(const webrtc::PeerConnectionInterface::RTCConfiguration& Config) override { SetConfig = Config; }

		//// Streamer-only
		virtual void OnSignallingSessionDescription(FPixelStreamingPlayerId PlayerId, webrtc::SdpType Type, const FString& Sdp) override
		{
			SetSDP = { Type, Sdp, PlayerId };
		}
		virtual void OnSignallingRemoteIceCandidate(FPixelStreamingPlayerId PlayerId, const FString& SdpMid, int SdpMLineIndex, const FString& Sdp)
		{
			SetIceCandidate = { SdpMid, SdpMLineIndex, Sdp, PlayerId };
		}
		virtual void OnSignallingPlayerConnected(FPixelStreamingPlayerId, const FPixelStreamingPlayerConfig&, bool bSendOffer) override {}
		virtual void OnSignallingPlayerDisconnected(FPixelStreamingPlayerId PlayerId) override {}
		virtual void OnSignallingSFUPeerDataChannels(FPixelStreamingPlayerId SFUId, FPixelStreamingPlayerId PlayerId, int32 SendStreamId, int32 RecvStreamId) override {}

		//// Player-only
		virtual void OnSignallingSessionDescription(webrtc::SdpType Type, const FString& Sdp) override
		{
			SetSDP = { Type, Sdp };
		}
		virtual void OnSignallingRemoteIceCandidate(const FString& SdpMid, int SdpMLineIndex, const FString& Sdp)
		{
			SetIceCandidate = { SdpMid, SdpMLineIndex, Sdp };
		}
		virtual void OnSignallingPlayerCount(uint32 Count) override {}
		virtual void OnSignallingPeerDataChannels(int32 SendStreamId, int32 RecvStreamId) override {}
		virtual void OnSignallingStreamerList(const TArray<FString>& StreamerList) override {}

		void Reset()
		{
			SetConfig.Reset();
			SetSDP.Reset();
			SetIceCandidate.Reset();
		}

		TOptional<webrtc::PeerConnectionInterface::RTCConfiguration> SetConfig;

		struct FSessionDescriptionData
		{
			webrtc::SdpType Type;
			FString SDP;
			TOptional<FPixelStreamingPlayerId> PlayerId;
		};
		TOptional<FSessionDescriptionData> SetSDP;

		struct FIceCandidateData
		{
			FString SdpMid;
			int SdpMLineIndex;
			FString SDP;
			TOptional<FPixelStreamingPlayerId> PlayerId;
		};
		TOptional<FIceCandidateData> SetIceCandidate;
	};

	// For faking a web socket connection
	class FMockWebSocket : public IWebSocket
	{
	public:
		FMockWebSocket() = default;
		virtual ~FMockWebSocket() = default;
		virtual void Connect() override
		{
			bConnected = true;
			OnConnectedEvent.Broadcast();
		}
		virtual void Close(int32 Code = 1000, const FString& Reason = FString()) override { bConnected = false; }
		virtual bool IsConnected() override { return bConnected; }
		virtual void Send(const FString& Data) override { OnMockResponseEvent.Broadcast(Data); }
		virtual void Send(const void* Data, SIZE_T Size, bool bIsBinary = false) override {}
		virtual void SetTextMessageMemoryLimit(uint64 TextMessageMemoryLimit) override {}
		virtual FWebSocketConnectedEvent& OnConnected() override { return OnConnectedEvent; }
		virtual FWebSocketConnectionErrorEvent& OnConnectionError() override { return OnErrorEvent; }
		virtual FWebSocketClosedEvent& OnClosed() override { return OnClosedEvent; }
		virtual FWebSocketMessageEvent& OnMessage() override { return OnMessageEvent; }
		virtual FWebSocketBinaryMessageEvent& OnBinaryMessage() override { return OnBinaryMessageEvent; }
		virtual FWebSocketRawMessageEvent& OnRawMessage() override { return OnRawMessageEvent; }
		virtual FWebSocketMessageSentEvent& OnMessageSent() override { return OnMessageSentEvent; }

		void MockSend(const FString& Msg) const { OnMessageEvent.Broadcast(Msg); }

		// used to check outgoing messages
		DECLARE_EVENT_OneParam(FMockWebSocket, FMockResponseEvent, const FString& /* Message */);
		FMockResponseEvent OnMockResponseEvent;

	private:
		FWebSocketConnectedEvent OnConnectedEvent;
		FWebSocketConnectionErrorEvent OnErrorEvent;
		FWebSocketClosedEvent OnClosedEvent;
		FWebSocketMessageEvent OnMessageEvent;
		FWebSocketBinaryMessageEvent OnBinaryMessageEvent;
		FWebSocketRawMessageEvent OnRawMessageEvent;
		FWebSocketMessageSentEvent OnMessageSentEvent;

		bool bConnected = false;
	};

	class FMockSignallingConnection : public IPixelStreamingSignallingConnection
	{
	public:
		FMockSignallingConnection(IPixelStreamingSignallingConnectionObserver& InObserver, FString InStreamerId)
			: Observer(InObserver)
			, StreamerId(InStreamerId)
		{
			RegisterHandler("identify", [this](FJsonObjectPtr JsonMsg) {
				FJsonObjectPtr Json = MakeShared<FJsonObject>();
				Json->SetStringField(TEXT("type"), TEXT("endpointId"));
				Json->SetStringField(TEXT("id"), StreamerId);
				FString Msg = UE::PixelStreaming::ToString(Json, false);
				UE_LOG(LogMockPixelStreamingSS, Verbose, TEXT("-> SS: endpointId\n%s"), *Msg);
				SendMessage(Msg);
			});
			RegisterHandler("config", [this](FJsonObjectPtr JsonMsg) {
				const TSharedPtr<FJsonObject>* PeerConnectionOptions = nullptr;
				if (!JsonMsg->TryGetObjectField(TEXT("peerConnectionOptions"), PeerConnectionOptions))
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Cannot find `peerConnectionOptions` field in SS config\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}

				webrtc::PeerConnectionInterface::RTCConfiguration RTCConfig;

				const TArray<TSharedPtr<FJsonValue>>* IceServers = nullptr;
				if ((*PeerConnectionOptions)->TryGetArrayField(TEXT("iceServers"), IceServers))
				{
					for (const TSharedPtr<FJsonValue>& IceServerVal : *IceServers)
					{
						const TSharedPtr<FJsonObject>* IceServerJson;
						if (!IceServerVal->TryGetObject(IceServerJson))
						{
							UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Failed to parse SS config: `iceServer` - not an object\n%s"), *UE::PixelStreaming::ToString(*PeerConnectionOptions));
							return;
						}

						RTCConfig.servers.push_back(webrtc::PeerConnectionInterface::IceServer{});
						webrtc::PeerConnectionInterface::IceServer& IceServer = RTCConfig.servers.back();

						TArray<FString> Urls;
						if ((*IceServerJson)->TryGetStringArrayField(TEXT("urls"), Urls))
						{
							for (const FString& UrlElem : Urls)
							{
								IceServer.urls.push_back(UE::PixelStreaming::ToString(UrlElem));
							}
						}
						else
						{
							// in the RTC Spec, "urls" can be an array or a single string
							// https://www.w3.org/TR/webrtc/#dictionary-rtciceserver-members
							FString UrlsSingle;
							if ((*IceServerJson)->TryGetStringField(TEXT("urls"), UrlsSingle))
							{
								IceServer.urls.push_back(UE::PixelStreaming::ToString(UrlsSingle));
							}
						}

						FString Username;
						if ((*IceServerJson)->TryGetStringField(TEXT("username"), Username))
						{
							IceServer.username = UE::PixelStreaming::ToString(Username);
						}

						FString Credential;
						if ((*IceServerJson)->TryGetStringField(TEXT("credential"), Credential))
						{
							IceServer.password = UE::PixelStreaming::ToString(Credential);
						}
					}
				}

				// force `UnifiedPlan` as we control both ends of WebRTC streaming
				RTCConfig.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

				Observer.OnSignallingConfig(RTCConfig);
			});

			RegisterHandler("offer", [this](FJsonObjectPtr JsonMsg) {
				webrtc::SdpType Type = JsonMsg->GetStringField(TEXT("type")) == TEXT("offer") ? webrtc::SdpType::kOffer : webrtc::SdpType::kAnswer;

				FString Sdp;
				if (!JsonMsg->TryGetStringField(TEXT("sdp"), Sdp))
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Cannot find `sdp` in Streamer's answer\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}

				FPixelStreamingPlayerId PlayerId;
				bool bGotPlayerId = GetPlayerIdJson(JsonMsg, PlayerId);
				if (!bGotPlayerId)
				{
					Observer.OnSignallingSessionDescription(Type, Sdp);
				}
				else
				{
					Observer.OnSignallingSessionDescription(PlayerId, Type, Sdp);
				}
			});
			RegisterHandler("answer", [this](FJsonObjectPtr JsonMsg) {
				webrtc::SdpType Type = JsonMsg->GetStringField(TEXT("type")) == TEXT("offer") ? webrtc::SdpType::kOffer : webrtc::SdpType::kAnswer;

				FString Sdp;
				if (!JsonMsg->TryGetStringField(TEXT("sdp"), Sdp))
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Cannot find `sdp` in Streamer's answer\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}

				FPixelStreamingPlayerId PlayerId;
				bool bGotPlayerId = GetPlayerIdJson(JsonMsg, PlayerId);
				if (!bGotPlayerId)
				{
					Observer.OnSignallingSessionDescription(Type, Sdp);
				}
				else
				{
					Observer.OnSignallingSessionDescription(PlayerId, Type, Sdp);
				}
			});
			RegisterHandler("iceCandidate", [this](FJsonObjectPtr JsonMsg) {
				FPixelStreamingPlayerId PlayerId;
				bool bGotPlayerId = GetPlayerIdJson(JsonMsg, PlayerId);

				const FJsonObjectPtr* CandidateJson;
				if (!JsonMsg->TryGetObjectField(TEXT("candidate"), CandidateJson))
				{
					PlayerError(PlayerId, TEXT("Failed to get `candiate` from remote `iceCandidate` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
				}

				FString SdpMid;
				if (!(*CandidateJson)->TryGetStringField(TEXT("sdpMid"), SdpMid))
				{
					PlayerError(PlayerId, TEXT("Failed to get `sdpMid` from remote `iceCandidate` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
				}

				int32 SdpMLineIndex = 0;
				if (!(*CandidateJson)->TryGetNumberField(TEXT("sdpMlineIndex"), SdpMLineIndex))
				{
					PlayerError(PlayerId, TEXT("Failed to get `sdpMlineIndex` from remote `iceCandidate` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
				}

				FString CandidateStr;
				if (!(*CandidateJson)->TryGetStringField(TEXT("candidate"), CandidateStr))
				{
					PlayerError(PlayerId, TEXT("Failed to get `candidate` from remote `iceCandidate` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
				}

				if (bGotPlayerId)
				{
					Observer.OnSignallingRemoteIceCandidate(PlayerId, SdpMid, SdpMLineIndex, CandidateStr);
				}
				else
				{
					Observer.OnSignallingRemoteIceCandidate(SdpMid, SdpMLineIndex, CandidateStr);
				}
			});
			RegisterHandler("ping", [this](FJsonObjectPtr JsonMsg) { /* nothing */ });
			RegisterHandler("pong", [this](FJsonObjectPtr JsonMsg) { /* nothing */ });
			RegisterHandler("playerCount", [this](FJsonObjectPtr JsonMsg) {
				uint32 Count;
				if (!JsonMsg->TryGetNumberField(TEXT("count"), Count))
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Failed to get `count` from `playerCount` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}

				Observer.OnSignallingPlayerCount(Count);
			});
			RegisterHandler("playerConnected", [this](FJsonObjectPtr JsonMsg) {
				FPixelStreamingPlayerId PlayerId;
				bool bGotPlayerId = GetPlayerIdJson(JsonMsg, PlayerId);
				if (!bGotPlayerId)
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Failed to get `playerId` from `join` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}

				UE_LOG(LogMockPixelStreamingSS, Log, TEXT("Got player connected, player id=%s"), *PlayerId);

				FPixelStreamingPlayerConfig PlayerConfig;

				// Default to always making datachannel, unless explicitly set to false
				JsonMsg->TryGetBoolField(TEXT("dataChannel"), PlayerConfig.SupportsDataChannel);

				// Default peer is not an SFU, unless explictly set as SFU
				JsonMsg->TryGetBoolField(TEXT("sfu"), PlayerConfig.IsSFU);

				// Default to always sending an offer, unless explicitly set to false
				bool bSendOffer = true;
				JsonMsg->TryGetBoolField(TEXT("sendOffer"), bSendOffer);

				Observer.OnSignallingPlayerConnected(PlayerId, PlayerConfig, bSendOffer);
			});
			RegisterHandler("playerDisconnected", [this](FJsonObjectPtr JsonMsg) {
				FPixelStreamingPlayerId PlayerId;
				bool bGotPlayerId = GetPlayerIdJson(JsonMsg, PlayerId);
				if (!bGotPlayerId)
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Failed to get `playerId` from `playerDisconnected` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}

				Observer.OnSignallingPlayerDisconnected(PlayerId);
			});
			RegisterHandler("streamerDataChannels", [this](FJsonObjectPtr JsonMsg) {
				FPixelStreamingPlayerId SFUId;
				bool bSuccess = GetPlayerIdJson(JsonMsg, SFUId, TEXT("sfuId"));
				if (!bSuccess)
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Failed to get `sfuId` from `streamerDataChannels` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}

				FPixelStreamingPlayerId PlayerId;
				bSuccess = GetPlayerIdJson(JsonMsg, PlayerId, TEXT("playerId"));
				if (!bSuccess)
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Failed to get `playerId` from `streamerDataChannels` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}

				int32 SendStreamId;
				bSuccess = JsonMsg->TryGetNumberField(TEXT("sendStreamId"), SendStreamId);
				if (!bSuccess)
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Failed to get `sendStreamId` from `streamerDataChannels` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}

				int32 RecvStreamId;
				bSuccess = JsonMsg->TryGetNumberField(TEXT("recvStreamId"), RecvStreamId);
				if (!bSuccess)
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Failed to get `recvStreamId` from `streamerDataChannels` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}

				Observer.OnSignallingSFUPeerDataChannels(SFUId, PlayerId, SendStreamId, RecvStreamId);
			});
			RegisterHandler("peerDataChannels", [this](FJsonObjectPtr JsonMsg) {
				int32 SendStreamId = 0;
				if (!JsonMsg->TryGetNumberField(TEXT("sendStreamId"), SendStreamId))
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Failed to get `sendStreamId` from remote `peerDataChannels` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}
				int32 RecvStreamId = 0;
				if (!JsonMsg->TryGetNumberField(TEXT("recvStreamId"), RecvStreamId))
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Failed to get `recvStreamId` from remote `peerDataChannels` message\n%s"), *UE::PixelStreaming::ToString(JsonMsg));
					return;
				}
				Observer.OnSignallingPeerDataChannels(SendStreamId, RecvStreamId);
			});
		}
		virtual ~FMockSignallingConnection() = default;

		virtual void TryConnect(FString URL) override
		{
			if (WebSocket && WebSocket->IsConnected())
			{
				UE_LOG(LogMockPixelStreamingSS, Log, TEXT("Skipping `Connect()` because we are already connected. If you want to reconnect then disconnect first."));
				return;
			}

			// Reconnecting on an existing websocket can be problematic depending what state it was
			// left in. Easier and safer to disconnect any existing socket/delegates and start fresh.
			Disconnect();

			Url = URL;

			WebSocket = MakeShared<FMockWebSocket>();

			OnConnectedHandle = WebSocket->OnConnected().AddLambda([this]() {
				UE_LOG(LogMockPixelStreamingSS, Log, TEXT("Connected to SS %s"), *Url);
				Observer.OnSignallingConnected();
			});

			OnConnectionErrorHandle = WebSocket->OnConnectionError().AddLambda([this](const FString& Error) {
				UE_LOG(LogMockPixelStreamingSS, Log, TEXT("Failed to connect to SS %s - signalling server may not be up yet. Message: \"%s\""), *Url, *Error);
				Observer.OnSignallingError(Error);
			});

			OnClosedHandle = WebSocket->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean) {
				UE_LOG(LogMockPixelStreamingSS, Log, TEXT("Closed connection to SS %s - \n\tstatus %d\n\treason: %s\n\twas clean: %s"), *Url, StatusCode, *Reason, bWasClean ? TEXT("true") : TEXT("false"));
				Observer.OnSignallingDisconnected(StatusCode, Reason, bWasClean);
			});

			OnMessageHandle = WebSocket->OnMessage().AddLambda([this](const FString& Msg) {
				FJsonObjectPtr JsonMsg;
				const auto JsonReader = TJsonReaderFactory<TCHAR>::Create(Msg);

				if (!FJsonSerializer::Deserialize(JsonReader, JsonMsg))
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Failed to parse SS message:\n%s"), *Msg);
					return;
				}

				FString MsgType;
				if (!JsonMsg->TryGetStringField(TEXT("type"), MsgType))
				{
					UE_LOG(LogMockPixelStreamingSS, Error, TEXT("Cannot find `type` field in SS message:\n%s"), *Msg);
					return;
				}

				TFunction<void(FJsonObjectPtr)>* Handler = MessageHandlers.Find(MsgType);
				if (Handler != nullptr)
				{
					(*Handler)(JsonMsg);
				}
			});

			UE_LOG(LogMockPixelStreamingSS, Log, TEXT("Connecting to SS %s"), *Url);
			WebSocket->Connect();
		}

		virtual void Disconnect() override
		{
			if (!WebSocket)
			{
				return;
			}

			WebSocket->OnConnected().Remove(OnConnectedHandle);
			WebSocket->OnConnectionError().Remove(OnConnectionErrorHandle);
			WebSocket->OnClosed().Remove(OnClosedHandle);
			WebSocket->OnMessage().Remove(OnMessageHandle);

			WebSocket->Close();
			WebSocket = nullptr;
			UE_LOG(LogMockPixelStreamingSS, Log, TEXT("Closing websocket to SS %s"), *Url);
		}
		virtual bool IsConnected() const override
		{
			return WebSocket != nullptr && WebSocket.IsValid() && WebSocket->IsConnected();
		}

		virtual void SendOffer(FPixelStreamingPlayerId PlayerId, const webrtc::SessionDescriptionInterface& SDP) override
		{
			FJsonObjectPtr OfferJson = MakeShared<FJsonObject>();
			OfferJson->SetStringField(TEXT("type"), TEXT("offer"));
			SetPlayerIdJson(OfferJson, PlayerId);

			std::string SdpAnsi;
			SDP.ToString(&SdpAnsi);
			FString SdpStr = UE::PixelStreaming::ToString(SdpAnsi);
			OfferJson->SetStringField(TEXT("sdp"), SdpStr);

#if WITH_EDITOR
			// Default hover preference needs to be sent with the offer to prevent error messages on older infrastructure
			OfferJson->SetStringField(TEXT("defaultToHover"), TEXT("true"));
#endif // WITH_EDITOR

			UE_LOG(LogMockPixelStreamingSS, Log, TEXT("Sending player=%s \"offer\" to SS %s"), *PlayerId, *Url);
			UE_LOG(LogMockPixelStreamingSS, Verbose, TEXT("SDP offer\n%s"), *SdpStr);

			SendMessage(UE::PixelStreaming::ToString(OfferJson, false));
		}

		virtual void SendAnswer(FPixelStreamingPlayerId PlayerId, const webrtc::SessionDescriptionInterface& SDP) override
		{
			FJsonObjectPtr AnswerJson = MakeShared<FJsonObject>();
			AnswerJson->SetStringField(TEXT("type"), TEXT("answer"));
			SetPlayerIdJson(AnswerJson, PlayerId);

			std::string SdpAnsi;

			if (SDP.ToString(&SdpAnsi))
			{
				FString SdpStr = UE::PixelStreaming::ToString(SdpAnsi);
				AnswerJson->SetStringField(TEXT("sdp"), SdpStr);

				UE_LOG(LogMockPixelStreamingSS, Log, TEXT("Sending player=%s \"answer\" to SS %s"), *PlayerId, *Url);
				UE_LOG(LogMockPixelStreamingSS, Verbose, TEXT("SDP answer\n%s"), *SdpStr);

				SendMessage(UE::PixelStreaming::ToString(AnswerJson, false));
			}
		}

		virtual void SendIceCandidate(FPixelStreamingPlayerId PlayerId, const webrtc::IceCandidateInterface& IceCandidate) override
		{
			FJsonObjectPtr IceCandidateJson = MakeShared<FJsonObject>();

			IceCandidateJson->SetStringField(TEXT("type"), TEXT("iceCandidate"));
			SetPlayerIdJson(IceCandidateJson, PlayerId);

			FJsonObjectPtr CandidateJson = MakeShared<FJsonObject>();
			CandidateJson->SetStringField(TEXT("sdpMid"), IceCandidate.sdp_mid().c_str());
			CandidateJson->SetNumberField(TEXT("sdpMLineIndex"), static_cast<double>(IceCandidate.sdp_mline_index()));

			std::string CandidateAnsi;
			if (IceCandidate.ToString(&CandidateAnsi))
			{
				FString CandidateStr = UE::PixelStreaming::ToString(CandidateAnsi);
				CandidateJson->SetStringField(TEXT("candidate"), CandidateStr);
				IceCandidateJson->SetObjectField(TEXT("candidate"), CandidateJson);
				UE_LOG(LogMockPixelStreamingSS, Verbose, TEXT("-> SS: iceCandidate\n%s"), *UE::PixelStreaming::ToString(IceCandidateJson));
				SendMessage(UE::PixelStreaming::ToString(IceCandidateJson, false));
			}
		}

		virtual void SendDisconnectPlayer(FPixelStreamingPlayerId PlayerId, const FString& Reason) override
		{
			FJsonObjectPtr JsonMsg = MakeShared<FJsonObject>();

			JsonMsg->SetStringField(TEXT("type"), TEXT("disconnectPlayer"));
			SetPlayerIdJson(JsonMsg, PlayerId);
			JsonMsg->SetStringField(TEXT("reason"), Reason);

			FString Msg = UE::PixelStreaming::ToString(JsonMsg, false);
			UE_LOG(LogMockPixelStreamingSS, Verbose, TEXT("Sending player=%s \"disconnectPlayer\" to SS %s"), *PlayerId, *Url);

			SendMessage(Msg);
		}

		virtual void RequestStreamerList() override
		{
			FJsonObjectPtr Json = MakeShared<FJsonObject>();
			Json->SetStringField(TEXT("type"), TEXT("streamerList"));

			const FString JsonStr = UE::PixelStreaming::ToString(Json, false);
			UE_LOG(LogMockPixelStreamingSS, Verbose, TEXT("-> SS: streamerList\n%s"), *JsonStr);

			SendMessage(JsonStr);
		}

		virtual void SendSubscribe(const FString& ToStreamerId) override
		{
			FJsonObjectPtr SubscribeJson = MakeShared<FJsonObject>();
			SubscribeJson->SetStringField(TEXT("type"), TEXT("subscribe"));
			SubscribeJson->SetStringField(TEXT("streamerId"), ToStreamerId);

			const FString SubscribeJsonStr = UE::PixelStreaming::ToString(SubscribeJson, false);
			UE_LOG(LogMockPixelStreamingSS, Verbose, TEXT("-> SS: subscribe\n%s"), *SubscribeJsonStr);

			SendMessage(SubscribeJsonStr);
		}

		virtual void SendUnsubscribe() override
		{
			FJsonObjectPtr SubscribeJson = MakeShared<FJsonObject>();
			SubscribeJson->SetStringField(TEXT("type"), TEXT("unsubscribe"));

			const FString SubscribeJsonStr = UE::PixelStreaming::ToString(SubscribeJson, false);
			UE_LOG(LogMockPixelStreamingSS, Verbose, TEXT("-> SS: subscribe\n%s"), *SubscribeJsonStr);

			SendMessage(SubscribeJsonStr);
		}

		virtual void SendOffer(const webrtc::SessionDescriptionInterface& SDP) override
		{
			FJsonObjectPtr OfferJson = MakeShared<FJsonObject>();
			OfferJson->SetStringField(TEXT("type"), TEXT("offer"));

			FString SdpStr = UE::PixelStreaming::ToString(SDP);
			OfferJson->SetStringField(TEXT("sdp"), UE::PixelStreaming::ToString(SDP));

			UE_LOG(LogMockPixelStreamingSS, Verbose, TEXT("-> SS: offer\n%s"), *SdpStr);

			SendMessage(UE::PixelStreaming::ToString(OfferJson, false));
		}

		virtual void SendAnswer(const webrtc::SessionDescriptionInterface& SDP) override
		{
			FJsonObjectPtr AnswerJson = MakeShared<FJsonObject>();
			AnswerJson->SetStringField(TEXT("type"), TEXT("answer"));

			FString SdpStr = UE::PixelStreaming::ToString(SDP);
			AnswerJson->SetStringField(TEXT("sdp"), UE::PixelStreaming::ToString(SDP));

			UE_LOG(LogMockPixelStreamingSS, Verbose, TEXT("-> SS: answer\n%s"), *SdpStr);

			SendMessage(UE::PixelStreaming::ToString(AnswerJson, false));
		}

		virtual void SendIceCandidate(const webrtc::IceCandidateInterface& IceCandidate) override
		{
			FJsonObjectPtr IceCandidateJson = MakeShared<FJsonObject>();

			IceCandidateJson->SetStringField(TEXT("type"), TEXT("iceCandidate"));

			FJsonObjectPtr CandidateJson = MakeShared<FJsonObject>();
			CandidateJson->SetStringField(TEXT("sdpMid"), IceCandidate.sdp_mid().c_str());
			CandidateJson->SetNumberField(TEXT("sdpMLineIndex"), static_cast<double>(IceCandidate.sdp_mline_index()));

			std::string CandidateAnsi;
			verifyf(IceCandidate.ToString(&CandidateAnsi), TEXT("Failed to serialize IceCandidate"));
			FString CandidateStr = UE::PixelStreaming::ToString(CandidateAnsi);
			CandidateJson->SetStringField(TEXT("candidate"), CandidateStr);

			IceCandidateJson->SetObjectField(TEXT("candidate"), CandidateJson);

			UE_LOG(LogMockPixelStreamingSS, Verbose, TEXT("-> SS: ice-candidate\n%s"), *UE::PixelStreaming::ToString(IceCandidateJson));

			SendMessage(UE::PixelStreaming::ToString(IceCandidateJson, false));
		}

		virtual void SetKeepAlive(bool bKeepAlive) override
		{
			if (bKeepAliveEnabled == bKeepAlive)
			{
				return;
			}

			bKeepAliveEnabled = bKeepAlive;
		}

		virtual void SetAutoReconnect(bool bAutoReconnect) override
		{
			if (bAutoReconnectEnabled == bAutoReconnect)
			{
				return;
			}

			bAutoReconnectEnabled = bAutoReconnect;
		}

		virtual void SendMessage(const FString& Msg) override
		{
			if (WebSocket && WebSocket->IsConnected())
			{
				WebSocket->Send(Msg);
			}
		}

		TSharedPtr<FMockWebSocket> WebSocket;

	private:
		using FJsonObjectPtr = TSharedPtr<FJsonObject>;
		void RegisterHandler(const FString& messageType, const TFunction<void(FJsonObjectPtr)>& handler)
		{
			MessageHandlers.Add(messageType, handler);
		}

		bool GetPlayerIdJson(const FJsonObjectPtr& Json, FPixelStreamingPlayerId& OutPlayerId, const FString& FieldId = TEXT("playerId"))
		{
			uint32 PlayerIdInt;
			if (Json->TryGetNumberField(FieldId, PlayerIdInt))
			{
				OutPlayerId = ToPlayerId(PlayerIdInt);
				return true;
			}

			return false;
		}

		void SetPlayerIdJson(FJsonObjectPtr& JsonObject, FPixelStreamingPlayerId PlayerId)
		{
			int32 PlayerIdAsInt = PlayerIdToInt(PlayerId);
			JsonObject->SetNumberField(TEXT("playerId"), PlayerIdAsInt);
		}

		void PlayerError(FPixelStreamingPlayerId PlayerId, const FString& Msg)
		{
			UE_LOG(LogMockPixelStreamingSS, Error, TEXT("player %s: %s"), *PlayerId, *Msg);
			SendDisconnectPlayer(PlayerId, Msg);
		}

		template <typename FmtType, typename... T>
		void PlayerError(FPixelStreamingPlayerId PlayerId, const FmtType& Msg, T... args)
		{
			const FString FormattedMsg = FString::Printf(Msg, args...);
			PlayerError(PlayerId, FormattedMsg);
		}

		IPixelStreamingSignallingConnectionObserver& Observer;
		FString StreamerId;
		FString Url;

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

	class FMockSessionDescriptionInterface : public webrtc::SessionDescriptionInterface
	{
	public:
		virtual ~FMockSessionDescriptionInterface() = default;
		virtual cricket::SessionDescription* description() override { return nullptr; }
		virtual const cricket::SessionDescription* description() const override { return nullptr; }
		virtual std::string session_id() const override { return ""; }
		virtual std::string session_version() const override { return ""; }
		virtual std::string type() const override { return ""; }
		virtual bool AddCandidate(const webrtc::IceCandidateInterface* candidate) override { return true; }
		virtual size_t RemoveCandidates(const std::vector<cricket::Candidate>& candidates) override { return 0; }
		virtual size_t number_of_mediasections() const override { return 0; }
		virtual const webrtc::IceCandidateCollection* candidates(size_t mediasection_index) const override { return nullptr; }
		virtual bool ToString(std::string* out) const override { return true; }
	};

	class FMockIceCandidateInterface : public webrtc::IceCandidateInterface
	{
	public:
		virtual ~FMockIceCandidateInterface() = default;
		virtual std::string sdp_mid() const override { return ""; }
		virtual int sdp_mline_index() const override { return 0; }
		virtual const cricket::Candidate& candidate() const override { return MockCandidate; }
		virtual std::string server_url() const override { return ""; }
		virtual bool ToString(std::string* out) const override { return true; }

	private:
		cricket::Candidate MockCandidate;
	};

	BEGIN_DEFINE_SPEC(FSignallingServerConnectionSpec, "System.Plugins.PixelStreaming.SignallingServerConnection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FMockSignallingConnection> SignallingServerConnection; // the object being tested
	FTestSSConnectionObserver Observer;
	FDelegateHandle OnMessageDelegateHandle; // so we can remove the delegate after tests
	TSharedPtr<FJsonObject> LastJsonMsg;	 // the parsed JsonMsg of the last message received from the SignallingServerConnection
	const FString ServerID = "MOCK SERVER";
	const FPixelStreamingPlayerId MockPlayerId = "MOCK";
	const FMockSessionDescriptionInterface MockSDP;
	const FMockIceCandidateInterface MockCandidate;
	void OnMessageReceived(const FString& Response);
	END_DEFINE_SPEC(FSignallingServerConnectionSpec)
	void FSignallingServerConnectionSpec::Define()
	{
		Describe("Mocked connection tests", [this]() {
			BeforeEach([this]() {
				SignallingServerConnection = MakeUnique<FMockSignallingConnection>(Observer, ServerID);
				SignallingServerConnection->TryConnect("Fake Url");
				OnMessageDelegateHandle = SignallingServerConnection->WebSocket->OnMockResponseEvent.AddLambda([this](const FString& message) {
					OnMessageReceived(message);
				});
			});

			AfterEach([this]() {
				LastJsonMsg.Reset();
				SignallingServerConnection->WebSocket->OnMockResponseEvent.Remove(OnMessageDelegateHandle);
				SignallingServerConnection.Reset();
			});

			It("should be connected", [this]() {
				TestTrue("Connected", SignallingServerConnection->IsConnected());
			});

			It("should be able to send an offer", [this]() {
				// these calls should probably set some data on these mock objects
				// and then check that the received data is correct, like the mock
				// sends below
				SignallingServerConnection->SendOffer(MockPlayerId, MockSDP);
			});

			It("should be able to send an answer with no player it", [this]() {
				SignallingServerConnection->SendAnswer(MockSDP);
			});

			It("should be able to send an answer with a player id", [this]() {
				SignallingServerConnection->SendAnswer(MockPlayerId, MockSDP);
			});

			It("should be able to send an ice candidate with no player id", [this]() {
				SignallingServerConnection->SendIceCandidate(MockCandidate);
			});

			It("should be able to send an ice candidate with a player id", [this]() {
				SignallingServerConnection->SendIceCandidate(MockPlayerId, MockCandidate);
			});

			It("should be able to send a player disconnect message", [this]() {
				SignallingServerConnection->SendDisconnectPlayer(MockPlayerId, "No Reason");
			});

			It("should be able to send a custom message", [this]() {
				SignallingServerConnection->SendMessage(R"({ "type": "custom message"})");
			});

			It("should reply to an identity request", [this]() {
				SignallingServerConnection->WebSocket->MockSend(R"({"type" : "identify"})");
				TestEqual("Type", LastJsonMsg->GetStringField("type"), "endpointId");
				TestEqual("Id", LastJsonMsg->GetStringField("id"), ServerID);
			});

			It("should receive configurations", [this]() {
				SignallingServerConnection->WebSocket->MockSend(R"({ "type": "config", "peerConnectionOptions": {} })");
				TestTrue("Config IsSet", Observer.SetConfig.IsSet());
			});

			It("should be able to receive offers with no player id", [this]() {
				SignallingServerConnection->WebSocket->MockSend(R"({ "type": "offer", "sdp": "sdp goes here" })");
				if (TestTrue("SDP IsSet", Observer.SetSDP.IsSet()))
				{
					TestEqual("Offer type", Observer.SetSDP.GetValue().Type, webrtc::SdpType::kOffer);
					TestFalse("Player id IsSet", Observer.SetSDP.GetValue().PlayerId.IsSet());
					TestEqual("SDP value", Observer.SetSDP.GetValue().SDP, "sdp goes here");
				}
			});

			It("should be able to receive offers with a player id", [this]() {
				SignallingServerConnection->WebSocket->MockSend(R"({ "type": "offer", "playerId": "101", "sdp": "sdp goes here" })");
				if (TestTrue("SDP IsSet", Observer.SetSDP.IsSet()))
				{
					TestEqual("Offer type", Observer.SetSDP.GetValue().Type, webrtc::SdpType::kOffer);
					if (TestTrue("Player id IsSet", Observer.SetSDP.GetValue().PlayerId.IsSet()))
					{
						TestEqual("Player id", Observer.SetSDP.GetValue().PlayerId.GetValue(), "101");
					}
					TestEqual("SDP value", Observer.SetSDP.GetValue().SDP, "sdp goes here");
				}
			});

			It("should be able to receive an answer", [this]() {
				SignallingServerConnection->WebSocket->MockSend(R"({ "type": "answer", "playerId": "102", "sdp": "sdp goes here" })");
				if (TestTrue("SDP IsSet", Observer.SetSDP.IsSet()))
				{
					TestEqual("Answer type", Observer.SetSDP.GetValue().Type, webrtc::SdpType::kAnswer);
					TestEqual("SDP value", Observer.SetSDP.GetValue().SDP, "sdp goes here");
					if (TestTrue("Player id IsSet", Observer.SetSDP.GetValue().PlayerId.IsSet()))
					{
						TestEqual("Player id", Observer.SetSDP.GetValue().PlayerId.GetValue(), "102");
					}
				}
			});

			It("should be able to receive an ice candidate with no player id", [this]() {
				SignallingServerConnection->WebSocket->MockSend(R"({ "type": "iceCandidate", "candidate": { "sdpMid": "mid", "sdpMlineIndex": 10, "candidate": "sdp string" } })");
				if (TestTrue("Ice Candidate IsSet", Observer.SetIceCandidate.IsSet()))
				{
					TestEqual("sdp mid", Observer.SetIceCandidate.GetValue().SdpMid, "mid");
					TestEqual("sdp mline index", Observer.SetIceCandidate.GetValue().SdpMLineIndex, 10);
					TestEqual("sdp value", Observer.SetIceCandidate.GetValue().SDP, "sdp string");
					TestFalse("PlayerId IsSet", Observer.SetIceCandidate.GetValue().PlayerId.IsSet());
				}
			});

			It("should be able to receive an ice candidate with a player id", [this]() {
				SignallingServerConnection->WebSocket->MockSend(R"({ "type": "iceCandidate", "playerId": "103", "candidate": { "sdpMid": "mid", "sdpMlineIndex": 10, "candidate": "sdp string" } })");
				if (TestTrue("Ice Candidate IsSet", Observer.SetIceCandidate.IsSet()))
				{
					TestEqual("sdp mid", Observer.SetIceCandidate.GetValue().SdpMid, "mid");
					TestEqual("sdp mline index", Observer.SetIceCandidate.GetValue().SdpMLineIndex, 10);
					TestEqual("sdp value", Observer.SetIceCandidate.GetValue().SDP, "sdp string");
					if (TestTrue("Player id IsSet", Observer.SetIceCandidate.GetValue().PlayerId.IsSet()))
					{
						TestEqual("Player id", Observer.SetIceCandidate.GetValue().PlayerId.GetValue(), "103");
					}
				}
			});
		});
	}

	// used to store the parsed JsonMsg response from the signalling server connection
	void FSignallingServerConnectionSpec::OnMessageReceived(const FString& Response)
	{
		const auto JsonReader = TJsonReaderFactory<TCHAR>::Create(Response);
		TestTrue("Deserialised JsonMsg", FJsonSerializer::Deserialize(JsonReader, LastJsonMsg));
	}
} // namespace
