// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingDetailCustomization_Screen.h"

#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "DMXPixelMappingDetailCustomization_Screen"

void FDMXPixelMappingDetailCustomization_Screen::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	DetailLayout.EditCategory("Patch Settings", FText::GetEmpty(), ECategoryPriority::Important);
}

#undef LOCTEXT_NAMESPACE
