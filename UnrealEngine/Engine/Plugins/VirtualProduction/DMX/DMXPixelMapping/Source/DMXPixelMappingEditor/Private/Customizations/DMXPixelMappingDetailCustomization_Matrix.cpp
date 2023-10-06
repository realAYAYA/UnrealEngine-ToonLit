// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingDetailCustomization_Matrix.h"

#include "Components/DMXPixelMappingMatrixComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"


FDMXPixelMappingDetailCustomization_Matrix::FDMXPixelMappingDetailCustomization_Matrix(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
	: ToolkitWeakPtr(InToolkitWeakPtr)
{}

TSharedRef<IDetailCustomization> FDMXPixelMappingDetailCustomization_Matrix::MakeInstance(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
{
	return MakeShared<FDMXPixelMappingDetailCustomization_Matrix>(InToolkitWeakPtr);
}

void FDMXPixelMappingDetailCustomization_Matrix::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	// Hide the Layout Script property (shown in its own panel, see SDMXPixelMappingLayoutView)
	InDetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, LayoutScript));

	// Sort categories
	InDetailLayout.SortCategories([](const TMap<FName, IDetailCategoryBuilder*>& CategoryMap)
		{
			int32 SortOrder = 0;

			IDetailCategoryBuilder* const* FixturePatchCategoryPtr = CategoryMap.Find("Fixture Patch");
			if (FixturePatchCategoryPtr)
			{
				(*FixturePatchCategoryPtr)->SetSortOrder(SortOrder++);
			}

			IDetailCategoryBuilder* const* ColorSpaceCategoryPtr = CategoryMap.Find("Color Space");
			if (ColorSpaceCategoryPtr)
			{
				(*ColorSpaceCategoryPtr)->SetSortOrder(SortOrder++);
			}

			// Either 'RGB' 'XY' or 'XYZ' is displayed
			IDetailCategoryBuilder* const* RGBCategoryPtr = CategoryMap.Find("RGB");
			if (RGBCategoryPtr)
			{
				(*RGBCategoryPtr)->SetSortOrder(SortOrder++);
			}

			IDetailCategoryBuilder* const* XYCategoryPtr = CategoryMap.Find("XY");
			if (XYCategoryPtr)
			{
				(*XYCategoryPtr)->SetSortOrder(SortOrder++);
			}

			IDetailCategoryBuilder* const* XYZCategoryPtr = CategoryMap.Find("XYZ");
			if (XYZCategoryPtr)
			{
				(*XYZCategoryPtr)->SetSortOrder(SortOrder++);
			}

			IDetailCategoryBuilder* const* IntensityCategoryPtr = CategoryMap.Find("Luminance");
			if (IntensityCategoryPtr)
			{
				(*IntensityCategoryPtr)->SetSortOrder(SortOrder++);
			}

			IDetailCategoryBuilder* const* QualityCategoryPtr = CategoryMap.Find("Quality");
			if (QualityCategoryPtr)
			{
				(*QualityCategoryPtr)->SetSortOrder(SortOrder++);
			}

			IDetailCategoryBuilder* const* ModulatorsCategoryPtr = CategoryMap.Find("Output Modulators");
			if (ModulatorsCategoryPtr)
			{
				(*ModulatorsCategoryPtr)->SetSortOrder(SortOrder++);
			}

			IDetailCategoryBuilder* const* TransformCategoryPtr = CategoryMap.Find("Transform");
			if (TransformCategoryPtr)
			{
				(*TransformCategoryPtr)->SetSortOrder(SortOrder++);
			}

			IDetailCategoryBuilder* const* EditorSettingsCategoryPtr = CategoryMap.Find("Editor Settings");
			if (EditorSettingsCategoryPtr)
			{
				(*EditorSettingsCategoryPtr)->SetSortOrder(SortOrder++);
			}
		});
}
