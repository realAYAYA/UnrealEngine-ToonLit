// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsNameWidgetOverrideCustomization.h"
#include "PropertyPath.h"
#include "Widgets/SWidget.h"

FDetailsNameWidgetOverrideCustomization::FDetailsNameWidgetOverrideCustomization()
{
}

TSharedRef<SWidget> FDetailsNameWidgetOverrideCustomization::CustomizeName(TSharedRef<SWidget> InnerNameContent, FPropertyPath& Path)
{
	return InnerNameContent;
}
