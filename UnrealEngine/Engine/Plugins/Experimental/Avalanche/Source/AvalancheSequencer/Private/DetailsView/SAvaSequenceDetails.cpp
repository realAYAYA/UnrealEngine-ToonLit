// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaSequenceDetails.h"
#include "Framework/Application/SlateApplication.h"
#include "Section/IAvaSequenceSectionDetails.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaSequenceDetails"

void SAvaSequenceDetails::Construct(const FArguments& InArgs
	, const TSharedPtr<FAvaSequencer>& InAvaSequencer
	, TArray<TSharedRef<IAvaSequenceSectionDetails>>&& InSections)
{
	AvaSequencerWeak = InAvaSequencer;
	Sections = MoveTemp(InSections);

	OnSelectedSectionsChanged = InArgs._OnSelectedSectionsChanged;
	SelectedSections = InArgs._InitiallySelectedSections;

	SectionBox = SNew(SHorizontalBox);

	ContentBox = SNew(SScrollBox)
		.Orientation(Orient_Vertical);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SectionBox.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			ContentBox.ToSharedRef()
		]
	];

	BuildSections();
}

void SAvaSequenceDetails::BuildSections()
{
	SectionBox->ClearChildren();
	ContentBox->ClearChildren();

	if (!AvaSequencerWeak.IsValid() || Sections.IsEmpty())
	{
		return;
	}

	TSharedRef<FAvaSequencer> AvaSequencer = AvaSequencerWeak.Pin().ToSharedRef();

	for (const TSharedRef<IAvaSequenceSectionDetails>& Section : Sections)
	{
		const FName SectionName = Section->GetSectionName();

		SectionBox->AddSlot()
		.Padding(2.f, 2.f)
		[
			SNew(SBox)
			.Padding(FMargin(0))
			.Visibility(this, &SAvaSequenceDetails::GetSectionButtonVisibility, Section->AsWeak())
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.OnCheckStateChanged(this, &SAvaSequenceDetails::OnSectionSelected, SectionName)
				.IsChecked(this, &SAvaSequenceDetails::GetSectionCheckBoxState, SectionName)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "SmallText")
					.Text(Section->GetSectionDisplayName())
					.Justification(ETextJustify::Center)
				]
			]
		];

		ContentBox->AddSlot()
		.AutoSize()
		[
			SNew(SBox)
			.Visibility(this, &SAvaSequenceDetails::GetSectionContentVisibility, SectionName, Section->AsWeak())
			[
				Section->CreateContentWidget(AvaSequencer)
			]
		];
	}

	if (SelectedSections.Num() == 0)
	{
		// Find first section that is visible
		TSharedRef<IAvaSequenceSectionDetails>* const FoundSection = Sections.FindByPredicate(
			[](const TSharedRef<IAvaSequenceSectionDetails>& InSection)
			{
				return InSection->ShouldShowSection();
			});

		checkf(FoundSection, TEXT("There should be at least one section visible to have initially selected!"));

		SelectedSections.Add((*FoundSection)->GetSectionName());
	}
}

void SAvaSequenceDetails::OnSectionSelected(ECheckBoxState InCheckBoxState, FName InSectionName)
{
	const bool bIsControlDown = FSlateApplication::Get().GetModifierKeys().IsControlDown();

	switch (InCheckBoxState)
	{
	case ECheckBoxState::Unchecked:
		if (bIsControlDown)
		{
			SelectedSections.Remove(InSectionName);

			// Force to always have a Selected Section, making it unable to deselect the last one
			if (SelectedSections.IsEmpty())
			{
				SelectedSections.Add(InSectionName);
			}
		}
		else
		{
			SelectedSections.Reset();
			SelectedSections.Add(InSectionName);
		}
		break;

	case ECheckBoxState::Checked:
		if (bIsControlDown)
		{
			SelectedSections.Add(InSectionName);
		}
		else
		{
			SelectedSections.Reset();
			SelectedSections.Add(InSectionName);
		}
		break;
	}

	OnSelectedSectionsChanged.ExecuteIfBound(SelectedSections);
}

bool SAvaSequenceDetails::IsSectionSelected(FName InSectionName) const
{
	return SelectedSections.Contains(InSectionName);
}

bool SAvaSequenceDetails::ShouldShowSection(TWeakPtr<IAvaSequenceSectionDetails> InSectionWeak) const
{
	TSharedPtr<IAvaSequenceSectionDetails> Section = InSectionWeak.Pin();
	return Section.IsValid() && Section->ShouldShowSection();
}

EVisibility SAvaSequenceDetails::GetSectionButtonVisibility(TWeakPtr<IAvaSequenceSectionDetails> InSectionWeak) const
{
	return ShouldShowSection(InSectionWeak)
		? EVisibility::SelfHitTestInvisible
		: EVisibility::Collapsed;
}

EVisibility SAvaSequenceDetails::GetSectionContentVisibility(FName InSectionName, TWeakPtr<IAvaSequenceSectionDetails> InSectionWeak) const
{
	return IsSectionSelected(InSectionName) && ShouldShowSection(InSectionWeak)
		? EVisibility::SelfHitTestInvisible
		: EVisibility::Collapsed;
}

ECheckBoxState SAvaSequenceDetails::GetSectionCheckBoxState(FName InSectionName) const
{
	return IsSectionSelected(InSectionName)
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE 
