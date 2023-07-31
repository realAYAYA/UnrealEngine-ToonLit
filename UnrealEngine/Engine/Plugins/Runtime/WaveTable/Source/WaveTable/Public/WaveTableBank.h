// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Algo/Transform.h"
#include "IAudioProxyInitializer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectSaveContext.h"
#include "WaveTable.h"
#include "WaveTableTransform.h"

#include "WaveTableBank.generated.h"


USTRUCT()
struct WAVETABLE_API FWaveTableBankEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Table)
	FWaveTableTransform Transform;
};

UCLASS(config = Engine, editinlinenew, BlueprintType)
class WAVETABLE_API UWaveTableBank : public UObject, public IAudioProxyDataFactory
{
	GENERATED_BODY()

public:
	// Number of sampled cached for each curve in the given bank.
	UPROPERTY(EditAnywhere, Category = Options)
	EWaveTableResolution Resolution = EWaveTableResolution::Res_256;

	// Determines if output from curve/wavetable are to be clamped between [-1.0f, 1.0f]
	// (i.e. for waveform generation, oscillation, etc.) or between [0.0f, 1.0f]
	// (i.e. for enveloping) when sampling curve/wavetable
	UPROPERTY(EditAnywhere, Category = Options)
	bool bBipolar = true;

#if WITH_EDITORONLY_DATA
	// Sum total size of all WaveTable data within the given bank
	UPROPERTY(VisibleAnywhere, Category = Options, meta = (DisplayName = "WaveTable Size (MB)"))
	float WaveTableSizeMB = 0.0f;

	// Length of all WaveTable samples in bank in seconds (at 48kHz)
	UPROPERTY(VisibleAnywhere, Category = Options, meta = (DisplayName = "WaveTable Length (sec)"))
	float WaveTableLengthSec = 0.0f;
#endif // WITH_EDITORONLY_DATA

	/** Tables within the given bank */
	UPROPERTY(EditAnywhere, Category = Options)
	TArray<FWaveTableBankEntry> Entries;

	/* IAudioProxyDataFactory Implementation */
	virtual TUniquePtr<Audio::IProxyData> CreateNewProxyData(const Audio::FProxyDataInitParams& InitParams) override;

#if WITH_EDITOR
	void RefreshWaveTables();

	virtual void PreSave(FObjectPreSaveContext InSaveContext) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedEvent) override;
#endif // WITH_EDITOR
};

class WAVETABLE_API FWaveTableBankAssetProxy : public Audio::TProxyData<FWaveTableBankAssetProxy>, public TSharedFromThis<FWaveTableBankAssetProxy, ESPMode::ThreadSafe>
{
public:
	IMPL_AUDIOPROXY_CLASS(FWaveTableBankAssetProxy);

	FWaveTableBankAssetProxy(const FWaveTableBankAssetProxy& InAssetProxy)
		: WaveTables(InAssetProxy.WaveTables)
	{
	}

	FWaveTableBankAssetProxy(const UWaveTableBank& InWaveTableBank)
	{
		Algo::Transform(InWaveTableBank.Entries, WaveTables, [](const FWaveTableBankEntry& Entry)
		{
			return Entry.Transform.WaveTable;
		});
	}

	virtual Audio::IProxyDataPtr Clone() const
	{
		return MakeUnique<FWaveTableBankAssetProxy>(*this);
	}

	virtual const TArray<WaveTable::FWaveTable>& GetWaveTables() const
	{
		return WaveTables;
	}

protected:
	TArray<WaveTable::FWaveTable> WaveTables;
};
using FWaveTableBankAssetProxyPtr = TSharedPtr<FWaveTableBankAssetProxy, ESPMode::ThreadSafe>;
