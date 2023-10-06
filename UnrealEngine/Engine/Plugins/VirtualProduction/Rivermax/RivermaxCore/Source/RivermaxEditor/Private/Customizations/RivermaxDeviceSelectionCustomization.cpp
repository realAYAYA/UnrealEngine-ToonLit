// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/RivermaxDeviceSelectionCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/SRivermaxInterfaceComboBox.h"

namespace UE::RivermaxCore::Utils
{

void SetupDeviceSelectionCustomization(int32 InObjectIndex, const FString& InitialValue, TSharedPtr<IPropertyHandle> PropertyHandle, IDetailLayoutBuilder& DetailBuilder)
{
	IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(PropertyHandle);

	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SWidget> ValueWidget;
	Row->GetDefaultWidgets(NameWidget, ValueWidget);
	
	Row->CustomWidget()
		.NameContent()
		[
			NameWidget->AsShared()
		]
		.ValueContent()
		[
			SNew(SRivermaxInterfaceComboBox)
			.InitialValue(InitialValue)
			.OnIPAddressSelected_Lambda([InObjectIndex, PropertyHandle](const FString& SelectedIp)
			{
				PropertyHandle->SetPerObjectValue(InObjectIndex, SelectedIp);
			})
		];
}

}
