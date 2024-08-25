// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IAudioProxyInitializer.h"
#include "AudioBus.generated.h"

class FAudioDevice;

// The number of channels to mix audio into the source bus
UENUM(BlueprintType)
enum class EAudioBusChannels : uint8
{
	Mono = 0,
	Stereo = 1,
	Quad = 3,
	FivePointOne = 5,
	SevenPointOne = 7,
	MaxChannelCount = 8
};

namespace AudioBusUtils
{
	static EAudioBusChannels ConvertIntToEAudioBusChannels(const int32 InValue)
	{
		switch (InValue)
		{
		case 1: return EAudioBusChannels::Mono;
		case 2:	return EAudioBusChannels::Stereo;
		case 4:	return EAudioBusChannels::Quad;
		case 6:	return EAudioBusChannels::FivePointOne;
		case 8:	return EAudioBusChannels::SevenPointOne;
		default:
			// TODO alex.perez: LogAudio undeclared identifier, is it ok to add the header that has LogAudio, or should we move this function to another place?
			//UE_LOG(LogAudio, Error, TEXT("Number of channels: %d not available in AudioBusChannels configurations. Make sure that the number of channels used is 1, 2, 4, 6 or 8."), InValue);
			check(false);
			return EAudioBusChannels::Mono;
		}
	};
}

class FAudioBusProxy;

using FAudioBusProxyPtr = TSharedPtr<FAudioBusProxy, ESPMode::ThreadSafe>;
class FAudioBusProxy final : public Audio::TProxyData<FAudioBusProxy>
{
public:
	IMPL_AUDIOPROXY_CLASS(FAudioBusProxy);

	ENGINE_API explicit FAudioBusProxy(UAudioBus* InAudioBus);

	FAudioBusProxy(const FAudioBusProxy& Other) = default;

	virtual ~FAudioBusProxy() override {}

	uint32 AudioBusId = INDEX_NONE;
	int32 NumChannels = INDEX_NONE;
};

// Function to retrieve an audio bus buffer given a handle
// static float* GetAudioBusBuffer(const FAudioBusHandle& AudioBusHandle);

// An audio bus is an object which represents an audio patch cord. Audio can be sent to it. It can be sonified using USoundSourceBuses.
// Instances of the audio bus are created in the audio engine. 
UCLASS(ClassGroup = Sound, meta = (BlueprintSpawnableComponent), MinimalAPI)
class UAudioBus : public UObject, public IAudioProxyDataFactory
{
	GENERATED_UCLASS_BODY()

public:

	/** Number of channels to use for the Audio Bus. */
	UPROPERTY(EditAnywhere, Category = BusProperties)
	EAudioBusChannels AudioBusChannels = EAudioBusChannels::Mono;

	//~ Begin UObject
	ENGINE_API virtual void BeginDestroy() override;
	//~ End UObject

	// Returns the number of channels of the audio bus in integer format
	int32 GetNumChannels() const { return (int32)AudioBusChannels + 1; }

	//~ Begin UObject Interface.
#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface.

	//~ Begin IAudioProxy Interface
	ENGINE_API virtual TSharedPtr<Audio::IProxyData> CreateProxyData(const Audio::FProxyDataInitParams& InitParams) override;
	//~ End IAudioProxy Interface
};
