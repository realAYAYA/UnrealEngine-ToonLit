// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderGroupDetails.h"

#include "Algo/AnyOf.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DMXControlConsoleFaderGroup.h"
#include "IPropertyUtilities.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFaderGroupDetails"

namespace UE::DMX::Private
{
	FDMXControlConsoleFaderGroupDetails::FDMXControlConsoleFaderGroupDetails(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
		: WeakEditorModel(InWeakEditorModel)
	{}

	TSharedRef<IDetailCustomization> FDMXControlConsoleFaderGroupDetails::MakeInstance(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
	{
		return MakeShared<FDMXControlConsoleFaderGroupDetails>(InWeakEditorModel);
	}

	void FDMXControlConsoleFaderGroupDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
	{
		PropertyUtilities = InDetailLayout.GetPropertyUtilities();

		IDetailCategoryBuilder& FaderGroupCategory = InDetailLayout.EditCategory("DMX Fader Group", FText::GetEmpty());

		const TSharedRef<IPropertyHandle> FaderGroupNameHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderGroup::GetFaderGroupNamePropertyName());
		InDetailLayout.HideProperty(FaderGroupNameHandle);

		FaderGroupCategory.AddProperty(FaderGroupNameHandle);

		// Fixture Patch section
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
	}

	bool FDMXControlConsoleFaderGroupDetails::IsAnyFaderGroupPatched() const
	{
		const TArray<UDMXControlConsoleFaderGroup*> SelectedFaderGroups = GetValidFaderGroupsBeingEdited();
		const bool bIsAnyFaderGroupPatched = Algo::AnyOf(SelectedFaderGroups, [](const UDMXControlConsoleFaderGroup* SelectedFaderGroup)
			{
				return SelectedFaderGroup && SelectedFaderGroup->HasFixturePatch();
			});

		return bIsAnyFaderGroupPatched;
	}

	FText FDMXControlConsoleFaderGroupDetails::GetFixturePatchText() const
	{
		if (!WeakEditorModel.IsValid())
		{
			return FText::GetEmpty();
		}

		const TArray<UDMXControlConsoleFaderGroup*> SelectedFaderGroups = GetValidFaderGroupsBeingEdited();

		if (SelectedFaderGroups.IsEmpty())
		{
			return FText::GetEmpty();
		}
		else if (SelectedFaderGroups.Num() > 1 && IsAnyFaderGroupPatched())
		{
			return FText::FromString(TEXT("Multiple Values"));
		}

		const UDMXControlConsoleFaderGroup* SelectedFaderGroup = SelectedFaderGroups[0];
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

	TArray<UDMXControlConsoleFaderGroup*> FDMXControlConsoleFaderGroupDetails::GetValidFaderGroupsBeingEdited() const
	{
		const TArray<TWeakObjectPtr<UObject>> EditedObjects = PropertyUtilities->GetSelectedObjects();
		TArray<UDMXControlConsoleFaderGroup*> Result;
		Algo::TransformIf(EditedObjects, Result,
			[](TWeakObjectPtr<UObject> Object)
			{
				return IsValid(Cast<UDMXControlConsoleFaderGroup>(Object.Get()));
			},
			[](TWeakObjectPtr<UObject> Object)
			{
				return Cast<UDMXControlConsoleFaderGroup>(Object.Get());
			}
		);

		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
