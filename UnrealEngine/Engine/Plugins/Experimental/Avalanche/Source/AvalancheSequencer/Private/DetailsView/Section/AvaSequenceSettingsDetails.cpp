// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequenceSettingsDetails.h"
#include "AvaSequence.h"
#include "AvaSequencer.h"
#include "CustomDetailsViewArgs.h"
#include "CustomDetailsViewModule.h"

#define LOCTEXT_NAMESPACE "AvaSequenceSettingsDetails"

FName FAvaSequenceSettingsDetails::GetSectionName() const
{
	return TEXT("Settings");
}

FText FAvaSequenceSettingsDetails::GetSectionDisplayName() const
{
	return LOCTEXT("SettingsLabel", "Sequence");
}

TSharedRef<SWidget> FAvaSequenceSettingsDetails::CreateContentWidget(const TSharedRef<FAvaSequencer>& InAvaSequencer)
{
	AvaSequencerWeak = InAvaSequencer;

	FCustomDetailsViewArgs CustomDetailsViewArgs;
	CustomDetailsViewArgs.IndentAmount = 0.f;
	CustomDetailsViewArgs.bShowCategories = true;
	CustomDetailsViewArgs.bAllowGlobalExtensions = true;
	CustomDetailsViewArgs.CategoryAllowList.Allow(TEXT("Sequence Settings"));
	CustomDetailsViewArgs.ExpansionState.Add(FCustomDetailsViewItemId::MakeCategoryId("Sequence Settings"), true);
	CustomDetailsViewArgs.ExpansionState.Add(FCustomDetailsViewItemId::MakePropertyId<UAvaSequence>(TEXT("Marks")), true);

	SettingsDetailsView = ICustomDetailsViewModule::Get().CreateCustomDetailsView(CustomDetailsViewArgs);

	InAvaSequencer->GetOnViewedSequenceChanged().AddSP(this, &FAvaSequenceSettingsDetails::OnViewedSequenceChanged);

	if (UAvaSequence* const ViewedSequence = InAvaSequencer->GetViewedSequence())
	{
		OnViewedSequenceChanged(ViewedSequence);	
	}

	return SettingsDetailsView.ToSharedRef();
}

bool FAvaSequenceSettingsDetails::ShouldShowSection() const
{
	TSharedPtr<FAvaSequencer> AvaSequencer = AvaSequencerWeak.Pin();
	return AvaSequencer.IsValid() && IsValid(AvaSequencer->GetViewedSequence());
}

void FAvaSequenceSettingsDetails::OnViewedSequenceChanged(UAvaSequence* InSequence)
{
	SettingsDetailsView->SetObject(InSequence);
}

#undef LOCTEXT_NAMESPACE
