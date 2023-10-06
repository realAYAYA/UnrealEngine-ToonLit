// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupPanel.h"

#include "DMXControlConsoleFaderGroup.h"
#include "IDMXControlConsoleFaderGroupElement.h"
#include "Library/DMXEntityFixturePatch.h"
#include "ScopedTransaction.h"
#include "Views/SDMXControlConsoleEditorFaderGroupView.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroupCore"

void SDMXControlConsoleEditorFaderGroupPanel::Construct(const FArguments& InArgs, const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& InFaderGroupView)
{
	FaderGroupView = InFaderGroupView;

	if (!ensureMsgf(FaderGroupView.IsValid(), TEXT("Invalid fader group view, cannot create fader group widget correctly.")))
	{
		return;
	}
	
	ChildSlot
	[
		SNew(SVerticalBox)
		// Fader Group Name section
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(2.f, 0.f)
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("FaderGroupName_Label", "Name"))
			]

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.Padding(0.f, 1.f)
			.AutoHeight()
			[
				SAssignNew(FaderGroupNameTextBox, SEditableTextBox)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.OnTextCommitted(this, &SDMXControlConsoleEditorFaderGroupPanel::OnFaderGroupNameCommitted)
				.Text(this, &SDMXControlConsoleEditorFaderGroupPanel::OnGetFaderGroupNameText)
				.MinDesiredWidth(40.f)
			]
		]

		// Fader Group Fixture ID section
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(2.f, 10.f, 0.f, 0.f)
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("FaderGroupFID_Label", "FID"))
			]

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.Padding(0.f, 1.f)
			.AutoHeight()
			[
				SNew(SEditableTextBox)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.IsReadOnly(true)
				.BackgroundColor(FLinearColor(.06f, .06f, .06f, .2f))
				.Justification(ETextJustify::Center)
				.Text(this, &SDMXControlConsoleEditorFaderGroupPanel::OnGetFaderGroupFIDText)
				.MinDesiredWidth(40.f)
			]
		]

		// Universe ID section
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(2.f, 10.f, 0.f, 0.f)
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("FaderGroupUniverse_Label", "Uni"))
			]

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.Padding(0.f, 1.f)
			.AutoHeight()
			[
				SNew(SEditableTextBox)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.IsReadOnly(true)
				.BackgroundColor(FLinearColor(.06f, .06f, .06f, .2f))
				.Justification(ETextJustify::Center)
				.Text(this, &SDMXControlConsoleEditorFaderGroupPanel::OnGetFaderGroupUniverseText)
				.MinDesiredWidth(40.f)
			]
		]

		// Fader Group Address Range section
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(2.f, 10.f, 0.f, 0.f)
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("FaderGroupAddress_Label", "Addr"))
			]

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.Padding(0.f, 1.f)
			.AutoHeight()
			[
				SNew(SEditableTextBox)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.IsReadOnly(true)
				.BackgroundColor(FLinearColor(.06f, .06f, .06f, .2f))
				.Justification(ETextJustify::Center)
				.Text(this, &SDMXControlConsoleEditorFaderGroupPanel::OnGetFaderGroupAddressText)
				.MinDesiredWidth(40.f)
			]
		]

		+ SVerticalBox::Slot()
		[
			SNew(SBox)
		]
	];
}

UDMXControlConsoleFaderGroup* SDMXControlConsoleEditorFaderGroupPanel::GetFaderGroup() const
{
	return FaderGroupView.IsValid() ? FaderGroupView.Pin()->GetFaderGroup() : nullptr;
}

FText SDMXControlConsoleEditorFaderGroupPanel::OnGetFaderGroupNameText() const
{
	const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	return IsValid(FaderGroup) ? FText::FromString(FaderGroup->GetFaderGroupName()) : FText::GetEmpty();
}

FText SDMXControlConsoleEditorFaderGroupPanel::OnGetFaderGroupFIDText() const
{
	const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	if (FaderGroup && FaderGroup->HasFixturePatch())
	{
		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		int32 FixtureID;
		if (FixturePatch->FindFixtureID(FixtureID))
		{
			return FText::FromString(FString::FromInt(FixtureID));
		}
	}

	return FText::FromString("None");
}

FText SDMXControlConsoleEditorFaderGroupPanel::OnGetFaderGroupUniverseText() const
{
	const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	if (FaderGroup && !FaderGroup->GetElements().IsEmpty())
	{
		if (FaderGroup->HasFixturePatch())
		{
			const int32 UniverseID = FaderGroup->GetFixturePatch()->GetUniverseID();
			return FText::FromString(FString::FromInt(UniverseID));
		}
		else
		{
			const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> SortedElements = FaderGroup->GetElements(true);

			const int32 StartingUniverse = SortedElements[0]->GetUniverseID();
			const int32 EndingUniverse = SortedElements.Last()->GetUniverseID();
			const FString StartingUniverseString = FString::FromInt(StartingUniverse);
			const FString EndingUniverseString = FString::FromInt(EndingUniverse);
			if (StartingUniverse == EndingUniverse)
			{
				return FText::FromString(StartingUniverseString);
			}
			else
			{
				return FText::FromString(TEXT("Many"));
			}
		}
	}

	return FText::FromString("None");
}

FText SDMXControlConsoleEditorFaderGroupPanel::OnGetFaderGroupAddressText() const
{
	const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	if (FaderGroup && !FaderGroup->GetElements().IsEmpty())
	{
		int32 StartingAddress;
		int32 EndingAddress;
		FString StartingAddressString;
		FString EndingAddressString;

		if (FaderGroup->HasFixturePatch())
		{
			StartingAddress = FaderGroup->GetFixturePatch()->GetStartingChannel();
			EndingAddress = FaderGroup->GetFixturePatch()->GetEndingChannel();
			StartingAddressString = FString::FromInt(StartingAddress);
			EndingAddressString = FString::FromInt(EndingAddress);
		}
		else
		{
			const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> SortedElements = FaderGroup->GetElements(true);
			StartingAddress = SortedElements[0]->GetStartingAddress();
			EndingAddress = SortedElements.Last()->GetEndingAddress();
			StartingAddressString = FString::FromInt(StartingAddress);
			EndingAddressString = FString::FromInt(EndingAddress);
		}

		return FText::FromString(FString::Format(TEXT("{0}-{1}"), { StartingAddressString, EndingAddressString }));
	}

	return FText::FromString("None");
}

void SDMXControlConsoleEditorFaderGroupPanel::OnFaderGroupNameCommitted(const FText& NewName, ETextCommit::Type InCommit)
{
	UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	if (FaderGroup && FaderGroupNameTextBox.IsValid())
	{
		const FScopedTransaction FaderGroupTransaction(LOCTEXT("FaderGroupTransaction", "Edit Fader Group Name"));
		FaderGroup->PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetFaderGroupNamePropertyName()));
		FaderGroup->SetFaderGroupName(NewName.ToString());
		FaderGroup->PostEditChange();

		FaderGroupNameTextBox->SetText(FText::FromString(FaderGroup->GetFaderGroupName()));
	}
}

#undef LOCTEXT_NAMESPACE
