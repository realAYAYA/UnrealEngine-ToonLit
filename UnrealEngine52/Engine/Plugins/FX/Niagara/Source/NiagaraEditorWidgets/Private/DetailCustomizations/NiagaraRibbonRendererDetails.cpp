// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraRibbonRendererDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Layout/SExpandableArea.h"

TSharedRef<IDetailCustomization> FNiagaraRibbonRendererDetails::MakeInstance()
{
	return MakeShared<FNiagaraRibbonRendererDetails>();
}

void FNiagaraRibbonRendererDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Categories to put first
	{
		static const FName CategoriesToCollapse[] =
		{
			FName("Ribbon Rendering"),
			FName("Ribbon Shape"),
			FName("Ribbon Tessellation"),
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
