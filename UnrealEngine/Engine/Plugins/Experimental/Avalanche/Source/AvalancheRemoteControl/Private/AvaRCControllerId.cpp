// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRCControllerId.h"
#include "RemoteControlPreset.h"

URCVirtualPropertyBase* FAvaRCControllerId::FindController(URemoteControlPreset* InPreset) const
{
	if (InPreset)
	{
		return InPreset->GetControllerByDisplayName(Name);
	}
	return nullptr;
}

FText FAvaRCControllerId::ToText() const
{
	return FText::FromName(Name);
}
