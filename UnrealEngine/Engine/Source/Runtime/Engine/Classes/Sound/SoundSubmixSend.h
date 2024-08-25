// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Curves/CurveFloat.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "SoundSubmixSend.generated.h"


// Forward Declarations
class USoundSubmixBase;

UENUM(BlueprintType)
enum class EAudioSpectrumBandPresetType: uint8
{
	/** Band which contains frequencies generally related to kick drums. */
	KickDrum,

	/** Band which contains frequencies generally related to snare drums. */
	SnareDrum,

	/** Band which contains frequencies generally related to vocals. */
	Voice,

	/** Band which contains frequencies generally related to cymbals. */
	Cymbals	
};

USTRUCT(BlueprintType)
struct FSoundSubmixSpectralAnalysisBandSettings
{
	GENERATED_USTRUCT_BODY()

	// The frequency band for the magnitude to analyze
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSpectralAnalysis, meta = (ClampMin = "10.0", ClampMax = "20000.0", UIMin = "10.0", UIMax = "10000.0"))
	float BandFrequency = 440.0f;

	// The attack time for the FFT band interpolation for delegate callback
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSpectralAnalysis, meta = (ClampMin = "0.0", UIMin = "10.0", UIMax = "10000.0"))
	int32 AttackTimeMsec = 10;

	// The release time for the FFT band interpolation for delegate callback
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSpectralAnalysis, meta = (ClampMin = "0.0", UIMin = "10.0", UIMax = "10000.0"))
	int32 ReleaseTimeMsec = 500;

	// The ratio of the bandwidth divided by the center frequency. Only used when the spectral analysis type is set to Constant Q.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = SubmixSpectralAnalysis, meta = (ClampMin = "0.001", UIMin = "0.1", UIMax = "100.0"))
	float QFactor = 10.0f;
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnSubmixEnvelopeBP, const TArray<float>&, Envelope);

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnSubmixSpectralAnalysisBP, const TArray<float>&, Magnitude);

UENUM(BlueprintType)
enum class EAudioRecordingExportType : uint8
{
	// Exports a USoundWave.
	SoundWave,

	// Exports a WAV file.
	WavFile
};

UENUM(BlueprintType)
enum class ESendLevelControlMethod : uint8
{
	// A send based on linear interpolation between a distance range and send-level range
	Linear,

	// A send based on a supplied curve
	CustomCurve,

	// A manual send level (Uses the specified constant send level value. Useful for 2D sounds.)
	Manual,
};

// Common set of settings that are uses as submix sends.
USTRUCT(BlueprintType)
struct FSoundSubmixSendInfoBase
{
	GENERATED_USTRUCT_BODY()

	ENGINE_API FSoundSubmixSendInfoBase();

	/*
		Manual: Use Send Level only
		Linear: Interpolate between Min and Max Send Levels based on listener distance (between Distance Min and Distance Max)
		Custom Curve: Use the float curve to map Send Level to distance (0.0-1.0 on curve maps to Distance Min - Distance Max)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSend)
	ESendLevelControlMethod SendLevelControlMethod;
	
	// The submix to send the audio to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSend)
	TObjectPtr<USoundSubmixBase> SoundSubmix;

	// The amount of audio to send
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSend, meta = (DisplayName = "Manual Send Level"))
	float SendLevel;

	// Whether to disable the 0-1 clamp for manual SendLevel control
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSend)
	bool DisableManualSendClamp;

	// The amount to send to master when sound is located at a distance equal to value specified in the min send distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSend)
	float MinSendLevel;

	// The amount to send to master when sound is located at a distance equal to value specified in the max send distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSend)
	float MaxSendLevel;

	// The min distance to send to the master
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSend)
	float MinSendDistance;

	// The max distance to send to the master
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSend)
	float MaxSendDistance;

	// The custom reverb send curve to use for distance-based send level.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSend)
	FRuntimeFloatCurve CustomSendLevelCurve;
};

UENUM(BlueprintType)
enum class ESubmixSendStage : uint8
{
	// Whether to do the send pre distance attenuation
	PostDistanceAttenuation,

	// Whether to do the send post distance attenuation
	PreDistanceAttenuation,
};

USTRUCT(BlueprintType)
struct FSoundSubmixSendInfo : public FSoundSubmixSendInfoBase
{
	GENERATED_BODY();

	/** Defines at what mix stage the send should happen.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSend)
	ESubmixSendStage SendStage = ESubmixSendStage::PostDistanceAttenuation;
};
