// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailGroup.h"
#include "DetailWidgetRow.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Generates the header widget for a customized struct or other type.
 * This widget is generally used in the property editor to display a struct as a single row, like with an FColor.
 * Properties passed in that do not have a header customization will return a null widget.
 */
class PROPERTYEDITOR_API SStandaloneCustomizedValueWidget : public SCompoundWidget, public IPropertyTypeCustomizationUtils
{
public:
	SLATE_BEGIN_ARGS( SStandaloneCustomizedValueWidget )
	{}
		/** Optional Parent Detail Category, useful to access Thumbnail Pool. */
		SLATE_ARGUMENT( TSharedPtr<FDetailCategoryImpl>, ParentCategory)
	SLATE_END_ARGS()
	
	void Construct( const FArguments& InArgs,
		TSharedPtr<IPropertyTypeCustomization> InCustomizationInterface, TSharedRef<IPropertyHandle> InPropertyHandle)
	{
		ParentCategory = InArgs._ParentCategory;
		
		CustomizationInterface = InCustomizationInterface;
		PropertyHandle = InPropertyHandle;
		CustomPropertyWidget = MakeShareable(new FDetailWidgetRow);

		CustomizationInterface->CustomizeHeader(InPropertyHandle, *CustomPropertyWidget, *this);

		ChildSlot
		[
			CustomPropertyWidget->ValueWidget.Widget
		];
	}

	virtual TSharedPtr<FAssetThumbnailPool> GetThumbnailPool() const override
	{
		TSharedPtr<FDetailCategoryImpl> ParentCategoryPinned = ParentCategory.Pin();
		return ParentCategoryPinned.IsValid() ? ParentCategoryPinned->GetParentLayout().GetThumbnailPool() : NULL;
	}

private:
	TWeakPtr<FDetailCategoryImpl> ParentCategory;
	TSharedPtr<IPropertyTypeCustomization> CustomizationInterface;
	TSharedPtr<IPropertyHandle> PropertyHandle;
	TSharedPtr<FDetailWidgetRow> CustomPropertyWidget;
};
