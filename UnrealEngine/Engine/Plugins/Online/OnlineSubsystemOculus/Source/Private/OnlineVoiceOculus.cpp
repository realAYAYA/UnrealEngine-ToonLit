// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineVoiceOculus.h"
#include "Voice.h"
#include "OnlineSubsystemOculus.h"
#include "Components/AudioComponent.h"
#include "Misc/ConfigCacheIni.h"

#include "OnlineSubsystemUtils.h"
#include "Sound/SoundWaveProcedural.h"

#include "Misc/ConfigCacheIni.h"
#include "OnlineSubsystemOculusPackage.h"

// Define the voice sample rate the default in OVR_VoipSampleRate
#define OCULUS_VOICE_SAMPLE_RATE 48000
#define OCULUS_NUM_VOICE_CHANNELS 1

FRemoteTalkerDataOculus::FRemoteTalkerDataOculus() :
	LastSeen(0.0),
	AudioComponent(nullptr)
{
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FOnlineVoiceOculus::FOnlineVoiceOculus(FOnlineSubsystemOculus& InSubsystem) :
	bIsLocalPlayerMuted(false),
	OculusSubsystem(InSubsystem)
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FOnlineVoiceOculus::~FOnlineVoiceOculus()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (VoipConnectionRequestDelegateHandle.IsValid())
	{
		OculusSubsystem.RemoveNotifDelegate(ovrMessage_Notification_Voip_ConnectRequest, VoipConnectionRequestDelegateHandle);
		VoipConnectionRequestDelegateHandle.Reset();
	}

	if (VoipStateChangeDelegateHandle.IsValid())
	{
		OculusSubsystem.RemoveNotifDelegate(ovrMessage_Notification_Voip_StateChange, VoipStateChangeDelegateHandle);
		VoipStateChangeDelegateHandle.Reset();
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool FOnlineVoiceOculus::Init()
{
	bool bHasVoiceEnabled = false;
	if (GConfig->GetBool(TEXT("OnlineSubsystem"), TEXT("bHasVoiceEnabled"), bHasVoiceEnabled, GEngineIni) && bHasVoiceEnabled)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		VoipConnectionRequestDelegateHandle = OculusSubsystem.GetNotifDelegate(ovrMessage_Notification_Voip_ConnectRequest)
			.AddRaw(this, &FOnlineVoiceOculus::OnVoipConnectionRequest);

		VoipStateChangeDelegateHandle = OculusSubsystem.GetNotifDelegate(ovrMessage_Notification_Voip_StateChange)
			.AddRaw(this, &FOnlineVoiceOculus::OnVoipStateChange);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	else
	{
		UE_LOG_ONLINE_VOICE(Log, TEXT("Voice interface disabled by config [OnlineSubsystem].bHasVoiceEnabled"));
	}

	return bHasVoiceEnabled;
}

void FOnlineVoiceOculus::StartNetworkedVoice(uint8 LocalUserNum)
{
	if (LocalUserNum == 0)
	{
		ovr_Voip_SetMicrophoneMuted(ovrVoipMuteState_Unmuted);
		bIsLocalPlayerMuted = false;
		UE_LOG_ONLINE_VOICE(Log, TEXT("Starting networked voice for user: %d"), LocalUserNum);
	}
	else
	{
		UE_LOG_ONLINE_VOICE(Log, TEXT("Invalid user specified in StartNetworkedVoice(%d)"),
			static_cast<uint32>(LocalUserNum));
	}
}

void FOnlineVoiceOculus::StopNetworkedVoice(uint8 LocalUserNum)
{
	if (LocalUserNum == 0)
	{
		ovr_Voip_SetMicrophoneMuted(ovrVoipMuteState_Muted);
		bIsLocalPlayerMuted = true;
		UE_LOG_ONLINE_VOICE(Log, TEXT("Stopping networked voice for user: %d"), LocalUserNum);
	}
	else
	{
		UE_LOG_ONLINE_VOICE(Log, TEXT("Invalid user specified in StartNetworkedVoice(%d)"),
			static_cast<uint32>(LocalUserNum));
	}
}

bool FOnlineVoiceOculus::RegisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	auto OculusId = static_cast<const FUniqueNetIdOculus&>(UniqueId);
	ovr_Voip_Start(OculusId.GetID());
	return true;
}

bool FOnlineVoiceOculus::UnregisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	auto OculusId = static_cast<const FUniqueNetIdOculus&>(UniqueId);
	ovr_Voip_Stop(OculusId.GetID());
	return true;
}

void FOnlineVoiceOculus::RemoveAllRemoteTalkers()
{
	for (auto &RemoteTalker : RemoteTalkers)
	{
		UnregisterRemoteTalker(*RemoteTalker.TalkerId);
	}
}

bool FOnlineVoiceOculus::IsRemotePlayerTalking(const FUniqueNetId& UniqueId)
{
	auto RemoteTalker = FindRemoteTalker(UniqueId);
	return (RemoteTalker) ? RemoteTalker->bIsTalking : false;
}

bool FOnlineVoiceOculus::IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const
{
	auto RemoteTalkerId = static_cast<const FUniqueNetIdOculus&>(UniqueId);
	return MutedRemoteTalkers.Contains(RemoteTalkerId);
}
bool FOnlineVoiceOculus::MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	if (!bIsSystemWide)
	{
		UE_LOG_ONLINE_VOICE(Error, TEXT("Only System Wide muting of another player is currently supported"));
		return false;
	}

	auto RemoteTalkerId = static_cast<const FUniqueNetIdOculus&>(PlayerId);
	MutedRemoteTalkers.Add(RemoteTalkerId);

	// The actual muting will happen on ProcessRemoteVoicePackets

	return true;
}
bool FOnlineVoiceOculus::UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	if (!bIsSystemWide)
	{
		UE_LOG_ONLINE_VOICE(Error, TEXT("Only System Wide muting of another player is currently supported"));
		return false;
	}

	auto RemoteTalkerId = static_cast<const FUniqueNetIdOculus&>(PlayerId);
	MutedRemoteTalkers.Remove(RemoteTalkerId);
	return true;
}

void FOnlineVoiceOculus::Tick(float DeltaTime)
{
	ProcessRemoteVoicePackets();
}

void FOnlineVoiceOculus::ProcessRemoteVoicePackets()
{
	auto CurrentTime = FPlatformTime::Seconds();

	// Set the talking state for remote talkers
	for (FRemoteTalker& RemoteTalker : RemoteTalkers)
	{
		const FUniqueNetIdOculus& RemoteTalkerId = FUniqueNetIdOculus::Cast(*RemoteTalker.TalkerId);

		auto BufferSize = ovr_Voip_GetOutputBufferMaxSize();

		DecompressedVoiceBuffer.Empty(BufferSize);
		DecompressedVoiceBuffer.AddUninitialized(BufferSize);
		auto ElementsWritten = ovr_Voip_GetPCM(RemoteTalkerId.GetID(), DecompressedVoiceBuffer.GetData(), BufferSize);

		if (MutedRemoteTalkers.Contains(RemoteTalkerId))
		{
			auto RemoteData = RemoteTalkerBuffers.Find(RemoteTalkerId.AsShared());
			// Throw away the whole packet and dump the whole talker
			if (RemoteData && RemoteData->AudioComponent)
			{
				RemoteData->AudioComponent->Stop();
			}

			if (RemoteTalker.bIsTalking)
			{
				// Mark this remote talker as no longer talking
				RemoteTalker.bIsTalking = false;
				TriggerOnPlayerTalkingStateChangedDelegates(RemoteTalker.TalkerId.ToSharedRef(), false);
			}

			return;
		}

		if (ElementsWritten > 0)
		{
			// Mark this remote talker as talking
			if (!RemoteTalker.bIsTalking)
			{
				RemoteTalker.bIsTalking = true;
				TriggerOnPlayerTalkingStateChangedDelegates(RemoteTalker.TalkerId.ToSharedRef(), true);
			}

			FRemoteTalkerDataOculus& QueuedData = RemoteTalkerBuffers.FindOrAdd(RemoteTalkerId.AsShared());

			QueuedData.LastSeen = CurrentTime;

			if (!IsValid(QueuedData.AudioComponent))
			{
				QueuedData.AudioComponent = CreateVoiceAudioComponent(OCULUS_VOICE_SAMPLE_RATE, OCULUS_NUM_VOICE_CHANNELS);
				if (QueuedData.AudioComponent)
				{
					QueuedData.AudioComponent->AddToRoot(); // make sure this doesn't get deleted by the GC
					QueuedData.AudioComponent->OnAudioFinishedNative.AddRaw(this, &FOnlineVoiceOculus::OnAudioFinished);
				}
			}

			if (QueuedData.AudioComponent != nullptr)
			{
				if (!QueuedData.AudioComponent->IsActive())
				{
					QueuedData.AudioComponent->Play();
				}

				USoundWaveProcedural* SoundStreaming = CastChecked<USoundWaveProcedural>(QueuedData.AudioComponent->Sound);

				auto BytesWritten = ElementsWritten * sizeof(int16_t);
				SoundStreaming->QueueAudio(reinterpret_cast<uint8*>(DecompressedVoiceBuffer.GetData()), BytesWritten);
			}
		}
		else if (RemoteTalker.bIsTalking)
		{
			// Remove the user if the user is done talking
			auto RemoteData = RemoteTalkerBuffers.Find(RemoteTalkerId.AsShared());
			if (RemoteData && CurrentTime - RemoteData->LastSeen >= 1.0)
			{
				// Dump the whole talker
				if (RemoteData->AudioComponent)
				{
					RemoteData->AudioComponent->Stop();

					// Mark this remote talker as no longer talking
					RemoteTalker.bIsTalking = false;
					TriggerOnPlayerTalkingStateChangedDelegates(RemoteTalker.TalkerId.ToSharedRef(), false);
				}
			}
		}
	}
}

FRemoteTalker* FOnlineVoiceOculus::FindRemoteTalker(const FUniqueNetId& TalkerId)
{
	auto Index = IndexOfRemoteTalker(TalkerId);
	return (Index != INDEX_NONE) ? &RemoteTalkers[Index] : nullptr;
}

int32 FOnlineVoiceOculus::IndexOfRemoteTalker(const FUniqueNetId& TalkerId)
{
	return RemoteTalkers.IndexOfByPredicate([&TalkerId](const FRemoteTalker& RemoteTalker) 
	{
		return *RemoteTalker.TalkerId == TalkerId;
	});
}

FString FOnlineVoiceOculus::GetVoiceDebugState() const
{
	FString Output = TEXT("Voice state\n");

	Output += FString::Printf(TEXT("Ring Buffer Max Size: %d\n"), ovr_Voip_GetOutputBufferMaxSize());

	Output += (bIsLocalPlayerMuted) ? TEXT("Local Player Muted:\n") : TEXT("Local Player Unmuted:\n");

	Output += TEXT("\nRemote Talkers:\n");
	
	for (auto const &RemoteTalker : RemoteTalkers)
	{
		const FUniqueNetIdOculus& RemoteTalkerId = (RemoteTalker.TalkerId.IsValid()) ? FUniqueNetIdOculus::Cast(*RemoteTalker.TalkerId) : *FUniqueNetIdOculus::EmptyId();
		auto bIsRemoteTalkerMuted = (RemoteTalker.TalkerId.IsValid()) ? IsMuted(0, *RemoteTalker.TalkerId) : false;
		Output += FString::Printf(
			TEXT("UserId: %s:\nIsTalking: %d\nIsMuted: %d\nPCM Size: %d\n\n"), 
			*RemoteTalker.TalkerId->ToString(), 
			RemoteTalker.bIsTalking, 
			bIsRemoteTalkerMuted,
			ovr_Voip_GetPCMSize(RemoteTalkerId.GetID())
		);
	}
	
	return Output;
}

void FOnlineVoiceOculus::OnVoipConnectionRequest(ovrMessageHandle Message, bool bIsError) const
{
	auto NetworkingPeer = ovr_Message_GetNetworkingPeer(Message);
	auto PeerID = ovr_NetworkingPeer_GetID(NetworkingPeer);

	UE_LOG_ONLINE_VOICE(Verbose, TEXT("New incoming peer request: %llu"), PeerID);

	// Accept the connection
	ovr_Voip_Accept(PeerID);
}

void FOnlineVoiceOculus::OnVoipStateChange(ovrMessageHandle Message, bool bIsError)
{
	auto NetworkingPeer = ovr_Message_GetNetworkingPeer(Message);

	auto PeerID = ovr_NetworkingPeer_GetID(NetworkingPeer);

	UE_LOG_ONLINE_VOICE(Verbose, TEXT("%llu changed network connection state"), PeerID);

	auto State = ovr_NetworkingPeer_GetState(NetworkingPeer);
	if (State == ovrPeerState_Connected)
	{
		UE_LOG_ONLINE_VOICE(Verbose, TEXT("%llu is connected"), PeerID);
	}
	else if (State == ovrPeerState_Closed)
	{
		UE_LOG_ONLINE_VOICE(Verbose, TEXT("%llu is closed"), PeerID);
	}
	else if (State == ovrPeerState_Timeout)
	{
		UE_LOG_ONLINE_VOICE(Warning, TEXT("%llu timed out"), PeerID);
	}
	else
	{
		UE_LOG_ONLINE_VOICE(Warning, TEXT("%llu is in an unknown state"), PeerID);
	}

	const FUniqueNetIdOculusRef OculusPeerID = FUniqueNetIdOculus::Create(PeerID);

	auto Index = IndexOfRemoteTalker(*OculusPeerID);
	if (Index == INDEX_NONE)
	{
		if (State == ovrPeerState_Connected)
		{
			UE_LOG_ONLINE_VOICE(Verbose, TEXT("Adding %llu to remote talker list"), PeerID);
			int32 AddIndex = RemoteTalkers.AddZeroed();
			auto RemoteTalker = &RemoteTalkers[AddIndex];
			RemoteTalker->TalkerId = OculusPeerID;
		}
	}
	else
	{
		if (State == ovrPeerState_Closed || State == ovrPeerState_Timeout)
		{
			UE_LOG_ONLINE_VOICE(Verbose, TEXT("Removing %llu from remote talker list"), PeerID);
			RemoteTalkers.RemoveAtSwap(Index);
			FRemoteTalkerDataOculus RemoteData;
			if (RemoteTalkerBuffers.RemoveAndCopyValue(OculusPeerID, RemoteData))
			{
				if (RemoteData.AudioComponent)
				{
					RemoteData.AudioComponent->Stop();
				}
			}
		}
	}
}

void FOnlineVoiceOculus::OnAudioFinished(UAudioComponent* AC)
{
	for (FRemoteTalkerDataMap::TIterator It(RemoteTalkerBuffers); It; ++It)
	{
		FRemoteTalkerDataOculus& RemoteData = It.Value();
		if (!IsValid(RemoteData.AudioComponent) || AC == RemoteData.AudioComponent)
		{
			UE_LOG_ONLINE_VOICE(Log, TEXT("Removing VOIP AudioComponent for Id: %s"), *It.Key()->ToDebugString());
			RemoteData.AudioComponent->RemoveFromRoot(); // Let the GC clean this up
			RemoteData.AudioComponent = nullptr;
			break;
		}
	}
	UE_LOG_ONLINE_VOICE(Verbose, TEXT("Audio Finished"));
}
