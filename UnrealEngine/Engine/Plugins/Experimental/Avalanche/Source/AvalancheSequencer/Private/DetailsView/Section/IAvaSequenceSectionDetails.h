// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class FAvaSequencer;
class FName;
class FText;
class SWidget;

class IAvaSequenceSectionDetails : public TSharedFromThis<IAvaSequenceSectionDetails>
{
public:
	virtual ~IAvaSequenceSectionDetails() {}

	virtual FName GetSectionName() const = 0;

	virtual FText GetSectionDisplayName() const = 0;

	virtual TSharedRef<SWidget> CreateContentWidget(const TSharedRef<FAvaSequencer>& InAvaSequencer) = 0;

	virtual bool ShouldShowSection() const { return true; }
};
