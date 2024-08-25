// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupControllerPanel.h"

#include "DMXControlConsoleFaderGroup.h"
#include "IDMXControlConsoleFaderGroupElement.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Models/DMXControlConsoleFaderGroupControllerModel.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroupControllerPanel"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorFaderGroupControllerPanel::Construct(const FArguments& InArgs, const TWeakPtr<FDMXControlConsoleFaderGroupControllerModel>& InFaderGroupControllerModel)
	{
		if (!ensureMsgf(InFaderGroupControllerModel.IsValid(), TEXT("Invalid fader group controller model, cannot create fader group controller panel correctly.")))
		{
			return;
		}

		WeakFaderGroupControllerModel = InFaderGroupControllerModel;

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
						SAssignNew(FaderGroupControllerNameTextBox, SEditableTextBox)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.OnTextCommitted(this, &SDMXControlConsoleEditorFaderGroupControllerPanel::OnFaderGroupControllerNameCommitted)
						.Text(this, &SDMXControlConsoleEditorFaderGroupControllerPanel::OnGetFaderGroupControllerNameText)
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
						.Text(this, &SDMXControlConsoleEditorFaderGroupControllerPanel::OnGetFaderGroupControllerFIDText)
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
						.Text(this, &SDMXControlConsoleEditorFaderGroupControllerPanel::OnGetFaderGroupControllerUniverseText)
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
						.Text(this, &SDMXControlConsoleEditorFaderGroupControllerPanel::OnGetFaderGroupControllerAddressText)
						.MinDesiredWidth(40.f)
					]
				]
			];
	}

	UDMXControlConsoleFaderGroupController* SDMXControlConsoleEditorFaderGroupControllerPanel::GetFaderGroupController() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		return FaderGroupControllerModel.IsValid() ? FaderGroupControllerModel->GetFaderGroupController() : nullptr;
	}

	FText SDMXControlConsoleEditorFaderGroupControllerPanel::OnGetFaderGroupControllerNameText() const
	{
		const UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		return IsValid(FaderGroupController) ? FText::FromString(FaderGroupController->GetUserName()) : FText::GetEmpty();
	}

	FText SDMXControlConsoleEditorFaderGroupControllerPanel::OnGetFaderGroupControllerFIDText() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		if (!FaderGroupControllerModel.IsValid())
		{
			return LOCTEXT("FIDNone", "None");
		}

		if (!FaderGroupControllerModel->HasSingleFaderGroup())
		{
			return LOCTEXT("FIDMany", "Many");
		}

		const UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupControllerModel->GetFirstAvailableFaderGroup();
		const UDMXEntityFixturePatch* FixturePatch = FaderGroup ? FaderGroup->GetFixturePatch() : nullptr;
		int32 FixtureID;
		if (FixturePatch && FixturePatch->FindFixtureID(FixtureID))
		{
			return FText::FromString(FString::FromInt(FixtureID));
		}

		return LOCTEXT("FIDNone", "None");
	}

	FText SDMXControlConsoleEditorFaderGroupControllerPanel::OnGetFaderGroupControllerUniverseText() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		if (!FaderGroupControllerModel.IsValid())
		{
			return LOCTEXT("UniverseIDNone", "None");
		}

		if (!FaderGroupControllerModel->HasSingleFaderGroup())
		{
			return LOCTEXT("UniverseIDMany", "Many");
		}

		const UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupControllerModel->GetFirstAvailableFaderGroup();
		const UDMXEntityFixturePatch* FixturePatch = FaderGroup ? FaderGroup->GetFixturePatch() : nullptr;
		if (FixturePatch)
		{
			const int32 UniverseID = FixturePatch->GetUniverseID();
			return FText::FromString(FString::FromInt(UniverseID));
		}
		else if (FaderGroup && !FaderGroup->GetElements().IsEmpty())
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
				return LOCTEXT("UniverseIDMany", "Many");
			}
		}

		return LOCTEXT("UniverseIDNone", "None");
	}

	FText SDMXControlConsoleEditorFaderGroupControllerPanel::OnGetFaderGroupControllerAddressText() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		if (!FaderGroupControllerModel.IsValid())
		{
			return LOCTEXT("AddressNone", "None");
		}

		if (!FaderGroupControllerModel->HasSingleFaderGroup())
		{
			return LOCTEXT("AddressMany", "Many");
		}

		int32 StartingAddress = -1;
		int32 EndingAddress = -1;

		const UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupControllerModel->GetFirstAvailableFaderGroup();
		const UDMXEntityFixturePatch* FixturePatch = FaderGroup ? FaderGroup->GetFixturePatch() : nullptr;
		if (FixturePatch)
		{
			StartingAddress = FixturePatch->GetStartingChannel();
			EndingAddress = FixturePatch->GetEndingChannel();
		}
		else if (FaderGroup && !FaderGroup->GetElements().IsEmpty())
		{
			const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> SortedElements = FaderGroup->GetElements(true);
			StartingAddress = SortedElements[0]->GetStartingAddress();
			EndingAddress = SortedElements.Last()->GetEndingAddress();
		}

		const FString StartingAddressString = StartingAddress >= 0 ? FString::FromInt(StartingAddress) : FString();
		const FString EndingAddressString = EndingAddress >= 0 ? FString::FromInt(EndingAddress) : FString();
		if (!StartingAddressString.IsEmpty() && !EndingAddressString.IsEmpty())
		{
			return FText::FromString(FString::Format(TEXT("{0}-{1}"), { StartingAddressString, EndingAddressString }));
		}

		return LOCTEXT("AddressNone", "None");
	}

	void SDMXControlConsoleEditorFaderGroupControllerPanel::OnFaderGroupControllerNameCommitted(const FText& NewName, ETextCommit::Type InCommit)
	{
		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (FaderGroupController && FaderGroupControllerNameTextBox.IsValid())
		{
			const FScopedTransaction FaderGroupControllerNameTransaction(LOCTEXT("FaderGroupControllerNameTransaction", "Edit Fader Group Name"));
			FaderGroupController->PreEditChange(UDMXControlConsoleFaderGroupController::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroupController::GetUserNamePropertyName()));
			FaderGroupController->SetUserName(NewName.ToString());
			FaderGroupController->PostEditChange();

			FaderGroupControllerNameTextBox->SetText(FText::FromString(FaderGroupController->GetUserName()));
		}
	}
}

#undef LOCTEXT_NAMESPACE
