//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "IAudioExtensionPlugin.h"
#include "PhononCommon.h"
#include "phonon.h"

class UOcclusionPluginSourceSettingsBase;

namespace SteamAudio
{
	class FEnvironment;

	struct FDirectSoundSource
	{
		FDirectSoundSource();

		FCriticalSection CriticalSection;
		IPLDirectSoundPath DirectSoundPath;
		IPLhandle DirectSoundEffect;
		EIplDirectOcclusionMethod DirectOcclusionMethod;
		EIplDirectOcclusionMode DirectOcclusionMode;
		IPLAudioBuffer InBuffer;
		IPLAudioBuffer OutBuffer;
		IPLSource SourceData;
		float Radius;
		float DipolePower;
		float DipoleWeight;
		bool bDirectAttenuation;
		bool bAirAbsorption;
		bool bNeedsUpdate;
	};

	/************************************************************************/
	/* FPhononOcclusion                                                     */
	/* Scene-dependent audio occlusion plugin. Receives updates from        */
	/* an FPhononPluginManager on the game thread on player position and    */
	/* geometry, and performs geometry-aware filtering of the direct path   */
	/* of an audio source.                                                  */
	/************************************************************************/
	class FPhononOcclusion : public IAudioOcclusion
	{
	public:
		FPhononOcclusion();
		~FPhononOcclusion();

		//~ Begin IAudioOcclusion
		virtual void Initialize(const FAudioPluginInitializationParams InitializationParams) override;
		virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UOcclusionPluginSourceSettingsBase* InSettings) override;
		virtual void OnReleaseSource(const uint32 SourceId) override;
		virtual void ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData) override;
		//~ End IAudioOcclusion

		// Receives updates on listener positions from the game thread using this function call.
		void UpdateDirectSoundSources(const FVector& ListenerPosition, const FVector& ListenerForward, const FVector& ListenerUp, const FVector& ListenerRight);

		// Sets up handle to the environment handle owned by the FPhononPluginManager.
		void SetEnvironment(FEnvironment* InEnvironment);

	private:
		IPLAudioFormat InputAudioFormat;
		IPLAudioFormat OutputAudioFormat;

		FEnvironment* Environment;

		// Cached array of direct sound sources to be occluded.
		TArray<FDirectSoundSource> DirectSoundSources;
	};
}
