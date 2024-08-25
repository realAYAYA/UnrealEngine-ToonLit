// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMStageEditDetailsCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "DMStageEditDetailsCustomization"

TSharedRef<IPropertyTypeCustomization> FDMStageEditDetailsCustomization::MakeInstance()
{
	return MakeShared<FDMStageEditDetailsCustomization>();
}

void FDMStageEditDetailsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle
	, FDetailWidgetRow& HeaderRow
	, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
}

#undef LOCTEXT_NAMESPACE
