// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlProtocolDMXSettings.h"

#include "IO/DMXInputPort.h"
#include "IO/DMXPortManager.h"

FSimpleMulticastDelegate URemoteControlProtocolDMXSettings::OnRemoteControlProtocolDMXSettingsChangedDelegate;

#if WITH_EDITOR
void URemoteControlProtocolDMXSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		OnRemoteControlProtocolDMXSettingsChangedDelegate.Broadcast();
	}
}
#endif // WITH_EDITOR

FGuid URemoteControlProtocolDMXSettings::GetOrCreateDefaultInputPortId()
{
	if (!DefaultInputPortId.IsValid() || !FDMXPortManager::Get().FindInputPortByGuid(DefaultInputPortId))
	{
		const TArray<FDMXInputPortSharedRef> InputPorts = FDMXPortManager::Get().GetInputPorts();
		if (InputPorts.Num() > 0)
		{
#if WITH_EDITOR
			Modify();
			PreEditChange(URemoteControlProtocolDMXSettings::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URemoteControlProtocolDMXSettings, DefaultInputPortId)));
#endif 

			DefaultInputPortId = InputPorts[0]->GetPortGuid();

#if WITH_EDITOR
			PostEditChange();
#endif
		}
	}

	return DefaultInputPortId;
}

FSimpleMulticastDelegate& URemoteControlProtocolDMXSettings::GetOnRemoteControlProtocolDMXSettingsChanged()
{
	return OnRemoteControlProtocolDMXSettingsChangedDelegate;
}
