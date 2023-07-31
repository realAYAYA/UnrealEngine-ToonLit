// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "HAL/Platform.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "WaveTableSettings.generated.h"


namespace WaveTable
{
	// Converts a ratio [0.0, 1.0] to an index within the given number of array elements.
	UE_NODISCARD static FORCEINLINE int32 RatioToIndex(float InRatio, int32 InArrayNum)
	{
		check(InArrayNum > 0);
		check(InRatio >= 0.0f);

		return FMath::Min(InArrayNum - 1, FMath::TruncToInt32(InRatio * InArrayNum));
	};
} // namespace WaveTable


UENUM(BlueprintType)
enum class EWaveTableResolution : uint8
{
	None		= 0 UMETA(Hidden),

	Res_8		= 3 UMETA(DisplayName = "8"),
	Res_16		= 4 UMETA(DisplayName = "16"),
	Res_32		= 5 UMETA(DisplayName = "32"),
	Res_64		= 6 UMETA(DisplayName = "64"),
	Res_128		= 7 UMETA(DisplayName = "128"),
	Res_256		= 8 UMETA(DisplayName = "256"),
	Res_512		= 9 UMETA(DisplayName = "512"),
	Res_1024	= 10 UMETA(DisplayName = "1024"),
	Res_2048	= 11 UMETA(DisplayName = "2048"),
	Res_4096	= 12 UMETA(DisplayName = "4096"),

	// Maximum "static" power-of-two WaveTable resolution
	Res_Max		= Res_4096 UMETA(Hidden),

	// Takes largest source file length (if WaveTable was produced
	// by a file, PCM stream, SoundWave, etc.) of superset of waves,
	// or Res_Max if all superset of waves were produced by curves. This
	// can be very memory intensive if source file is large!
	Maximum
};


USTRUCT()
struct WAVETABLE_API FWaveTableSettings
{
	GENERATED_USTRUCT_BODY()

	// File to import
	UPROPERTY(EditAnywhere, Category = Options, meta = (FilePathFilter = "Audio Files (*.aif, *.flac, *.ogg, *.wav)|*.aif;*.flac;*.ogg;*.wav"))
	FFilePath FilePath;

	// Index of channel in file to build WaveTable from (wraps if channel is greater than number in file)
	UPROPERTY(EditAnywhere, Category = Options, meta = (ClampMin = "0.0"))
	int32 ChannelIndex = 0;

	// Percent to phase shift of table
	UPROPERTY(EditAnywhere, Category = Options, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Phase = 0.0f;

	// Percent to remove from beginning of sampled WaveTable.
	UPROPERTY(EditAnywhere, Category = Options, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Top = 0.0f;

	// Percent to remove from end of sampled WaveTable.
	UPROPERTY(EditAnywhere, Category = Options, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Tail = 0.0f;

	// Percent to fade in over.
	UPROPERTY(EditAnywhere, Category = Options, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FadeIn  = 0.0f;

	// Percent to fade out over.
	UPROPERTY(EditAnywhere, Category = Options, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FadeOut = 0.0f;

	// Whether or not to normalize the WaveTable.
	UPROPERTY(EditAnywhere, Category = Options, meta = (DisplayAfter = "FilePath"))
	bool bNormalize = true;

	// Whether or not to remove offset from original file
	// (analogous to "DC offset" in circuit theory).
	UPROPERTY(EditAnywhere, Category = Options, meta = (DisplayAfter = "bNormalize"))
	bool bRemoveOffset = false;

	// SourcePCM Data
	UPROPERTY()
	TArray<float> SourcePCMData;

	// Returns a TArrayView of Source PCM values that are on when
	// importing with the given settings (restricted effectively
	// due to top/tail settings).
	TArrayView<const float> GetEditSourceView() const;
};

UENUM(BlueprintType)
enum class EWaveTableCurve : uint8
{
	Linear		UMETA(DisplayName = "Linear (Ramp In)"),
	Linear_Inv	UMETA(DisplayName = "Linear (Ramp Out)"),
	Exp			UMETA(DisplayName = "Exponential"),
	Exp_Inverse UMETA(DisplayName = "Exponential (Inverse)"),
	Log			UMETA(DisplayName = "Log"),

	Sin			UMETA(DisplayName = "Sin (90 deg)"),
	Sin_Full	UMETA(DisplayName = "Sin (360 deg)"),
	SCurve		UMETA(DisplayName = "Sin (+/- 90 deg)"),

	// Reference a shared curve asset
	Shared		UMETA(DisplayName = "Shared"),

	// Design a custom curve unique to the owning transform
	Custom		UMETA(DisplayName = "Custom"),

	// Generate WaveTable from audio file
	File		UMETA(DisplayName = "File"),

	Count		UMETA(Hidden),
};

namespace WaveTable
{
	// Returns WaveTable size given WaveTableResolution and optional Curve or PCM size. If resolution is none, uses default value associated
	// with provided curve. If set to file, will use either the largest static resolution (Res_Max, or 4096) or InMaxPCMSize, whichever is larger.
	WAVETABLE_API int32 GetWaveTableSize(EWaveTableResolution InWaveTableResolution, EWaveTableCurve InCurve, int32 InMaxPCMSize = INDEX_NONE);
} // namespace WaveTable