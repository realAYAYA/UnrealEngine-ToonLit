// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CustomizableObjectSettings.generated.h"


/** CustomizableObject module settings.
  *
  * These settings can also be changed using command line arguments. For example: 
  * -ini:Engine:[/Script/CustomizableObject.CustomizableObjectSettings]:bEnableStreamingManager=true */
UCLASS(Config = Engine)
class UCustomizableObjectSettings : public UObject
{
	GENERATED_BODY()

public:
	/** If true, use the new StreamManager ticker. If false, use the old FTSTicker. */
	UPROPERTY(Config)
	bool bEnableStreamingManager = true;
};
