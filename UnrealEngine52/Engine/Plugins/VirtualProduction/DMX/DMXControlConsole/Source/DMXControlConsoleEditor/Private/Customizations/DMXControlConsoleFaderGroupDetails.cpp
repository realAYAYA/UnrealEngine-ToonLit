// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderGroupDetails.h"

#include "DMXControlConsole.h"
#include "DMXControlConsoleEditorManager.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Library/DMXEntityFixturePatch.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Layout/Visibility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFaderGroupDetails"

void FDMXControlConsoleFaderGroupDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	PropertyUtilities = InDetailLayout.GetPropertyUtilities();

	IDetailCategoryBuilder& FaderGroupCategory = InDetailLayout.EditCategory("DMX Fader Group", FText::GetEmpty());

	const TSharedPtr<IPropertyHandle> FaderGroupNameHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderGroup::GetFaderGroupNamePropertyName());
	InDetailLayout.HideProperty(FaderGroupNameHandle);
	const TSharedPtr<IPropertyHandle> EditorColorHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderGroup::GetEditorColorPropertyName());
	InDetailLayout.HideProperty(EditorColorHandle);

	FaderGroupCategory.AddProperty(FaderGroupNameHandle);
	FaderGroupCategory.AddProperty(EditorColorHandle)
		.Visibility(TAttribute<EVisibility>::CreateSP(this, &FDMXControlConsoleFaderGroupDetails::GetEditorColorVisibility));

	FaderGroupCategory.AddCustomRow(FText::GetEmpty())
		.NameContent()
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(LOCTEXT("FixturePatch", "Fixture Patch"))
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.IsReadOnly(true)
			.Text(this, &FDMXControlConsoleFaderGroupDetails::GetFixturePatchText)
		];

	FaderGroupCategory.AddCustomRow(FText::GetEmpty())
		.Visibility(TAttribute<EVisibility>::CreateSP(this, &FDMXControlConsoleFaderGroupDetails::GetClearButtonVisibility))
		.WholeRowContent()
		[
			SNew(SBox)
			.Padding(5.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.OnClicked(this, &FDMXControlConsoleFaderGroupDetails::OnClearButtonClicked)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(LOCTEXT("ClearButtonTitle", "Clear"))
				]
			]
		];
}

void FDMXControlConsoleFaderGroupDetails::ForceRefresh() const
{
	if (!PropertyUtilities.IsValid())
	{
		return;
	}
	
	PropertyUtilities->ForceRefresh();
}

bool FDMXControlConsoleFaderGroupDetails::DoSelectedFaderGroupsHaveAnyFixturePatches() const
{
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();

	bool bHasFixturePatch = false;
	if (SelectedFaderGroupsObjects.IsEmpty())
	{
		return bHasFixturePatch;
	}

	for (const TWeakObjectPtr<UObject>& SelectedFaderGroupObject : SelectedFaderGroupsObjects)
	{
		const UDMXControlConsoleFaderGroup* SelectedFaderGroup = Cast<UDMXControlConsoleFaderGroup>(SelectedFaderGroupObject);
		if (!SelectedFaderGroup)
		{
			continue;
		}

		const UDMXEntityFixturePatch* FixturePatch = SelectedFaderGroup->GetFixturePatch();
		if (!FixturePatch)
		{
			continue;
		}

		bHasFixturePatch = true;
		break;
	}

	return bHasFixturePatch;
}

FReply FDMXControlConsoleFaderGroupDetails::OnClearButtonClicked()
{
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	for (const TWeakObjectPtr<UObject>& SelectedFaderGroupObject : SelectedFaderGroupsObjects)
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = Cast<UDMXControlConsoleFaderGroup>(SelectedFaderGroupObject);
		if (!SelectedFaderGroup || !SelectedFaderGroup->HasFixturePatch())
		{
			continue;
		}

		const FScopedTransaction FaderGroupFixturePatchClearTransaction(LOCTEXT("FaderGroupFixturePatchClearTransaction", "Clear Fixture Patch"));
		SelectedFaderGroup->PreEditChange(nullptr);

		SelectedFaderGroup->Reset();

		SelectedFaderGroup->PostEditChange();
	}

	return FReply::Handled();
}

FText FDMXControlConsoleFaderGroupDetails::GetFixturePatchText() const
{
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();

	if (SelectedFaderGroupsObjects.IsEmpty())
	{
		return FText::GetEmpty();
	}
	else if (SelectedFaderGroupsObjects.Num() > 1 && DoSelectedFaderGroupsHaveAnyFixturePatches())
	{
		return FText::FromString(TEXT("Multiple Values"));
	}

	const UDMXControlConsoleFaderGroup* SelectedFaderGroup = Cast<UDMXControlConsoleFaderGroup>(SelectedFaderGroupsObjects[0]);
	if (!SelectedFaderGroup)
	{
		return FText::GetEmpty();
	}

	if (!SelectedFaderGroup->HasFixturePatch())
	{
		return LOCTEXT("FaderGroupFixturePatchNoneName", "None");
	}

	UDMXEntityFixturePatch* FixturePatch = SelectedFaderGroup->GetFixturePatch();
	return FText::FromString(FixturePatch->GetDisplayName());
}

EVisibility FDMXControlConsoleFaderGroupDetails::GetEditorColorVisibility() const
{
	return DoSelectedFaderGroupsHaveAnyFixturePatches() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FDMXControlConsoleFaderGroupDetails::GetClearButtonVisibility() const
{
	return DoSelectedFaderGroupsHaveAnyFixturePatches() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
