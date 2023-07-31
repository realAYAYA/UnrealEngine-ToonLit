// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OVR_Audio.h"
#include "Misc/Paths.h"
#include "OculusAudioDllManager.h"
#include "OculusAudioSettings.h"
#include "Containers/Ticker.h"
#include "AudioMixerDevice.h"
#include "Containers/Ticker.h"

/************************************************************************/
/* OculusAudioSpatializationAudioMixer									*/
/* This implementation of IAudioSpatialization uses the Oculus Audio	*/
/* library to render audio sources with HRTF spatialization.			*/
/************************************************************************/
class OculusAudioSpatializationAudioMixer : public IAudioSpatialization
{
public:
	OculusAudioSpatializationAudioMixer();
	~OculusAudioSpatializationAudioMixer();

	void ClearContext();

	//~ Begin IAudioSpatialization 
	virtual void Initialize(const FAudioPluginInitializationParams InitializationParams) override;
	virtual void Shutdown() override;
	virtual bool IsSpatializationEffectInitialized() const override;
	virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, USpatializationPluginSourceSettingsBase* InSettings) override;
	virtual void SetSpatializationParameters(uint32 VoiceId, const FSpatializationParams& InParams) override;
	virtual void ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData) override;
	//~ End IAudioSpatialization
	
	bool Tick(float DeltaTime);

	// Helper function to convert from UE coords to OVR coords.
	static FVector ToOVRVector(const FVector& InVec)
	{
		return FVector(InVec.Y, InVec.Z, -InVec.X);
	}

	static FVector ToOVRVector(const Audio::FChannelPositionInfo& ChannelPositionInfo)
	{
		FVector OvrVector;
		OvrVector.X = ChannelPositionInfo.Radius * FMath::Sin(ChannelPositionInfo.Azimuth) * FMath::Cos(ChannelPositionInfo.Elevation);
		OvrVector.Y = ChannelPositionInfo.Radius * FMath::Sin(ChannelPositionInfo.Azimuth) * FMath::Sin(ChannelPositionInfo.Elevation);
		OvrVector.Z = ChannelPositionInfo.Radius * FMath::Cos(ChannelPositionInfo.Azimuth);

		return OvrVector;
	}

	static const int MIXER_CLASS_ID = 0x98765432;
	const int ClassID = MIXER_CLASS_ID;
private:

	void ApplyOculusAudioSettings(const UOculusAudioSettings* Settings);

	TArray<FSpatializationParams> Params;

	ovrAudioContext Context;
	FCriticalSection ContextLock;

	FAudioPluginInitializationParams InitParams;
	FTSTicker::FDelegateHandle TickDelegateHandle;
};