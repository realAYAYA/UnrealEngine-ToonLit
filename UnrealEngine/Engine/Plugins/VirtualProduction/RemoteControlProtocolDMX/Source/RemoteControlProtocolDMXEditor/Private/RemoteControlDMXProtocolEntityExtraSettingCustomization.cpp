// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlDMXProtocolEntityExtraSettingCustomization.h"

#include "DMXConversions.h"
#include "DetailWidgetRow.h"
#include "DMXProtocolTypes.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "RemoteControlProtocolDMX.h"
#include "RemoteControlProtocolDMXSettings.h"
#include "Library/DMXEntityFixtureType.h"

#include "Styling/AppStyle.h"
#include "IPropertyUtilities.h"
#include "IO/DMXInputPort.h"
#include "IO/DMXPortManager.h"
#include "Widgets/SDMXPortSelector.h"
#include "Widgets/Text/STextBlock.h"


TSharedRef<IPropertyTypeCustomization> FRemoteControlDMXProtocolEntityExtraSettingCustomization::MakeInstance()
{
	return MakeShared<FRemoteControlDMXProtocolEntityExtraSettingCustomization>();
}

void FRemoteControlDMXProtocolEntityExtraSettingCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructProperty, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// No need for a header widget, only structure child widgets should be present
}

void FRemoteControlDMXProtocolEntityExtraSettingCustomization::CustomizeChildren(const TSharedRef<IPropertyHandle> InStructProperty, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	// Remember relevant property handles
	InputPortIdHandle = InStructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRemoteControlDMXProtocolEntityExtraSetting, InputPortId));
	UseDefaultInputPortHandle = InStructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRemoteControlDMXProtocolEntityExtraSetting, bUseDefaultInputPort));
	FixtureSignalFormatHandle = InStructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRemoteControlDMXProtocolEntityExtraSetting, DataType));
	StartingChannelPropertyHandle = InStructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRemoteControlDMXProtocolEntityExtraSetting, StartingChannel));

	// Handle project setting changes
	URemoteControlProtocolDMXSettings* ProtocolDMXSettings = GetMutableDefault<URemoteControlProtocolDMXSettings>();
	ProtocolDMXSettings->GetOnRemoteControlProtocolDMXSettingsChanged().AddSP(this, &FRemoteControlDMXProtocolEntityExtraSettingCustomization::OnInputPortChanged);

	// Handle property changes
	if (!ensure(UseDefaultInputPortHandle.IsValid()))
	{
		return;
	}
	UseDefaultInputPortHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRemoteControlDMXProtocolEntityExtraSettingCustomization::OnInputPortChanged));

	if (!ensure(FixtureSignalFormatHandle.IsValid()))
	{
		return;
	}
	FixtureSignalFormatHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRemoteControlDMXProtocolEntityExtraSettingCustomization::OnFixtureSignalFormatChange));

	if (!ensure(StartingChannelPropertyHandle.IsValid()))
	{
		return;
	}
	StartingChannelPropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRemoteControlDMXProtocolEntityExtraSettingCustomization::OnStartingChannelChange));

	// Add all properties besides the InputPortId
	uint32 NumChildren = 0;
	InStructProperty->GetNumChildren(NumChildren);
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
	{
		const TSharedPtr<IPropertyHandle> ChildHandle = InStructProperty->GetChildHandle(ChildIndex);
		const FName PropertyName = ChildHandle->GetProperty() ? ChildHandle->GetProperty()->GetFName() : NAME_None;

		if (PropertyName == GET_MEMBER_NAME_CHECKED(FRemoteControlDMXProtocolEntityExtraSetting, InputPortId))
		{
			continue;
		}

		ChildBuilder.AddProperty(ChildHandle.ToSharedRef());

	}

	// Show a port selector instead of the InputPortId
	const FGuid PortGuid = [ProtocolDMXSettings, this]()
	{
		FGuid Result = GetPortGuid();
		if (!Result.IsValid())
		{
			Result = ProtocolDMXSettings->GetOrCreateDefaultInputPortId();
		}

		return Result;
	}();

	TAttribute<bool> PortSelectorEnabledAttribute = TAttribute<bool>::Create(
		TAttribute<bool>::FGetter::CreateSP(this, &FRemoteControlDMXProtocolEntityExtraSettingCustomization::GetIsPortSelectorEnabled)
	);

	if(PortGuid.IsValid())
	{
		ChildBuilder.AddCustomRow(NSLOCTEXT("RemoteControlProtocolDMXEditorTypeCustomization", "PortSearchString", "Port"))
		.IsEnabled(PortSelectorEnabledAttribute)
		.NameContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("RemoteControlProtocolDMXEditorTypeCustomization", "PortLabel", "Input Port"))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		.ValueContent()
			[
				SAssignNew(PortSelector, SDMXPortSelector)
				.Mode(EDMXPortSelectorMode::SelectFromAvailableInputs)
				.InitialSelection(PortGuid)
				.OnPortSelected(this, &FRemoteControlDMXProtocolEntityExtraSettingCustomization::OnPortSelected)
			];
	}
	else
	{
		ChildBuilder.AddCustomRow(NSLOCTEXT("RemoteControlProtocolDMXEditorTypeCustomization", "NoPortAvailableSearchString", "Port"))
			.WholeRowContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("RemoteControlProtocolDMXEditorTypeCustomization", "NoPortAvailableText", "No ports available, please create an Input Port in Project Settings -> Plugins -> DMX"))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			];
	}
}

void FRemoteControlDMXProtocolEntityExtraSettingCustomization::OnInputPortChanged()
{
	bool bUseDefaultInputPort;
	if (UseDefaultInputPortHandle->GetValue(bUseDefaultInputPort) == FPropertyAccess::Success)
	{
		if (bUseDefaultInputPort)
		{
			URemoteControlProtocolDMXSettings* ProtocolDMXSettings = GetMutableDefault<URemoteControlProtocolDMXSettings>();
			FGuid PortGuid = ProtocolDMXSettings->GetOrCreateDefaultInputPortId();

			SetPortGuid(PortGuid);
		}
	}

	PropertyUtilities->ForceRefresh();
}

void FRemoteControlDMXProtocolEntityExtraSettingCustomization::OnFixtureSignalFormatChange()
{
	CheckAndApplyStartingChannelValue();
}

void FRemoteControlDMXProtocolEntityExtraSettingCustomization::OnStartingChannelChange()
{
	CheckAndApplyStartingChannelValue();
}

void FRemoteControlDMXProtocolEntityExtraSettingCustomization::OnPortSelected()
{
	FDMXInputPortSharedPtr InputPort = PortSelector->GetSelectedInputPort();
	FGuid NewPortGuid = InputPort->GetPortGuid();

	SetPortGuid(NewPortGuid);
}

void FRemoteControlDMXProtocolEntityExtraSettingCustomization::CheckAndApplyStartingChannelValue()
{
	// Define Number channels to occupy
	uint8 DataTypeByte = 0;
	FPropertyAccess::Result Result = FixtureSignalFormatHandle->GetValue(DataTypeByte);
	if(!ensure(Result == FPropertyAccess::Success))
	{
		return;
	}
	const EDMXFixtureSignalFormat DataType = static_cast<EDMXFixtureSignalFormat>(DataTypeByte);
	const uint8 NumChannelsToOccupy = FDMXConversions::GetSizeOfSignalFormat(DataType);

	// Get starting channel value
	int32 StartingChannel = 0;
	Result = StartingChannelPropertyHandle->GetValue(StartingChannel);
	if(!ensure(Result == FPropertyAccess::Success))
	{
		return;
	}

	// Calculate channel overhead and apply if StartingChannel plus num channels to occupy doesn't fit into the Universe
	const int32 ChannelOverhead = StartingChannel + NumChannelsToOccupy - DMX_UNIVERSE_SIZE;
	if (ChannelOverhead > 0)
	{
		const int32 MaxStartingChannel = StartingChannel - ChannelOverhead;
		Result = StartingChannelPropertyHandle->SetValue(MaxStartingChannel);
		if(!ensure(Result == FPropertyAccess::Success))
		{
			return;
		}
	}
}

FGuid FRemoteControlDMXProtocolEntityExtraSettingCustomization::GetPortGuid() const
{
	if (InputPortIdHandle.IsValid())
	{
		TArray<void*> RawDatas;
		InputPortIdHandle->AccessRawData(RawDatas);

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

void FRemoteControlDMXProtocolEntityExtraSettingCustomization::SetPortGuid(const FGuid& PortGuid)
{
	if (InputPortIdHandle.IsValid() && PortGuid.IsValid())
	{
		TArray<void*> RawDatas;
		InputPortIdHandle->AccessRawData(RawDatas);

		for (void* RawData : RawDatas)
		{
			*(FGuid*)RawData = PortGuid;
		}
	}
}

bool FRemoteControlDMXProtocolEntityExtraSettingCustomization::GetIsPortSelectorEnabled()
{
	bool bUseDefaultInputPort;
	if (UseDefaultInputPortHandle->GetValue(bUseDefaultInputPort) == FPropertyAccess::Success)
	{
		if (!bUseDefaultInputPort)
		{
			return true;
		}
	}

	return false;
}