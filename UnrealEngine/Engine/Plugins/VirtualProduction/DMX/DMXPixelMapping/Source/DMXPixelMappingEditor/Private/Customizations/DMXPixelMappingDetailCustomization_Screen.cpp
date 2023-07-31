// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingDetailCustomization_Screen.h"
#include "Components/DMXPixelMappingScreenComponent.h"
#include "DMXEditorStyle.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "DetailWidgetRow.h"
#include "Layout/Visibility.h"

#define LOCTEXT_NAMESPACE "DMXPixelMappingDetailCustomization_Screen"

void FDMXPixelMappingDetailCustomization_Screen::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	DetailLayout.EditCategory("Patch Settings", FText::GetEmpty(), ECategoryPriority::Important);
}

#undef LOCTEXT_NAMESPACE
