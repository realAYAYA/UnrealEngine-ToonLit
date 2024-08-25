// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "AvaRCControllerId.generated.h"

class URCVirtualPropertyBase;
class URemoteControlPreset;

/** Struct describing data to identify/find a Controller in a given preset */
USTRUCT()
struct FAvaRCControllerId
{
	GENERATED_BODY()

	AVALANCHEREMOTECONTROL_API URCVirtualPropertyBase* FindController(URemoteControlPreset* InPreset) const;

	AVALANCHEREMOTECONTROL_API FText ToText() const;

	UPROPERTY(EditAnywhere, Category = "Motion Design Remote Control")
	FName Name;
};
