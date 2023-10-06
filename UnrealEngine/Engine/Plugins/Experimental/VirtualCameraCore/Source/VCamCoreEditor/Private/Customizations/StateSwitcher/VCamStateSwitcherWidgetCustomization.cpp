// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamStateSwitcherWidgetCustomization.h"

#include "UI/Switcher/VCamStateSwitcherWidget.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Util/SharedPropertyCustomizationUtils.h"

namespace UE::VCamCoreEditor::Private
{
	TSharedRef<IDetailCustomization> FVCamStateSwitcherWidgetCustomization::MakeInstance()
	{
		return MakeShared<FVCamStateSwitcherWidgetCustomization>();
	}

	void FVCamStateSwitcherWidgetCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		CustomizeCurrentState(DetailBuilder);
	}
	
	void FVCamStateSwitcherWidgetCustomization::CustomizeCurrentState(IDetailLayoutBuilder& DetailBuilder)
	{
		const TSharedRef<IPropertyHandle> CurrentStatePropertyHandle = DetailBuilder.GetProperty(UVCamStateSwitcherWidget::GetCurrentStatePropertyName());
		TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
		DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);
		UVCamStateSwitcherWidget* StateSwitcher = CustomizedObjects.Num() == 1
			? Cast<UVCamStateSwitcherWidget>(CustomizedObjects[0])
			: nullptr;
		if (!StateSwitcher)
		{
			return;
		}
		
		IDetailPropertyRow* PropertyRow = DetailBuilder.EditDefaultProperty(CurrentStatePropertyHandle);
		if (ensure(PropertyRow))
		{
			StateSwitcher::CustomizeCurrentState(*StateSwitcher, *PropertyRow, CurrentStatePropertyHandle, DetailBuilder.GetDetailFont());
		}
	}
}