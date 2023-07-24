// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSpriteRendererDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Layout/SExpandableArea.h"

TSharedRef<IDetailCustomization> FNiagaraSpriteRendererDetails::MakeInstance()
{
	return MakeShared<FNiagaraSpriteRendererDetails>();
}

void FNiagaraSpriteRendererDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Categories to put first
	{
		static const FName CategoriesToCollapse[] =
		{
			FName("Sprite Rendering"),
			FName("Sorting"),
			FName("Visibility"),
		};

		for (const FName& Category : CategoriesToCollapse)
		{
			IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(Category);
		}
	}

	// Collapse default categories
	{
		static const FName CategoriesToCollapse[] =
		{
			FName("SubUV"),
			FName("Cutout"),
			FName("Bindings"),
			FName("Rendering"),
			FName("Scalability"),
		};

		for (const FName& Category : CategoriesToCollapse)
		{
			IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(Category);
			CategoryBuilder.InitiallyCollapsed(true);
		}
	}
}
