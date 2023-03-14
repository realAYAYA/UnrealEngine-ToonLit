// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "PixelStreamingSignallingConnection.h"
#include "Serialization/JsonSerializer.h"
#include "IWebSocket.h"

namespace
{
	//using namespace UE::PixelStreaming;

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
		// virtual void OnPlayerConnected(FPixelStreamingPlayerId PlayerId, int Flags) { unimplemented(); }
		// virtual void OnPlayerDisconnected(FPixelStreamingPlayerId PlayerId) { unimplemented(); }
		// virtual void OnSFUPeerDataChannels(FPixelStreamingPlayerId SFUId, FPixelStreamingPlayerId PlayerId, int32 SendStreamId, int32 RecvStreamId) { unimplemented(); }

		//// Player-only
		virtual void OnSignallingSessionDescription(webrtc::SdpType Type, const FString& Sdp) override
		{
			SetSDP = { Type, Sdp };
		}
		virtual void OnSignallingRemoteIceCandidate(const FString& SdpMid, int SdpMLineIndex, const FString& Sdp)
		{
			SetIceCandidate = { SdpMid, SdpMLineIndex, Sdp };
		}
		// virtual void OnPlayerCount(uint32 Count) { unimplemented(); }
		// virtual void OnPeerDataChannels(int32 SendStreamId, int32 RecvStreamId) { unimplemented(); }

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
		virtual FWebSocketConnectedEvent& OnConnected() override { return OnConnectedEvent; }
		virtual FWebSocketConnectionErrorEvent& OnConnectionError() override { return OnErrorEvent; }
		virtual FWebSocketClosedEvent& OnClosed() override { return OnClosedEvent; }
		virtual FWebSocketMessageEvent& OnMessage() override { return OnMessageEvent; }
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
		FWebSocketRawMessageEvent OnRawMessageEvent;
		FWebSocketMessageSentEvent OnMessageSentEvent;

		bool bConnected = false;
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

	BEGIN_DEFINE_SPEC(FSignallingServerConnectionSpec, "System.Plugins.PixelStreaming.SignallingServerConnection",  EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FPixelStreamingSignallingConnection> SignallingServerConnection; // the object being tested
	FTestSSConnectionObserver Observer;
	FPixelStreamingSignallingConnection::FWebSocketFactory WebSocketFactory;
	TSharedPtr<FMockWebSocket> SocketEndpoint; // the socket created for the SignallingServerConnection. Used to fake send data.
	FDelegateHandle OnMessageDelegateHandle;   // so we can remove the delegate after tests
	TSharedPtr<FJsonObject> LastJsonMsg;	   // the parsed json of the last message received from the SignallingServerConnection
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
				WebSocketFactory = [this](const FString&) {
					SocketEndpoint = MakeShared<FMockWebSocket>();
					return SocketEndpoint;
				};
				SignallingServerConnection = MakeUnique<FPixelStreamingSignallingConnection>(WebSocketFactory, Observer, ServerID);
				SignallingServerConnection->TryConnect("Fake Url");
				OnMessageDelegateHandle = SocketEndpoint->OnMockResponseEvent.AddLambda([this](const FString& message) {
					OnMessageReceived(message);
				});
			});

			AfterEach([this]() {
				LastJsonMsg.Reset();
				SocketEndpoint->OnMockResponseEvent.Remove(OnMessageDelegateHandle);
				SocketEndpoint.Reset();
				SignallingServerConnection.Reset();
				WebSocketFactory.Reset();
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

			It("should reply to an identity request", [this]() {
				SocketEndpoint->MockSend(R"({"type" : "identify"})");
				TestEqual("Type", LastJsonMsg->GetStringField("type"), "endpointId");
				TestEqual("Id", LastJsonMsg->GetStringField("id"), ServerID);
			});

			It("should receive configurations", [this]() {
				SocketEndpoint->MockSend(R"({ "type": "config", "peerConnectionOptions": {} })");
				TestTrue("Config IsSet", Observer.SetConfig.IsSet());
			});

			It("should be able to receive offers with no player id", [this]() {
				SocketEndpoint->MockSend(R"({ "type": "offer", "sdp": "sdp goes here" })");
				if (TestTrue("SDP IsSet", Observer.SetSDP.IsSet()))
				{
					TestEqual("Offer type", Observer.SetSDP.GetValue().Type, webrtc::SdpType::kOffer);
					TestFalse("Player id IsSet", Observer.SetSDP.GetValue().PlayerId.IsSet());
					TestEqual("SDP value", Observer.SetSDP.GetValue().SDP, "sdp goes here");
				}
			});

			It("should be able to receive offers with a player id", [this]() {
				SocketEndpoint->MockSend(R"({ "type": "offer", "playerId": "101", "sdp": "sdp goes here" })");
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
				SocketEndpoint->MockSend(R"({ "type": "answer", "playerId": "102", "sdp": "sdp goes here" })");
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
				SocketEndpoint->MockSend(R"({ "type": "iceCandidate", "candidate": { "sdpMid": "mid", "sdpMlineIndex": 10, "candidate": "sdp string" } })");
				if (TestTrue("Ice Candidate IsSet", Observer.SetIceCandidate.IsSet()))
				{
					TestEqual("sdp mid", Observer.SetIceCandidate.GetValue().SdpMid, "mid");
					TestEqual("sdp mline index", Observer.SetIceCandidate.GetValue().SdpMLineIndex, 10);
					TestEqual("sdp value", Observer.SetIceCandidate.GetValue().SDP, "sdp string");
					TestFalse("PlayerId IsSet", Observer.SetIceCandidate.GetValue().PlayerId.IsSet());
				}
			});

			It("should be able to receive an ice candidate with a player id", [this]() {
				SocketEndpoint->MockSend(R"({ "type": "iceCandidate", "playerId": "103", "candidate": { "sdpMid": "mid", "sdpMlineIndex": 10, "candidate": "sdp string" } })");
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

	// used to store the parsed json response from the signalling server connection
	void FSignallingServerConnectionSpec::OnMessageReceived(const FString& Response)
	{
		const auto JsonReader = TJsonReaderFactory<TCHAR>::Create(Response);
		TestTrue("Deserialised json", FJsonSerializer::Deserialize(JsonReader, LastJsonMsg));
	}
} // namespace
