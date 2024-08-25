// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMarkSetting.h"
#include "AvaTagSoftHandle.h"
#include "AvaSequencePreset.generated.h"

class UAvaSequence;

/** Simple settings to apply to a sequence */
USTRUCT()
struct FAvaSequencePreset
{
	GENERATED_BODY()

	FAvaSequencePreset() = default;

	explicit FAvaSequencePreset(FName InPresetName)
		: PresetName(InPresetName)
	{
	}

	bool ShouldModifySequence() const;

	bool ShouldModifyMovieScene() const;

	void ApplyPreset(UAvaSequence* InSequence) const;

	bool operator==(const FAvaSequencePreset& InOther) const
	{
		return PresetName == InOther.PresetName;
	}

	friend uint32 GetTypeHash(const FAvaSequencePreset& InPreset)
	{
		return GetTypeHash(InPreset.PresetName);
	}

	/** Name to identify this preset */
	UPROPERTY(EditAnywhere, Category="Sequence")
	FName PresetName;

	/** If not none, the sequence label to set */
	UPROPERTY(EditAnywhere, Category="Sequence", meta=(EditCondition="bEnableLabel"))
	FName SequenceLabel;

	UPROPERTY(EditAnywhere, Category="Sequence", meta=(EditCondition="bEnableTag"))
	FAvaTagSoftHandle SequenceTag;

	UPROPERTY(EditAnywhere, Category="Sequence", meta=(EditCondition="bEnableMarks"))
	TArray<FAvaMarkSetting> Marks;

	UPROPERTY(meta = (InlineEditConditionToggle))
	bool bEnableLabel = false;

	UPROPERTY(meta = (InlineEditConditionToggle))
	bool bEnableTag = false;

	UPROPERTY(meta = (InlineEditConditionToggle))
	bool bEnableMarks = false;
};
