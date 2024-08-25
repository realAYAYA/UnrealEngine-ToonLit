// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaSequenceSectionDetails.h"

class FAvaSequencePlaybackDetails : public IAvaSequenceSectionDetails
{
	//~ Begin IAvaSequenceSectionDetails
	virtual FName GetSectionName() const override;
	virtual FText GetSectionDisplayName() const override;
	virtual TSharedRef<SWidget> CreateContentWidget(const TSharedRef<FAvaSequencer>& InAvaSequencer) override;
	virtual bool ShouldShowSection() const;
	//~ End IAvaSequenceSectionDetails

	TWeakPtr<FAvaSequencer> AvaSequencerWeak;
};
