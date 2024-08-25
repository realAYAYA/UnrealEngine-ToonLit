// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLayer/DataLayerPropertyTypeCustomizationHelper.h"

#include "DataLayerMode.h"
#include "Delegates/Delegate.h"

class SWidget;

#define LOCTEXT_NAMESPACE "DataLayer"

TSharedRef<SWidget> FDataLayerPropertyTypeCustomizationHelper::CreateDataLayerMenu(TFunction<void(const UDataLayerInstance* DataLayer)> OnDataLayerSelectedFunction)
{
	return FDataLayerPickingMode::CreateDataLayerPickerWidget(FOnDataLayerInstancePicked::CreateLambda([OnDataLayerSelectedFunction](UDataLayerInstance* TargetDataLayer)
	{
		OnDataLayerSelectedFunction(TargetDataLayer);
	}));
}

#undef LOCTEXT_NAMESPACE