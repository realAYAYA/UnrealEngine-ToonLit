// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingDetailCustomization_MatrixCell.h"

#include "DMXPixelMappingTypes.h"
#include "Components/DMXPixelMappingMatrixCellComponent.h"
#include "Toolkits/DMXPixelMappingToolkit.h"

#include "DetailLayoutBuilder.h"

void FDMXPixelMappingDetailCustomization_MatrixCell::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	// Hide absolute postition property handles
	TSharedPtr<IPropertyHandle> PositionXPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetPositionXPropertyName());
	TSharedPtr<IPropertyHandle> PositionYPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetPositionXPropertyName());
	InDetailLayout.HideProperty(PositionXPropertyHandle);

	// Hide size property handles
	TSharedPtr<IPropertyHandle> SizeXPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetSizeXPropertyName());
	TSharedPtr<IPropertyHandle> SizeYPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetSizeYPropertyName());
	InDetailLayout.HideProperty(SizeXPropertyHandle);
}
