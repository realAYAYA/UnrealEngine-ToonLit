// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaDetailsCustomization.h"

#include "Customizations/RivermaxDeviceSelectionCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "RivermaxMediaOutput.h"
#include "RivermaxMediaSource.h"

TSharedRef<IDetailCustomization> FRivermaxMediaDetailsCustomization::MakeInstance()
{
	return MakeShared<FRivermaxMediaDetailsCustomization>();
}

void FRivermaxMediaDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);

	for (int32 ObjectIndex = 0; ObjectIndex < CustomizedObjects.Num(); ++ObjectIndex)
	{
		// For now, take care of both Rivermax source and output to customize their interface field
		const TWeakObjectPtr<UObject> Obj = CustomizedObjects[ObjectIndex];
		if (URivermaxMediaOutput* Output = Cast<URivermaxMediaOutput>(Obj.Get()))
		{
			UE::RivermaxCore::Utils::SetupDeviceSelectionCustomization(ObjectIndex, Output->InterfaceAddress, DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(URivermaxMediaOutput, InterfaceAddress)), DetailBuilder);
		}
		else if (URivermaxMediaSource* Source = Cast<URivermaxMediaSource>(Obj.Get()))
		{
			UE::RivermaxCore::Utils::SetupDeviceSelectionCustomization(ObjectIndex, Source->InterfaceAddress, DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(URivermaxMediaSource, InterfaceAddress)), DetailBuilder);
		}
	}
}
