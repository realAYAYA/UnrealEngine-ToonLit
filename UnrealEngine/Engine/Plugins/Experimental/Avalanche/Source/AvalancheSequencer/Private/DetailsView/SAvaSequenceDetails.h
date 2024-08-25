// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FAvaSequencer;
class IAvaSequenceSectionDetails;
class SHorizontalBox;
class SScrollBox;

DECLARE_DELEGATE_OneParam(FAvaSelectedSectionsChanged, const TSet<FName>&)

class SAvaSequenceDetails : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaSequenceDetails){}
		SLATE_ARGUMENT(TSet<FName>, InitiallySelectedSections)
		SLATE_EVENT(FAvaSelectedSectionsChanged, OnSelectedSectionsChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
		, const TSharedPtr<FAvaSequencer>& InAvaSequencer
		, TArray<TSharedRef<IAvaSequenceSectionDetails>>&& InSections);

private:
	void BuildSections();

	void OnSectionSelected(ECheckBoxState InCheckBoxState, FName InSectionName);

	bool IsSectionSelected(FName InSectionName) const;

	bool ShouldShowSection(TWeakPtr<IAvaSequenceSectionDetails> InSectionWeak) const;

	EVisibility GetSectionButtonVisibility(TWeakPtr<IAvaSequenceSectionDetails> InSectionWeak) const;

	EVisibility GetSectionContentVisibility(FName InSectionName, TWeakPtr<IAvaSequenceSectionDetails> InSectionWeak) const;

	ECheckBoxState GetSectionCheckBoxState(FName InSectionName) const;

	TWeakPtr<FAvaSequencer> AvaSequencerWeak;

	TArray<TSharedRef<IAvaSequenceSectionDetails>> Sections;

	TSet<FName> SelectedSections;

	FAvaSelectedSectionsChanged OnSelectedSectionsChanged;

	TSharedPtr<SHorizontalBox> SectionBox;

	TSharedPtr<SScrollBox> ContentBox;
};
