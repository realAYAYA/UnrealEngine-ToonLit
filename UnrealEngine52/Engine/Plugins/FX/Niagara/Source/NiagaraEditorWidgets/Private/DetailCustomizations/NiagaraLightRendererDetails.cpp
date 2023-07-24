// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraLightRendererDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Layout/SExpandableArea.h"

TSharedRef<IDetailCustomization> FNiagaraLightRendererDetails::MakeInstance()
{
	return MakeShared<FNiagaraLightRendererDetails>();
}

void FNiagaraLightRendererDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Categories to put first
	{
		static const FName CategoriesToCollapse[] =
		{
			FName("Light Rendering"),
			FName("Bindings"),
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
