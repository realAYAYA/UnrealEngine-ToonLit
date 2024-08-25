// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequencerSettings.h"
#include "AvaSequenceDefaultTags.h"
#include "Misc/FrameRate.h"

namespace UE::AvaSequencer::Private
{
	FAvaSequencePreset CreateDefaultPreset(FName InSequenceType, const FAvaTagSoftHandle& InSequenceTag)
	{
		FAvaSequencePreset Preset;
		Preset.PresetName    = InSequenceType;
		Preset.SequenceLabel = InSequenceType;
		Preset.SequenceTag   = InSequenceTag;
		Preset.bEnableLabel  = true;
		Preset.bEnableTag    = true;
		return Preset;
	}

	TArray<FAvaSequencePreset> MakeDefaultSequencePresets()
	{
		const FAvaSequenceDefaultTags& DefaultTags = FAvaSequenceDefaultTags::Get();

		// Guids found via the entries that DefaultSequenceTags had for In/Out/Change
		TArray<FAvaSequencePreset> DefaultPresets =
			{
				CreateDefaultPreset(TEXT("In")    , DefaultTags.In),
				CreateDefaultPreset(TEXT("Out")   , DefaultTags.Out),
				CreateDefaultPreset(TEXT("Change"), DefaultTags.Change),
			};

		// Configure Change Marks
		{
			FAvaSequencePreset& ChangePreset = DefaultPresets[2];
			ChangePreset.bEnableMarks = true;

			FAvaMarkSetting& MarkSetting = ChangePreset.Marks.AddDefaulted_GetRef();
        	MarkSetting.Label = TEXT("A");
			MarkSetting.FrameNumber = 60;
		}

		return DefaultPresets;
	}
}

UAvaSequencerSettings::UAvaSequencerSettings()
{
	CategoryName = TEXT("Motion Design");
	SectionName = TEXT("Sequencer");

	DisplayRate.FrameRate = FFrameRate(60000, 1001);
}

TConstArrayView<FAvaSequencePreset> UAvaSequencerSettings::GetDefaultSequencePresets() const
{
	static const TArray<FAvaSequencePreset> DefaultPresets = UE::AvaSequencer::Private::MakeDefaultSequencePresets();
	return DefaultPresets;
}
