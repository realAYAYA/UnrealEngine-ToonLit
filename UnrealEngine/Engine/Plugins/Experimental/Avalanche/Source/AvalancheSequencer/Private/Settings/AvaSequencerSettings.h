// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSequencePreset.h"
#include "AvaSequencerDisplayRate.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/Object.h"
#include "AvaSequencerSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, meta = (DisplayName = "Sequencer"))
class UAvaSequencerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* SettingsCategory = TEXT("Motion Design");
	static constexpr const TCHAR* SettingsSection  = TEXT("AvaSequencerSettings");

	UAvaSequencerSettings();

	FFrameRate GetDisplayRate() const
	{
		return DisplayRate.FrameRate;
	}

	double GetStartTime() const
	{
		return StartTime;
	}

	double GetEndTime() const
	{
		return EndTime;
	}

	TConstArrayView<FAvaSequencePreset> GetDefaultSequencePresets() const;

	const TSet<FAvaSequencePreset>& GetCustomSequencePresets() const
	{
		return CustomSequencePresets;
	}

private:
	/** The default display rate to use for new sequences */
	UPROPERTY(Config, EditAnywhere, Category = "Playback")
	FAvaSequencerDisplayRate DisplayRate;

	/** The default start time to use for new sequences */
	UPROPERTY(Config, EditAnywhere, Category = "Playback")
	double StartTime = 0.0;

	/** The default end time to use for new sequences */
	UPROPERTY(Config, EditAnywhere, Category = "Playback")
	double EndTime = 2.0;

	/** Sequence Presets that are uniquely identified by their Preset Name */
	UPROPERTY(Config, EditAnywhere, Category = "Sequencer")
	TSet<FAvaSequencePreset> CustomSequencePresets;
};
