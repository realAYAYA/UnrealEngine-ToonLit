// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Audio.h"
#include "DSP/MultithreadedPatching.h"
#include "Sound/AudioSettings.h"

namespace Audio
{
	class FMixerSourceManager;

	// Struct holding mappings of runtime source ids (bus instances) to bus send level
	struct FAudioBusSend
	{
		int32 SourceId = INDEX_NONE;
		float SendLevel = 0.0f;
	};

	// Bus instance data. Holds source id bus instances and bus sends data
	class FMixerAudioBus
	{
	public:
		// Allow anybody to add a pre-existing patch output object to the audio bus
		void AddNewPatchOutput(const FPatchOutputStrongPtr& InPatchOutputStrongPtr);

		// Allow anybody to write audio into this audio bus from any thread.
		void AddNewPatchInput(const FPatchInput& InPatchInput);

		// Allow anybody to write audio into this audio bus from any thread.
		void RemovePatchInput(const FPatchInput& InPatchInput);

	private:

		// Creates an audio bus.
		// SourceMnager		The owning source manager object.
		// bInIsAutomatic	Whether or not this audio bus was created automatically via source buses.
		// InNumChannels	The number of channels of the source bus.
		// InNumFrames		The number of frames to mix per mix buffer call. I.e. source manager render size.
		FMixerAudioBus(FMixerSourceManager* SourceManager, bool bInIsAutomatic, int32 InNumChannels);
		
		// Sets whether or not this audio bus is automatic.
		void SetAutomatic(bool bInIsAutomatic) { bIsAutomatic = bInIsAutomatic; }

		// Returns if this is a manual audio bus vs automatic
		bool IsAutomatic() const { return bIsAutomatic; }

		// Returns the number of channels of the audio bus
		int32 GetNumChannels() const { return NumChannels; }

		// Update the mixer bus after a render block
		void Update();

		// Adds a source id for instances of this bus
		void AddInstanceId(const int32 InSourceInstanceId, int32 InNumOutputChannels);

		// Removes the source id from this bus. Returns true if there are no more instances or sends.
		bool RemoveInstanceId(const int32 InSourceId);

		// Adds a bus send to the bus
		void AddSend(EBusSendType BusSendType, const FAudioBusSend& InBusSend);

		// Removes the source instance from this bus's send list
		bool RemoveSend(EBusSendType BusSendType, const int32 InSourceId);

		// Gets the current mixed bus buffer
		const float* GetCurrentBusBuffer() const;

		// Gets the previous mixed bus buffer
		const float* GetPreviousBusBuffer() const;

		// Compute the mixed buffer
		void MixBuffer();

		// Copies the current internal buffer to a provided output buffer. Only supports mono or stereo input/output formats.
		void CopyCurrentBuffer(Audio::FAlignedFloatBuffer& InChannelMap, int32 InNumOutputChannels, FAlignedFloatBuffer& OutBuffer, int32 NumOutputFrames) const;
		void CopyCurrentBuffer(int32 InNumOutputChannels, FAlignedFloatBuffer& OutBuffer, int32 NumOutputFrames) const;

		// If this bus was constructed before
		void SetNumOutputChannels(int32 InNumOutputChannels);

		// Array of instance ids. These are sources which are instances of this.
		// It's possible for this data to have bus sends but no instance ids.
		// This means a source would send its audio to the bus if the bus had an instance.
		// Once and instance plays, it will then start sending its audio to the bus instances.
		TArray<int32> InstanceIds;

		// Bus sends to this instance
		TArray<FAudioBusSend> AudioBusSends[(int32)EBusSendType::Count];

		// The mixed source data. This is double-buffered to allow buses to send audio to themselves.
		// Buses feed audio to each other by storing their previous buffer. Current buses mix in previous other buses (including themselves)
		FAlignedFloatBuffer MixedSourceData[2];

		// The index of the bus data currently being rendered
		int32 CurrentBufferIndex;

		// The number of channels of this bus
		int32 NumChannels;

		// The number of output frames
		int32 NumFrames;

		// Owning soruce manager
		FMixerSourceManager* SourceManager;

		// Multiple places can produce and consume from audio buses
		Audio::FPatchMixer PatchMixer;
		Audio::FPatchSplitter PatchSplitter;

		// Was created manually, not via source buses.
		bool bIsAutomatic;

		friend FMixerSourceManager;
		friend FMixerSubmix;
	};

}
