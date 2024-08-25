// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_MidiStepSequence.h"
#include "HarmonixMetasound/DataTypes/MidiStepSequence.h"

TSoftClassPtr<UObject> UAssetDefinition_MidiStepSequence::GetAssetClass() const
{
	return UMidiStepSequence::StaticClass();
}

FText UAssetDefinition_MidiStepSequence::GetAssetDisplayName() const
{
	return NSLOCTEXT("AssetTypeActions", "MIDIStepSequenceDefinition", "MIDI Step Sequence");
}

FLinearColor  UAssetDefinition_MidiStepSequence::GetAssetColor() const
{
	return FLinearColor(1.0f, 0.5f, 0.0f);
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MidiStepSequence::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::Audio / NSLOCTEXT("Harmonix", "HmxAssetCategoryName", "Harmonix") };
	return Categories;
}

bool UAssetDefinition_MidiStepSequence::CanImport() const
{
	return true;
}

