// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlProtocolDMXSettingsDetails.h"

#include "IRemoteControlPropertyHandle.h"
#include "RemoteControlProtocolDMXSettings.h"
#include "RemoteControlPreset.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Styling/AppStyle.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "IPropertyUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IO/DMXInputPort.h"
#include "IO/DMXPortManager.h"
#include "Misc/MessageDialog.h"
#include "Widgets/SDMXPortSelector.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RemoteControlProtocolDMXSettingsDetails"

TSharedRef<IDetailCustomization> FRemoteControlProtocolDMXSettingsDetails::MakeInstance()
{
	return MakeShared<FRemoteControlProtocolDMXSettingsDetails>();
}

void FRemoteControlProtocolDMXSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	// Customize the InputPortId property view
	DefaultInputPortIdHandle = DetailBuilder.GetProperty(URemoteControlProtocolDMXSettings::GetDefaultInputPortIdPropertyNameChecked());
	if (!ensure(DefaultInputPortIdHandle.IsValid()))
	{
		return;
	}
	DefaultInputPortIdHandle->MarkHiddenByCustomization();

	FGuid PortGuid = GetPortGuid();
	if (!PortGuid.IsValid() || !FDMXPortManager::Get().FindInputPortByGuid(PortGuid))
	{
		const TArray<FDMXInputPortSharedRef>& AvailableInputPorts = FDMXPortManager::Get().GetInputPorts();
		if (AvailableInputPorts.Num() > 0)
		{
			PortGuid = AvailableInputPorts[0]->GetPortGuid();
		}
		else
		{
			DetailBuilder.EditCategory(FName("DMX"))
				.AddCustomRow(LOCTEXT("NoInputPortAvailableSearchString", "Port"))
				.WholeRowContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoInputPortAvailableText", "No input port available. Please create one under Project Settings -> Plugins -> DMX"))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				];
			return;
		}
	}

	DetailBuilder.EditCategory(FName("DMX"))
		.AddCustomRow(LOCTEXT("DefaultPortSearchString", "Port"))
		.NameContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DefaultPortLabel", "Default Input Port"))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		.ValueContent()
			[
				SAssignNew(PortSelector, SDMXPortSelector)
				.Mode(EDMXPortSelectorMode::SelectFromAvailableInputs)
				.InitialSelection(PortGuid)
				.OnPortSelected(this, &FRemoteControlProtocolDMXSettingsDetails::OnPortSelected)
			];
}

void FRemoteControlProtocolDMXSettingsDetails::OnPortSelected()
{
	if (FDMXInputPortSharedPtr InputPort = PortSelector->GetSelectedInputPort())
	{
		const FGuid NewPortGuid = InputPort->GetPortGuid();

		SetPortGuid(NewPortGuid);
	}
	else
	{
		PropertyUtilities->ForceRefresh();
	}
}

FGuid FRemoteControlProtocolDMXSettingsDetails::GetPortGuid() const
{
	if (DefaultInputPortIdHandle.IsValid())
	{
		TArray<void*> RawDatas;
		DefaultInputPortIdHandle->AccessRawData(RawDatas);

		for (void* RawData : RawDatas)
		{
			const FGuid* PortGuidPtr = reinterpret_cast<FGuid*>(RawData);
			if (PortGuidPtr && PortGuidPtr->IsValid())
			{
				return *PortGuidPtr;
			}
		}
	}

	return FGuid();
}

void FRemoteControlProtocolDMXSettingsDetails::SetPortGuid(const FGuid& PortGuid)
{
	if (DefaultInputPortIdHandle.IsValid() && PortGuid.IsValid())
	{
		DefaultInputPortIdHandle->NotifyPreChange();

		TArray<void*> RawDatas;
		DefaultInputPortIdHandle->AccessRawData(RawDatas);

		for (void* RawData : RawDatas)
		{
			*(FGuid*)RawData = PortGuid;
		}

		DefaultInputPortIdHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

#undef LOCTEXT_NAMESPACE
