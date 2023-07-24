// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveTableBank.h"
#include "UObject/ObjectSaveContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaveTableBank)

#define LOCTEXT_NAMESPACE "WaveTable"


TSharedPtr<Audio::IProxyData> UWaveTableBank::CreateProxyData(const Audio::FProxyDataInitParams& InitParams)
{
	return MakeShared<FWaveTableBankAssetProxy>(*this);
}

#if WITH_EDITOR
void UWaveTableBank::RefreshWaveTables()
{
	int32 MaxPCMSize = 0;
	for (FWaveTableBankEntry& Entry : Entries)
	{
		TArrayView<const float> EditSourceView = Entry.Transform.WaveTableSettings.GetEditSourceView();
		MaxPCMSize = FMath::Max(MaxPCMSize, EditSourceView.Num());
	}

	WaveTableSizeMB = 0.0f;
	WaveTableLengthSec = 0.0f;
	for (FWaveTableBankEntry& Entry : Entries)
	{
		TArray<float>& TransformWaveTable = Entry.Transform.WaveTable;
		TransformWaveTable.Empty();

		const int32 WaveTableSize = WaveTable::GetWaveTableSize(Resolution, Entry.Transform.Curve, MaxPCMSize);
		TransformWaveTable.AddZeroed(WaveTableSize);

		float FinalValue = Entry.Transform.GetFinalValue();
		Entry.Transform.CreateWaveTable(TransformWaveTable, FinalValue, bBipolar);
		Entry.Transform.SetFinalValue(FinalValue);

		WaveTableSizeMB += sizeof(float) * WaveTableSize;
		WaveTableLengthSec = FMath::Max(WaveTableLengthSec, WaveTableSize / 48000.f);
	}

	WaveTableSizeMB /= FMath::Square(1024);
}

void UWaveTableBank::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	FProperty* Property = InPropertyChangedEvent.Property;
	if (Property && InPropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		RefreshWaveTables();
	}

	Super::PostEditChangeProperty(InPropertyChangedEvent);
}

void UWaveTableBank::PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedEvent)
{
	FProperty* Property = InPropertyChangedEvent.Property;
	if (Property && InPropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		RefreshWaveTables();
	}

	Super::PostEditChangeChainProperty(InPropertyChangedEvent);
}

void UWaveTableBank::PreSave(FObjectPreSaveContext InSaveContext)
{
	if (!InSaveContext.IsCooking())
	{
		RefreshWaveTables();
	}

	Super::PreSave(InSaveContext);
}
#endif // WITH_EDITOR
#undef LOCTEXT_NAMESPACE // WaveTable
