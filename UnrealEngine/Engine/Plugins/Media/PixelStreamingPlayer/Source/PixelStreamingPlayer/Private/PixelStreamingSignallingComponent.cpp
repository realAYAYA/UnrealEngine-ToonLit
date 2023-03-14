// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingSignallingComponent.h"
#include "WebSocketsModule.h"
#include "PixelStreamingPeerComponent.h"
#include "PixelStreamingPlayerPrivate.h"

UPixelStreamingSignallingComponent::UPixelStreamingSignallingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FPixelStreamingSignallingConnection::FWebSocketFactory WebSocketFactory = [](const FString& Url) { return FWebSocketsModule::Get().CreateWebSocket(Url, TEXT("")); };
	SignallingConnection = MakeUnique<FPixelStreamingSignallingConnection>(WebSocketFactory, *this);
	// I dont disable keep alive here because it causes a deadlock.
	// This is because the game thread is waiting to load this component but the timer must be set
	// on the game thread.
}

void UPixelStreamingSignallingComponent::Connect(const FString& Url)
{
	SignallingConnection->SetKeepAlive(false);
	SignallingConnection->SetAutoReconnect(true);
	if (MediaSource == nullptr)
	{
		SignallingConnection->TryConnect(Url);
	}
	else
	{
		SignallingConnection->TryConnect(MediaSource->GetUrl());
	}
}

void UPixelStreamingSignallingComponent::Disconnect()
{
	SignallingConnection->Disconnect();
}

void UPixelStreamingSignallingComponent::SendOffer(const FPixelStreamingSessionDescriptionWrapper& Offer)
{
	if (Offer.SDP == nullptr)
	{
		UE_LOG(LogPixelStreamingPlayer, Error, TEXT("Send Offer failed: Offer was null"));
	}
	else
	{
		SignallingConnection->SendOffer(*Offer.SDP);
	}
}

void UPixelStreamingSignallingComponent::SendAnswer(const FPixelStreamingSessionDescriptionWrapper& Answer)
{
	if (Answer.SDP == nullptr)
	{
		UE_LOG(LogPixelStreamingPlayer, Error, TEXT("Send Answer failed: Answer was null"));
	}
	else
	{
		SignallingConnection->SendAnswer(*Answer.SDP);
	}
}

void UPixelStreamingSignallingComponent::SendIceCandidate(const FPixelStreamingIceCandidateWrapper& CandidateWrapper)
{
	SignallingConnection->SendIceCandidate(*CandidateWrapper.ToWebRTC());
}

void UPixelStreamingSignallingComponent::OnSignallingConnected()
{
	OnConnected.Broadcast();
}

void UPixelStreamingSignallingComponent::OnSignallingDisconnected(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	OnDisconnected.Broadcast(StatusCode, Reason, bWasClean);
}

void UPixelStreamingSignallingComponent::OnSignallingError(const FString& ErrorMsg)
{
	OnConnectionError.Broadcast(ErrorMsg);
}

void UPixelStreamingSignallingComponent::OnSignallingConfig(const webrtc::PeerConnectionInterface::RTCConfiguration& Config)
{
	FPixelStreamingRTCConfigWrapper Wrapper;
	Wrapper.Config = Config;
	OnConfig.Broadcast(Wrapper);
}

void UPixelStreamingSignallingComponent::OnSignallingSessionDescription(webrtc::SdpType Type, const FString& Sdp)
{
	if (Type == webrtc::SdpType::kOffer)
	{
		OnOffer.Broadcast(Sdp);
	}
	else if (Type == webrtc::SdpType::kAnswer)
	{
		OnAnswer.Broadcast(Sdp);
	}
}

void UPixelStreamingSignallingComponent::OnSignallingRemoteIceCandidate(const FString& SdpMid, int SdpMLineIndex, const FString& Sdp)
{
	FPixelStreamingIceCandidateWrapper CandidateWrapper(SdpMid, SdpMLineIndex, Sdp);
	OnIceCandidate.Broadcast(CandidateWrapper);
}

void UPixelStreamingSignallingComponent::OnSignallingPeerDataChannels(int32 SendStreamId, int32 RecvStreamId)
{
	// TODO what to do with data channels.
}

void UPixelStreamingSignallingComponent::OnSignallingPlayerCount(uint32 Count)
{
}
