// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkSourceSettings.h"

#include "LiveLinkMessageBusSourceSettings.generated.h"




/**
 * Settings for LiveLinkMessageBusSource.
 * Used to apply default Evaluation mode from project settings when constructed
 */
UCLASS()
class LIVELINK_API ULiveLinkMessageBusSourceSettings : public ULiveLinkSourceSettings
{
public:
	GENERATED_BODY()

public:
	ULiveLinkMessageBusSourceSettings();
};
