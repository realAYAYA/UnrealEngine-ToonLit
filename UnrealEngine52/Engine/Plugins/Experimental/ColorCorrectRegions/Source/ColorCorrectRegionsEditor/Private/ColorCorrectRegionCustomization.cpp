// Copyright Epic Games, Inc. All Rights Reserved.

#include "ColorCorrectRegionCustomization.h"
#include "ColorCorrectRegion.h"
#include "ColorCorrectWindow.h"
#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FColorCorrectWindowDetails::MakeInstance()
{
	return MakeShareable(new FColorCorrectWindowDetails);
}

void FColorCorrectWindowDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	for (const TWeakObjectPtr<UObject>& SelectedObject : DetailBuilder.GetSelectedObjects())
	{
		if (AColorCorrectionWindow* CCW = Cast<AColorCorrectionWindow>(SelectedObject.Get()))
		{
			TSharedRef<IPropertyHandle> PriorityProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(AColorCorrectRegion, Priority));
			DetailBuilder.HideProperty(PriorityProperty);

			TSharedRef<IPropertyHandle> TypeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(AColorCorrectRegion, Type));
			DetailBuilder.HideProperty(TypeProperty);
		}
	}
}
