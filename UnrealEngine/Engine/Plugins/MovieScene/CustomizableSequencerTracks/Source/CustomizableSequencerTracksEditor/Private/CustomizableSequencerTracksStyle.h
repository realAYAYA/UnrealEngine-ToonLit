// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

class USequencerTrackBP;

class FCustomizableSequencerTracksStyle : public FSlateStyleSet
{
public:

	static FName StyleName;

	/** Access the singleton instance for this style set */
	static FCustomizableSequencerTracksStyle& Get();

	void RegisterNewTrackType(TSubclassOf<USequencerTrackBP> TrackType);

private:

	FCustomizableSequencerTracksStyle();
	~FCustomizableSequencerTracksStyle();
};
