// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once 

/*------------------------------------------------------------------------------------
IAndroidTargetPlatformSettingsModule interface
------------------------------------------------------------------------------------*/

#include "CoreMinimal.h"
#include "Interfaces/ITargetPlatformSettingsModule.h"
#include "AndroidTargetPlatformSettings.h"

class IAndroidTargetPlatformSettingsModule : public ITargetPlatformSettingsModule
{
public:
	virtual void GetSinglePlatformSettings(TMap<EAndroidTextureFormatCategory, ITargetPlatformSettings*>& OutMap) = 0;
	virtual ITargetPlatformSettings* GetMultiPlatformSettings() = 0;
};
