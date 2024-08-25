// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectCustomSettingsDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectCustomSettings.h"

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
			SNew(SCustomizableObjectCustomSettings)
				.PreviewSettings(Cast<UCustomSettings>(DetailsView->GetSelectedObjects()[0]))
		];
	}
}
