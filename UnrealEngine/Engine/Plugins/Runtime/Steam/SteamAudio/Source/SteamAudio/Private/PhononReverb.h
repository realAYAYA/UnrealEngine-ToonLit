//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "IAudioExtensionPlugin.h"
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundEffectPreset.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "PhononCommon.h"
#include "phonon.h"
#include "PhononReverb.generated.h"


// Forward Declaration
class FSubmixEffectReverbPlugin;


namespace SteamAudio
{
	class FEnvironment;

	struct FReverbSource
	{
		FReverbSource();

		IPLhandle ConvolutionEffect;
		float IndirectContribution;
		float DipolePower;
		float DipoleWeight;
		IPLAudioBuffer InBuffer;
		TArray<float> IndirectInArray;
	};

	/************************************************************************/
	/* FPhononReverb. Reverb plugin for Steam Audio.                        */
	/************************************************************************/
	class FPhononReverb : public IAudioReverb
	{
	public:
		FPhononReverb();
		~FPhononReverb();

		virtual void Initialize(const FAudioPluginInitializationParams InitializationParams) override;
		virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UReverbPluginSourceSettingsBase* InSettings) override;
		virtual void OnReleaseSource(const uint32 SourceId) override;
		virtual FSoundEffectSubmixPtr GetEffectSubmix() override;
		virtual USoundSubmix* GetSubmix() override;
		virtual void ProcessSourceAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData) override;
		
		void ProcessMixedAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData);
		void SetEnvironment(FEnvironment* InEnvironment);
		void CreateReverbEffect();
		void UpdateListener(const FVector& Position, const FVector& Forward, const FVector& Up, const FVector& Right);

	private:
		FSoundEffectSubmixPtr SubmixEffect;
		TWeakObjectPtr<USoundSubmix> ReverbSubmix;

		IPLhandle BinauralRenderer;
		IPLhandle IndirectBinauralEffect;
		IPLhandle IndirectPanningEffect;
		IPLhandle ReverbConvolutionEffect;
		IPLAudioBuffer DryBuffer;

		IPLAudioBuffer IndirectOutBuffer;
		int32 AmbisonicsChannels;
		IPLfloat32** IndirectOutDeinterleaved;
		TArray<float> IndirectOutArray;

		IPLAudioBuffer IndirectIntermediateBuffer;
		TArray<float> IndirectIntermediateArray;

		IPLAudioFormat InputAudioFormat;
		IPLAudioFormat ReverbInputAudioFormat;
		IPLAudioFormat IndirectOutputAudioFormat;
		IPLAudioFormat BinauralOutputAudioFormat;

		FCriticalSection ListenerCriticalSection;
		IPLVector3 ListenerPosition;
		IPLVector3 ListenerForward;
		IPLVector3 ListenerRight;
		IPLVector3 ListenerUp;
		bool bListenerInitialized;

		EIplSpatializationMethod CachedSpatializationMethod;

		TArray<FReverbSource> ReverbSources;

		TArray<float> ReverbIndirectInArray;

		TAudioPluginListenerPtr PluginManagerPtr;

		FAudioPluginInitializationParams AudioPluginInitializationParams;
		FEnvironment* Environment;
	};
}

class FSubmixEffectReverbPlugin : public FSoundEffectSubmix
{
public:
	FSubmixEffectReverbPlugin();

	virtual void Init(const FSoundEffectSubmixInitData& InSampleRate) override;
	virtual uint32 GetDesiredInputChannelCountOverride() const override;
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;
	virtual void OnPresetChanged() override;

	void SetPhononReverbPlugin(SteamAudio::FPhononReverb* PhononReverbPlugin);

private:
	SteamAudio::FPhononReverb* PhononReverbPlugin;
};

USTRUCT()
struct FSubmixEffectReverbPluginSettings
{
	GENERATED_USTRUCT_BODY()
};

UCLASS()
class USubmixEffectReverbPluginPreset : public USoundEffectSubmixPreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SubmixEffectReverbPlugin)

	UPROPERTY(EditAnywhere, Category = SubmixEffectPreset)
	FSubmixEffectReverbPluginSettings Settings;
};
