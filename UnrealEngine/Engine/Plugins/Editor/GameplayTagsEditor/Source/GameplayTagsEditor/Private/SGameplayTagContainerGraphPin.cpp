// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGameplayTagContainerGraphPin.h"
#include "GameplayTagPinUtilities.h"

#define LOCTEXT_NAMESPACE "GameplayTagGraphPin"

void SGameplayTagContainerGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGameplayTagGraphPin::Construct(SGameplayTagGraphPin::FArguments(), InGraphPinObj);
}

void SGameplayTagContainerGraphPin::ParseDefaultValueData()
{
	FilterString = GameplayTagPinUtilities::ExtractTagFilterStringFromGraphPin(GraphPinObj);

	// Read using import text, but with serialize flag set so it doesn't always throw away invalid ones
	TagContainer->FromExportString(GraphPinObj->GetDefaultAsString(), PPF_SerializedAsImportText);
}

TSharedRef<SWidget> SGameplayTagContainerGraphPin::GetEditContent()
{
	EditableContainers.Empty();
	EditableContainers.Add( SGameplayTagWidget::FEditableGameplayTagContainerDatum( GraphPinObj->GetOwningNode(), TagContainer.Get() ) );

	return SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight( 400 )
		[
			SNew( SGameplayTagWidget, EditableContainers )
			.OnTagChanged(this, &SGameplayTagContainerGraphPin::SaveDefaultValueData)
			.TagContainerName( TEXT("SGameplayTagContainerGraphPin") )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.MultiSelect(true)
			.Filter(FilterString)
		];
}

void SGameplayTagContainerGraphPin::SaveDefaultValueData()
{	
	RefreshCachedData();

	// Set Pin Data
	FString TagContainerString = TagContainer->ToString();
	FString CurrentDefaultValue = GraphPinObj->GetDefaultAsString();
	if (CurrentDefaultValue.IsEmpty())
	{
		CurrentDefaultValue = FString(TEXT("(GameplayTags=)"));
	}
	if (!CurrentDefaultValue.Equals(TagContainerString))
	{
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagContainerString);
	}
}

#undef LOCTEXT_NAMESPACE
