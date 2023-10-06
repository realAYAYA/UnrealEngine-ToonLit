// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/AudioEngineSubsystem.h"
#include "Templates/TypeHash.h"
#include "UObject/StrongObjectPtr.h"

#include "AudioBusSubsystem.generated.h"

class UAudioBus;

namespace Audio
{
	// Forward declarations 
	class FMixerAudioBus;
	class FMixerSourceManager;
	class FPatchInput;
	struct FPatchOutput;
	typedef TSharedPtr<FPatchOutput, ESPMode::ThreadSafe> FPatchOutputStrongPtr;

	struct FAudioBusKey
	{
		uint32 ObjectId = INDEX_NONE; // from a corresponding UObject (UAudioBus) if applicable
		uint32 InstanceId = INDEX_NONE;

		FAudioBusKey()
			: InstanceId(InstanceIdCounter++)
		{
		}

		// For construction with a given UObject unique id 
		FAudioBusKey(uint32 InObjectId)
			: ObjectId(InObjectId)
		{
		}

		const bool IsValid() const
		{
			return ObjectId != INDEX_NONE || InstanceId != INDEX_NONE;
		}

		FORCEINLINE friend uint32 GetTypeHash(const FAudioBusKey& Key)
		{
			return HashCombineFast(Key.ObjectId, Key.InstanceId);
		}
		 		
		FORCEINLINE friend bool operator==(const FAudioBusKey& InLHS, const FAudioBusKey& InRHS) 
		{
			return (InLHS.ObjectId == InRHS.ObjectId) && (InLHS.InstanceId == InRHS.InstanceId);
		}

		FORCEINLINE friend bool operator!=(const FAudioBusKey& InLHS, const FAudioBusKey& InRHS) 
		{
			return !(InLHS == InRHS);
		}
		

	private:
		static AUDIOMIXER_API std::atomic<uint32> InstanceIdCounter;
	};
}

/**
*  UAudioBusSubsystem
*/
UCLASS(MinimalAPI)
class UAudioBusSubsystem : public UAudioEngineSubsystem
{
	GENERATED_BODY()

public:
	AUDIOMIXER_API UAudioBusSubsystem();
	virtual ~UAudioBusSubsystem() = default;

	//~ Begin USubsystem interface
	AUDIOMIXER_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	AUDIOMIXER_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	AUDIOMIXER_API virtual void Deinitialize() override;
	//~ End USubsystem interface

	// Audio bus API from FMixerDevice
	AUDIOMIXER_API void StartAudioBus(Audio::FAudioBusKey InAudioBusKey, int32 InNumChannels, bool bInIsAutomatic);
	AUDIOMIXER_API void StopAudioBus(Audio::FAudioBusKey InAudioBusKey);
	AUDIOMIXER_API bool IsAudioBusActive(Audio::FAudioBusKey InAudioBusKey) const;
	
	AUDIOMIXER_API Audio::FPatchInput AddPatchInputForAudioBus(Audio::FAudioBusKey InAudioBusKey, int32 InFrames, int32 InChannels, float InGain = 1.f);
	AUDIOMIXER_API Audio::FPatchOutputStrongPtr AddPatchOutputForAudioBus(Audio::FAudioBusKey InAudioBusKey, int32 InFrames, int32 InChannels, float InGain = 1.f);

	AUDIOMIXER_API void InitDefaultAudioBuses();
	AUDIOMIXER_API void ShutdownDefaultAudioBuses();

private:
	struct FActiveBusData
	{
		Audio::FAudioBusKey BusKey = 0;
		int32 NumChannels = 0;
		bool bIsAutomatic = false;
	};

	TArray<TStrongObjectPtr<UAudioBus>> DefaultAudioBuses; 
	// The active audio bus list accessible on the game thread
	TMap<Audio::FAudioBusKey, FActiveBusData> ActiveAudioBuses_GameThread;
};
