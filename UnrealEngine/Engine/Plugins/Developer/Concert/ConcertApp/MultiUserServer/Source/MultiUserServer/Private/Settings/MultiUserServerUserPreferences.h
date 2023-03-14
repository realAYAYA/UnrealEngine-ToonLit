// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MultiUserServerUserPreferences.generated.h"

/**
 * 
 */
UCLASS(Config = MultiUserServerUserSettings, DefaultConfig)
class MULTIUSERSERVER_API UMultiUserServerUserPreferences : public UObject
{
	GENERATED_BODY()
public:
	
	static UMultiUserServerUserPreferences* GetSettings();

	UPROPERTY(Config)
	bool bWarnUserAboutMuting = true;

	UPROPERTY(Config)
	bool bWarnUserAboutUnmuting = true;
};
