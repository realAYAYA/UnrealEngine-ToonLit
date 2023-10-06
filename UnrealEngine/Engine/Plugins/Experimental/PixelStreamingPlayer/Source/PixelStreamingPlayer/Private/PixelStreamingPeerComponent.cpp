// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingPeerComponent.h"
#include "PixelStreamingPlayerPrivate.h"
#include "PixelStreamingPeerConnection.h"
#include "FrameBufferRHI.h"
#include "FrameBufferI420.h"
#include "ITextureMediaPlayer.h"
#include "ITextureMediaPlayerModule.h"
#include "RTCStatsCollector.h"
#include "Async/Async.h"

UPixelStreamingPeerComponent::UPixelStreamingPeerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPixelStreamingPeerComponent::SetConfig(const FPixelStreamingRTCConfigWrapper& Config)
{
	PeerConnection = FPixelStreamingPeerConnection::Create(Config.Config);
#if WEBRTC_5414
	PeerConnection->SetWebRTCStatsCallback(rtc::scoped_refptr<UE::PixelStreaming::FRTCStatsCollector>(new UE::PixelStreaming::FRTCStatsCollector(TEXT("Streamer"))));
#else
	PeerConnection->SetWebRTCStatsCallback(new rtc::RefCountedObject<UE::PixelStreaming::FRTCStatsCollector>(TEXT("Streamer")));
#endif

	if (PeerConnection)
	{
		if (VideoSinkPlayer != nullptr)
		{
			ITextureMediaPlayerModule* TextureModule = FModuleManager::LoadModulePtr<ITextureMediaPlayerModule>("TextureMediaPlayer");
			if (TextureModule != nullptr)
			{
				TextureMediaPlayer = TextureModule->OpenPlayer(VideoSinkPlayer);
				if (TextureMediaPlayer != nullptr)
				{
					PeerConnection->SetVideoSink(this);
				}
			}
		}
		else
		{
			UE_LOG(LogPixelStreamingPlayer, Error, TEXT("MediaPlayer not found."));
		}

		PeerConnection->OnEmitIceCandidate.AddLambda([this](const webrtc::IceCandidateInterface* Candidate) {
			FPixelStreamingIceCandidateWrapper CandidateWrapper(*Candidate);
			AsyncTask(ENamedThreads::GameThread, [this, CandidateWrapper]() {
				OnIceCandidate.Broadcast(CandidateWrapper);
			});
		});
		PeerConnection->OnIceStateChanged.AddLambda([this](webrtc::PeerConnectionInterface::IceConnectionState NewState) {
			AsyncTask(ENamedThreads::GameThread, [this, NewState]() {
				OnIceConnectionChange(NewState);
			});
		});
	}
	else
	{
		UE_LOG(LogPixelStreamingPlayer, Error, TEXT("SetConfig Failed."));
	}
}

FPixelStreamingSessionDescriptionWrapper UPixelStreamingPeerComponent::CreateOffer()
{
	if (PeerConnection)
	{
		FPixelStreamingSessionDescriptionWrapper Wrapper;
		FEvent* TaskEvent = FPlatformProcess::GetSynchEventFromPool();
		const auto OnGeneralFailure = [&TaskEvent](const FString& ErrorMsg) {
			UE_LOG(LogPixelStreamingPlayer, Error, TEXT("CreateOffer Failed: %s"), *ErrorMsg);
			TaskEvent->Trigger();
		};
		AsyncTask(ENamedThreads::AnyNormalThreadNormalTask, [this, &TaskEvent, &Wrapper, &OnGeneralFailure]() {
			PeerConnection->CreateOffer(
				FPixelStreamingPeerConnection::EReceiveMediaOption::All,
				[&TaskEvent, &Wrapper](const webrtc::SessionDescriptionInterface* Sdp) {
					// success
					Wrapper.SDP = Sdp;
					TaskEvent->Trigger();
				},
				OnGeneralFailure);
		});
		TaskEvent->Wait();
		FPlatformProcess::ReturnSynchEventToPool(TaskEvent);

		if (Wrapper.SDP == nullptr)
		{
			UE_LOG(LogPixelStreamingPlayer, Error, TEXT("CreateOffer Failed: Timeout"));
		}

		return Wrapper;
	}
	else
	{
		UE_LOG(LogPixelStreamingPlayer, Error, TEXT("Failed to create offer. Call SetConfig first."));
		return {};
	}
}

FPixelStreamingSessionDescriptionWrapper UPixelStreamingPeerComponent::CreateAnswer(const FString& Sdp)
{
	if (PeerConnection)
	{
		FPixelStreamingSessionDescriptionWrapper Wrapper;
		FEvent* TaskEvent = FPlatformProcess::GetSynchEventFromPool();
		const auto OnGeneralFailure = [&TaskEvent](const FString& ErrorMsg) {
			UE_LOG(LogPixelStreamingPlayer, Error, TEXT("CreateAnswer Failed: %s"), *ErrorMsg);
			TaskEvent->Trigger();
		};
		AsyncTask(ENamedThreads::AnyNormalThreadNormalTask, [this, &TaskEvent, &Sdp, &Wrapper, &OnGeneralFailure]() {
			PeerConnection->ReceiveOffer(
				Sdp,
				[this, &TaskEvent, &Wrapper, &OnGeneralFailure]() {
					// success
					PeerConnection->CreateAnswer(
						FPixelStreamingPeerConnection::EReceiveMediaOption::All,
						[&TaskEvent, &Wrapper](const webrtc::SessionDescriptionInterface* Sdp) {
							// success
							Wrapper.SDP = Sdp;
							TaskEvent->Trigger();
						},
						OnGeneralFailure);
				},
				OnGeneralFailure);
		});
		TaskEvent->Wait();
		FPlatformProcess::ReturnSynchEventToPool(TaskEvent);

		if (Wrapper.SDP == nullptr)
		{
			UE_LOG(LogPixelStreamingPlayer, Error, TEXT("CreateAnswer Failed: Timeout"));
		}

		return Wrapper;
	}
	else
	{
		UE_LOG(LogPixelStreamingPlayer, Error, TEXT("Failed to create answer. Call SetConfig first."));
		return {};
	}
}

void UPixelStreamingPeerComponent::ReceiveAnswer(const FString& Offer)
{
	if (PeerConnection)
	{
		FEvent* TaskEvent = FPlatformProcess::GetSynchEventFromPool();
		const auto OnGeneralFailure = [&TaskEvent](const FString& ErrorMsg) {
			UE_LOG(LogPixelStreamingPlayer, Error, TEXT("ReceiveAnswer Failed: %s"), *ErrorMsg);
			TaskEvent->Trigger();
		};
		AsyncTask(ENamedThreads::AnyNormalThreadNormalTask, [this, &TaskEvent, &Offer, &OnGeneralFailure]() {
			PeerConnection->ReceiveAnswer(
				Offer,
				[this, &TaskEvent, &OnGeneralFailure]() {
					// success
					TaskEvent->Trigger();
				},
				OnGeneralFailure);
		});
		TaskEvent->Wait();
		FPlatformProcess::ReturnSynchEventToPool(TaskEvent);
	}
	else
	{
		UE_LOG(LogPixelStreamingPlayer, Error, TEXT("Failed to receive answer. Call SetConfig first."));
	}
}

void UPixelStreamingPeerComponent::ReceiveIceCandidate(const FPixelStreamingIceCandidateWrapper& Candidate)
{
	if (PeerConnection)
	{
		PeerConnection->AddRemoteIceCandidate(Candidate.Mid, Candidate.MLineIndex, Candidate.SDP);
	}
}

void UPixelStreamingPeerComponent::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState NewState)
{
	if (NewState == webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionConnected)
	{
		OnIceConnection.Broadcast(0);
	}
	else if (NewState == webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected)
	{
		OnIceDisconnection.Broadcast();
	}
}

void UPixelStreamingPeerComponent::OnFrame(const webrtc::VideoFrame& frame)
{
	if (frame.video_frame_buffer()->type() == webrtc::VideoFrameBuffer::Type::kNative)
	{
		UE::PixelStreaming::FFrameBufferRHI* const FrameBuffer = static_cast<UE::PixelStreaming::FFrameBufferRHI*>(frame.video_frame_buffer().get());

		if (FrameBuffer != nullptr)
		{
			TSharedPtr<FVideoResourceRHI, ESPMode::ThreadSafe> VideoResource = static_cast<UE::PixelStreaming::FFrameBufferRHI*>(frame.video_frame_buffer().get())->GetVideoResource();

			if (TextureMediaPlayer.IsValid() && VideoResource.IsValid())
			{
				ResourceQueue.Enqueue(VideoResource);

				// Get off WebRTC thread ASAP
				AsyncTask(ENamedThreads::AnyHiPriThreadHiPriTask, [this, TMPWeak = TextureMediaPlayer.ToWeakPtr()]() {
					auto PinnedTextureMediaPlayer = TMPWeak.Pin();

					if (PinnedTextureMediaPlayer.IsValid() && !ResourceQueue.IsEmpty())
					{
						TSharedPtr<FVideoResource, ESPMode::ThreadSafe> VideoResource;

						while (!ResourceQueue.IsEmpty())
						{
							ResourceQueue.Dequeue(VideoResource);
						}

						auto& Raw = StaticCastSharedPtr<FVideoResourceRHI>(VideoResource)->GetRaw();
						PinnedTextureMediaPlayer->OnFrame(Raw.Texture, Raw.Fence, Raw.FenceValue);
					}
				});
			}
		}
	}
	if (frame.video_frame_buffer()->type() == webrtc::VideoFrameBuffer::Type::kI420)
	{
		// TODO (aidan.possemiers)
		unimplemented();
	}
}
