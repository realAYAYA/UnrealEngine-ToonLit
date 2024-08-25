// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaSequenceSectionDetails.h"

class FAvaSequencer;
class FName;
class FText;
class ICustomDetailsView;
class SBorder;
class SWidget;
struct FKeyEditData;

class FAvaSequenceSelectionDetails : public IAvaSequenceSectionDetails
{
protected:
	//~ Begin IAvaSequenceSectionDetails
	virtual FName GetSectionName() const override;
	virtual FText GetSectionDisplayName() const override;
	virtual TSharedRef<SWidget> CreateContentWidget(const TSharedRef<FAvaSequencer>& InAvaSequencer) override;
	//~ End IAvaSequenceSectionDetails

	virtual void OnSequencerSelectionChanged();

	virtual TSharedRef<SWidget> CreateHintText(const FText& InMessage);

	FKeyEditData GetKeyEditData() const;

	TWeakPtr<FAvaSequencer> AvaSequencerWeak;

	TSharedPtr<SBorder> ContentBorder;

	TSharedPtr<ICustomDetailsView> DetailsView;
};
