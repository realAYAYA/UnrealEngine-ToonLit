// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerSourceVoice.h"
#include "AudioMixerSource.h"
#include "AudioMixerSourceManager.h"
#include "AudioMixerDevice.h"

namespace Audio
{

	/**
	* FMixerSourceVoice Implementation
	*/

	FMixerSourceVoice::FMixerSourceVoice()
	{
		Reset(nullptr);
	}

	FMixerSourceVoice::~FMixerSourceVoice()
	{
	}

	void FMixerSourceVoice::Reset(FMixerDevice* InMixerDevice)
	{
		if (InMixerDevice)
		{
			MixerDevice = InMixerDevice;
			SourceManager = MixerDevice->GetSourceManager();
		}
		else
		{
			MixerDevice = nullptr;
			SourceManager = nullptr;
		}

		Pitch = -1.0f;
		Volume = -1.0f;
		DistanceAttenuation = -1.0f;
		Distance = -1.0f;
		LPFFrequency = -1.0f;
		HPFFrequency = -1.0f;
		SourceId = INDEX_NONE;
		bIsPlaying = false;
		bIsPaused = false;
		bIsActive = false;
		bIsBus = false;
		bEnableBusSends = false;
		bEnableBaseSubmix = false;
		bEnableSubmixSends = false;
		bStopFadedOut = false;

		PitchModBase = TNumericLimits<float>::Max();
		VolumeModBase = TNumericLimits<float>::Max();
		LPFFrequencyModBase = TNumericLimits<float>::Max();
		HPFFrequencyModBase = TNumericLimits<float>::Max();

		SubmixSends.Reset();
	}

	bool FMixerSourceVoice::Init(const FMixerSourceVoiceInitParams& InitParams)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (SourceManager->GetFreeSourceId(SourceId))
		{
			AUDIO_MIXER_CHECK(InitParams.SourceListener != nullptr);
			AUDIO_MIXER_CHECK(InitParams.NumInputChannels > 0);

			bEnableBusSends = InitParams.bEnableBusSends;
			bEnableBaseSubmix = InitParams.bEnableBaseSubmix;
			bEnableSubmixSends = InitParams.bEnableSubmixSends;

			bIsBus = InitParams.AudioBusId != INDEX_NONE;

			for (int32 i = 0; i < InitParams.SubmixSends.Num(); ++i)
			{
				FMixerSubmixPtr SubmixPtr = InitParams.SubmixSends[i].Submix.Pin();
				if (SubmixPtr.IsValid())
				{
					SubmixSends.Add(SubmixPtr->GetId(), InitParams.SubmixSends[i]);
				}
			}

			bStopFadedOut = false;
			SourceManager->InitSource(SourceId, InitParams);
			return true;
		}

		return false;
	}

	void FMixerSourceVoice::Release()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		SourceManager->ReleaseSourceId(SourceId);
	}

	void FMixerSourceVoice::SetPitch(const float InPitch)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (Pitch != InPitch)
		{
			Pitch = InPitch;
			SourceManager->SetPitch(SourceId, InPitch);
		}
	}

	void FMixerSourceVoice::SetVolume(const float InVolume)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (Volume != InVolume)
		{
			Volume = InVolume;
			SourceManager->SetVolume(SourceId, InVolume);
		}
	}

	void FMixerSourceVoice::SetDistanceAttenuation(const float InDistanceAttenuation)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (DistanceAttenuation != InDistanceAttenuation)
		{
			DistanceAttenuation = InDistanceAttenuation;
			SourceManager->SetDistanceAttenuation(SourceId, InDistanceAttenuation);
		}
	}

	void FMixerSourceVoice::SetLPFFrequency(const float InLPFFrequency)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (LPFFrequency != InLPFFrequency)
		{
			LPFFrequency = InLPFFrequency;
			SourceManager->SetLPFFrequency(SourceId, LPFFrequency);
		}
	}

	void FMixerSourceVoice::SetHPFFrequency(const float InHPFFrequency)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (HPFFrequency != InHPFFrequency)
		{
			HPFFrequency = InHPFFrequency;
			SourceManager->SetHPFFrequency(SourceId, HPFFrequency);
		}
	}

	void FMixerSourceVoice::SetModVolume(const float InVolumeModBase)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (InVolumeModBase != VolumeModBase)
		{
			VolumeModBase = InVolumeModBase;
			SourceManager->SetModVolume(SourceId, VolumeModBase);
		}
	}

	void FMixerSourceVoice::SetModPitch(const float InPitchModBase)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (InPitchModBase != PitchModBase)
		{
			PitchModBase = InPitchModBase;
			SourceManager->SetModPitch(SourceId, InPitchModBase);
		}
	}

	void FMixerSourceVoice::SetModHPFFrequency(const float InHPFFrequencyModBase)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (InHPFFrequencyModBase != HPFFrequencyModBase)
		{
			HPFFrequencyModBase = InHPFFrequencyModBase;
			SourceManager->SetModHPFFrequency(SourceId, InHPFFrequencyModBase);
		}
	}

	void FMixerSourceVoice::SetModLPFFrequency(const float InLPFFrequencyModBase)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (InLPFFrequencyModBase != LPFFrequencyModBase)
		{
			LPFFrequencyModBase = InLPFFrequencyModBase;
			SourceManager->SetModLPFFrequency(SourceId, InLPFFrequencyModBase);
		}
	}

	void FMixerSourceVoice::SetModulationRouting(FSoundModulationDefaultRoutingSettings& RoutingSettings)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		SourceManager->SetModulationRouting(SourceId, RoutingSettings);
	}

	void FMixerSourceVoice::SetSourceBufferListener(FSharedISourceBufferListenerPtr& InSourceBufferListener, bool InShouldSourceBufferListenerZeroBuffer)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		SourceManager->SetSourceBufferListener(SourceId, InSourceBufferListener, InShouldSourceBufferListenerZeroBuffer);
	}

	void FMixerSourceVoice::SetChannelMap(const uint32 NumInputChannels, const Audio::FAlignedFloatBuffer& InChannelMap, const bool bInIs3D, const bool bInIsCenterChannelOnly)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		SourceManager->SetChannelMap(SourceId, NumInputChannels, InChannelMap, bInIs3D, bInIsCenterChannelOnly);
	}

	void FMixerSourceVoice::SetSpatializationParams(const FSpatializationParams& InParams)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		SourceManager->SetSpatializationParams(SourceId, InParams);
	}

	void FMixerSourceVoice::Play()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		bIsPlaying = true;
		bIsPaused = false;
		bIsActive = true;

		SourceManager->Play(SourceId);
	}

	void FMixerSourceVoice::Stop()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		bIsPlaying = false;
		bIsPaused = false;
		bIsActive = false;
		// We are instantly fading out with this stop command
		bStopFadedOut = true;
		SourceManager->Stop(SourceId);
	}

	void FMixerSourceVoice::StopFade(int32 NumFrames)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		bIsPaused = false;
		SourceManager->StopFade(SourceId, NumFrames);
	}

	int32 FMixerSourceVoice::GetSourceId() const
	{
		return SourceId;
	}

	float FMixerSourceVoice::GetDistanceAttenuation() const
	{
		return DistanceAttenuation;
	}

	float FMixerSourceVoice::GetDistance() const
	{
		return Distance;
	}

	void FMixerSourceVoice::Pause()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		bIsPaused = true;
		bIsActive = false;
		SourceManager->Pause(SourceId);
	}

	bool FMixerSourceVoice::IsPlaying() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return bIsPlaying;
	}

	bool FMixerSourceVoice::IsPaused() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return bIsPaused;
	}

	bool FMixerSourceVoice::IsActive() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return bIsActive;
	}

	bool FMixerSourceVoice::NeedsSpeakerMap() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return SourceManager->NeedsSpeakerMap(SourceId);
	}

	bool FMixerSourceVoice::IsUsingHRTFSpatializer(bool bDefaultValue) const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (SourceId != INDEX_NONE)
		{
			return SourceManager->IsUsingHRTFSpatializer(SourceId);
		}

		return bDefaultValue;
	}

	int64 FMixerSourceVoice::GetNumFramesPlayed() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return SourceManager->GetNumFramesPlayed(SourceId);
	}

	float FMixerSourceVoice::GetEnvelopeValue() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return SourceManager->GetEnvelopeValue(SourceId);
	}

#if ENABLE_AUDIO_DEBUG
	double FMixerSourceVoice::GetCPUCoreUtilization() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return SourceManager->GetCPUCoreUtilization(SourceId);
	}
#endif // ENABLE_AUDIO_DEBUG

	void FMixerSourceVoice::MixOutputBuffers(int32 InNumOutputChannels, const float SendLevel, EMixerSourceSubmixSendStage InSubmixSendStage, FAlignedFloatBuffer& OutWetBuffer) const
	{
		AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

		if (IsRenderingToSubmixes())
		{
			SourceManager->MixOutputBuffers(SourceId, InNumOutputChannels, SendLevel, InSubmixSendStage, OutWetBuffer);
		}
	}

	const ISoundfieldAudioPacket* FMixerSourceVoice::GetEncodedOutput(const FSoundfieldEncodingKey& InKey) const
	{
		AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

		if (IsRenderingToSubmixes())
		{
			return SourceManager->GetEncodedOutput(SourceId, InKey);
		}
		return nullptr;
	}

	const FQuat FMixerSourceVoice::GetListenerRotationForVoice() const
	{
		return SourceManager->GetListenerRotation(SourceId);
	}

	void FMixerSourceVoice::SetSubmixSendInfo(FMixerSubmixWeakPtr Submix, const float SendLevel, const EMixerSourceSubmixSendStage SendStage/* = EMixerSourceSubmixSendStage::PostDistanceAttenuation*/)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		FMixerSubmixPtr SubmixPtr = Submix.Pin();
		if (SubmixPtr.IsValid())
		{
			FMixerSourceSubmixSend* SubmixSend = SubmixSends.Find(SubmixPtr->GetId());

			if (!SubmixSend)
			{
				FMixerSourceSubmixSend NewSubmixSend;
				NewSubmixSend.Submix = Submix;
				NewSubmixSend.SendLevel = SendLevel;
				NewSubmixSend.bIsMainSend = false;
				NewSubmixSend.SubmixSendStage = SendStage;

				SubmixSends.Add(SubmixPtr->GetId(), NewSubmixSend);
				SourceManager->SetSubmixSendInfo(SourceId, NewSubmixSend);
			}
			else if (!FMath::IsNearlyEqual(SubmixSend->SendLevel, SendLevel) || SubmixSend->SubmixSendStage != SendStage)
			{
				SubmixSend->SendLevel = SendLevel;
				SubmixSend->SubmixSendStage = SendStage;
				SourceManager->SetSubmixSendInfo(SourceId, *SubmixSend);
			}
		}
	}

	void FMixerSourceVoice::ClearSubmixSendInfo(FMixerSubmixWeakPtr Submix)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		FMixerSubmixPtr SubmixPtr = Submix.Pin();
		if (SubmixPtr.IsValid())
		{
			FMixerSourceSubmixSend* SubmixSend = SubmixSends.Find(SubmixPtr->GetId());
			if (SubmixSend)
			{
				SourceManager->ClearSubmixSendInfo(SourceId, *SubmixSend);
				SubmixSends.Remove(SubmixPtr->GetId());
			}
		}
	}

	void FMixerSourceVoice::SetOutputToBusOnly(bool bInOutputToBusOnly)
	{
		if (bInOutputToBusOnly)
		{
			bEnableBusSends = true;
		}

		bEnableBaseSubmix = !bInOutputToBusOnly;
		bEnableSubmixSends = !bInOutputToBusOnly;
	}

	void FMixerSourceVoice::SetEnablement(bool bInEnableBusSendRouting, bool bInEnableMainSubmixOutput, bool bInEnableSubmixSendRouting)
	{
		bEnableBusSends = bInEnableBusSendRouting;
		bEnableBaseSubmix = bInEnableMainSubmixOutput;
		bEnableSubmixSends = bInEnableSubmixSendRouting;
	}


	void FMixerSourceVoice::SetAudioBusSendInfo(EBusSendType InBusSendType, uint32 AudioBusId, float BusSendLevel)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (!bEnableBusSends)
		{
			BusSendLevel = 0.0f;
		}

		SourceManager->SetBusSendInfo(SourceId, InBusSendType, AudioBusId, BusSendLevel);
	}

	bool FMixerSourceVoice::IsRenderingToSubmixes() const
	{
		return bEnableBaseSubmix || bEnableSubmixSends;
	}

	void FMixerSourceVoice::OnMixBus(FMixerSourceVoiceBuffer* OutMixerSourceBuffer)
	{
		AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

		check(OutMixerSourceBuffer->AudioData.Num() > 0);

		for (int32 i = 0; i < OutMixerSourceBuffer->AudioData.Num(); ++i)
		{
			OutMixerSourceBuffer->AudioData[i] = 0.0f;
		}
	}
}
