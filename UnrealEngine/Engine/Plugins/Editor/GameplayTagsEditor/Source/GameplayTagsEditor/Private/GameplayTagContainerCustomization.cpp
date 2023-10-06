// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagContainerCustomization.h"
#include "DetailWidgetRow.h"
#include "GameplayTagsEditorModule.h"
#include "GameplayTagsManager.h"
#include "Widgets/Input/SHyperlink.h"
#include "SGameplayTagContainerCombo.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ScopedTransaction.h"
#include "GameplayTagEditorUtilities.h"
#include "SGameplayTagPicker.h"

#define LOCTEXT_NAMESPACE "GameplayTagContainerCustomization"

TSharedRef<IPropertyTypeCustomization> FGameplayTagContainerCustomizationPublic::MakeInstance()
{
	return MakeShareable(new FGameplayTagContainerCustomization());
}

// Deprecated version.
TSharedRef<IPropertyTypeCustomization> FGameplayTagContainerCustomizationPublic::MakeInstanceWithOptions(const FGameplayTagContainerCustomizationOptions& Options)
{
	return MakeShareable(new FGameplayTagContainerCustomization());
}

FGameplayTagContainerCustomization::FGameplayTagContainerCustomization()
{
}

void FGameplayTagContainerCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.Padding(FMargin(0,2,0,1))
			[
				SNew(SGameplayTagContainerCombo)
				.PropertyHandle(StructPropertyHandle)
			]
		]
	.PasteAction(FUIAction(
	FExecuteAction::CreateSP(this, &FGameplayTagContainerCustomization::OnPasteTag),
		FCanExecuteAction::CreateSP(this, &FGameplayTagContainerCustomization::CanPasteTag)));
}

void FGameplayTagContainerCustomization::OnPasteTag() const
{
	if (!StructPropertyHandle.IsValid())
	{
		return;
	}
	
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);
	bool bHandled = false;

	// Try to paste single tag
	const FGameplayTag PastedTag = UE::GameplayTags::EditorUtilities::GameplayTagTryImportText(PastedText);
	if (PastedTag.IsValid())
	{
		TArray<FString> NewValues;
		SGameplayTagPicker::EnumerateEditableTagContainersFromPropertyHandle(StructPropertyHandle.ToSharedRef(), [&NewValues, PastedTag](const FGameplayTagContainer& EditableTagContainer)
		{
			FGameplayTagContainer TagContainerCopy = EditableTagContainer;
			TagContainerCopy.AddTag(PastedTag);

			NewValues.Add(TagContainerCopy.ToString());
			return true;
		});

		FScopedTransaction Transaction(LOCTEXT("GameplayTagContainerCustomization_PasteTag", "Paste Gameplay Tag"));
		StructPropertyHandle->SetPerObjectValues(NewValues);
		bHandled = true;
	}

	// Try to paste a container
	if (!bHandled)
	{
		const FGameplayTagContainer PastedTagContainer = UE::GameplayTags::EditorUtilities::GameplayTagContainerTryImportText(PastedText);
		if (PastedTagContainer.IsValid())
		{
			// From property
			FScopedTransaction Transaction(LOCTEXT("GameplayTagContainerCustomization_PasteTagContainer", "Paste Gameplay Tag Container"));
			StructPropertyHandle->SetValueFromFormattedString(PastedText);
			bHandled = true;
		}
	}
}

bool FGameplayTagContainerCustomization::CanPasteTag() const
{
	if (!StructPropertyHandle.IsValid())
	{
		return false;
	}

	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);
	
	const FGameplayTag PastedTag = UE::GameplayTags::EditorUtilities::GameplayTagTryImportText(PastedText);
	if (PastedTag.IsValid())
	{
		return true;
	}

	const FGameplayTagContainer PastedTagContainer = UE::GameplayTags::EditorUtilities::GameplayTagContainerTryImportText(PastedText);
	if (PastedTagContainer.IsValid())
	{
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
