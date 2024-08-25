// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsPanel/DMMaterialInterfaceTypeCustomizer.h"
#include "DetailWidgetRow.h"
#include "DetailsPanel/Slate/SDMDetailsPanelMaterialInterfaceWidget.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"

TSharedRef<IPropertyTypeCustomization> FDMMaterialInterfaceTypeCustomizer::MakeInstance()
{
	return MakeShared<FDMMaterialInterfaceTypeCustomizer>();
}

void FDMMaterialInterfaceTypeCustomizer::CustomizeHeader(TSharedRef<IPropertyHandle> EditorPropertyHandle, class FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()
	[
		EditorPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.VAlign(VAlign_Center)
	[
		SNew(SDMDetailsPanelMaterialInterfaceWidget, EditorPropertyHandle)
		.ThumbnailPool(CustomizationUtils.GetThumbnailPool())
	];
}

void FDMMaterialInterfaceTypeCustomizer::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

FDMMaterialInterfaceTypeCustomizer::FDMMaterialInterfaceTypeCustomizer()
{
}
