// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaDisplayRateCustomization.h"
#include "DetailWidgetRow.h"
#include "Widgets/SAvaDisplayRate.h"

void FAvaDisplayRateCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle
	, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	InHeaderRow
		.NameContent()
		[
			InPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SAvaDisplayRate, InPropertyHandle)
		];
}
