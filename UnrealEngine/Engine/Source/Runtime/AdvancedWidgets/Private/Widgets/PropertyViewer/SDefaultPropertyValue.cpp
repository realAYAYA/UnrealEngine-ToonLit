// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/PropertyViewer/SDefaultPropertyValue.h"

#include "Styling/SlateTypes.h"
#include "Styling/AppStyle.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

#include "Widgets/Text/STextBlock.h"

namespace UE::PropertyViewer
{

TSharedPtr<SWidget> SDefaultPropertyValue::CreateInstance(const FPropertyValueFactory::FGenerateArgs Args)
{
	return SNew(SDefaultPropertyValue)
		.Path(Args.Path)
		.IsEnabled(Args.bCanEditValue);
}


void SDefaultPropertyValue::Construct(const FArguments& InArgs)
{
	Path = InArgs._Path;

	const FProperty* Property = Path.GetLastProperty();
	ChildSlot
	[
		SNew(STextBlock)
		.Text(this, &SDefaultPropertyValue::GetText)
 	];
}


FText SDefaultPropertyValue::GetText() const
{
	if (const FProperty* LastProperty = Path.GetLastProperty())
	{
		if (const void* ContainerPtr = Path.GetContainerPtr())
		{
			FString TextVersion;
			LastProperty->ExportTextItem_InContainer(TextVersion, ContainerPtr, nullptr, nullptr, PPF_SimpleObjectText);
			return FText::FromString(TextVersion);
		}
	}
	return FText::GetEmpty();
}

} //namespace
