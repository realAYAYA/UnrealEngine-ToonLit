// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkMessageBusSourceSettings.h"

#include "LiveLinkSettings.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LiveLinkMessageBusSourceSettings)

ULiveLinkMessageBusSourceSettings::ULiveLinkMessageBusSourceSettings()
{
	Mode = GetDefault<ULiveLinkSettings>()->DefaultMessageBusSourceMode;
}

