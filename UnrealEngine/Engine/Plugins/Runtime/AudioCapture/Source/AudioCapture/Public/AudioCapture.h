// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "HAL/ThreadSafeBool.h"
#include "DSP/Delay.h"
#include "DSP/EnvelopeFollower.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Generators/AudioGenerator.h"
#include "AudioCaptureCore.h"

#include "AudioCapture.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAudioCapture, Log, All);

class FAudioCaptureModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

// Struct defining the time synth global quantization settings
USTRUCT(BlueprintType)
struct AUDIOCAPTURE_API FAudioCaptureDeviceInfo
{
	GENERATED_USTRUCT_BODY()

	// The name of the audio capture device
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioCapture")
	FName DeviceName;

	// The number of input channels
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioCapture")
	int32 NumInputChannels = 0;

	// The sample rate of the audio capture device
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioCapture")
	int32 SampleRate = 0;
};

// Class which opens up a handle to an audio capture device.
// Allows other objects to get audio buffers from the capture device.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AUDIOCAPTURE_API UAudioCapture : public UAudioGenerator
{
	GENERATED_BODY()

public:
	UAudioCapture();
	~UAudioCapture();

	bool OpenDefaultAudioStream();

	// Returns the audio capture device info
	UFUNCTION(BlueprintCallable, Category = "AudioCapture")
	bool GetAudioCaptureDeviceInfo(FAudioCaptureDeviceInfo& OutInfo);

	// Starts capturing audio
	UFUNCTION(BlueprintCallable, Category = "AudioCapture")
	void StartCapturingAudio();

	// Stops capturing audio
	UFUNCTION(BlueprintCallable, Category = "AudioCapture")
	void StopCapturingAudio();

	// Returns true if capturing audio
	UFUNCTION(BlueprintCallable, Category = "AudioCapture")
	bool IsCapturingAudio();

protected:

	Audio::FAudioCapture AudioCapture;
};

UCLASS()
class AUDIOCAPTURE_API UAudioCaptureFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "Audio Capture")
	static class UAudioCapture* CreateAudioCapture();
};
