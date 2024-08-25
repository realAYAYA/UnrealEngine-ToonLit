// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaSequenceSectionDetails.h"

class UAvaSequence;
class ICustomDetailsView;

class FAvaSequenceSettingsDetails : public IAvaSequenceSectionDetails
{
	//~ Begin IAvaSequenceSectionDetails
	virtual FName GetSectionName() const override;
	virtual FText GetSectionDisplayName() const override;
	virtual TSharedRef<SWidget> CreateContentWidget(const TSharedRef<FAvaSequencer>& InAvaSequencer) override;
	virtual bool ShouldShowSection() const override;
	//~ End IAvaSequenceSectionDetails

	void OnViewedSequenceChanged(UAvaSequence* InSequence);

	TWeakPtr<FAvaSequencer> AvaSequencerWeak;

	TSharedPtr<ICustomDetailsView> SettingsDetailsView;
};
