// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingDetailCustomization_Screen.h"

#include "Components/DMXPixelMappingOutputDMXComponent.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "DMXPixelMappingDetailCustomization_Screen"

void FDMXPixelMappingDetailCustomization_Screen::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	// Hide pointless properties of the common base class with fixture group item and matrix component while retaining support for this, 5.3 Deprecated, component
	DetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingOutputDMXComponent, FixturePatchRef));
	DetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingOutputDMXComponent, ColorSpaceClass));
	DetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingOutputDMXComponent, ColorSpace));
	DetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingOutputDMXComponent, ModulatorClasses));
}

#undef LOCTEXT_NAMESPACE
