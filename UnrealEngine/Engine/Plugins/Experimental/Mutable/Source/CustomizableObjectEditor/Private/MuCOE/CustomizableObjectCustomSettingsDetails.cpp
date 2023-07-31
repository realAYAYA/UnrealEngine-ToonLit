// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectCustomSettingsDetails.h"

#include "Containers/Array.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "Internationalization/Text.h"
#include "MuCOE/CustomizableObjectCustomSettings.h"
#include "Templates/Casts.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

TSharedRef<IDetailCustomization> FCustomizableObjectCustomSettingsDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectCustomSettingsDetails);
}

void FCustomizableObjectCustomSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num() > 0 && DetailsView->GetSelectedObjects()[0].IsValid())
	{
		IDetailCategoryBuilder& MainCategory = DetailBuilder.EditCategory("Custom Settings");
		MainCategory.AddCustomRow(FText::FromString("Custom Settings"))
		[
			SNew(SCustomizableObjectCustomSettings, Cast<UCustomizableObjectEmptyClassForSettings>(DetailsView->GetSelectedObjects()[0].Get()))
		];
	}
}
