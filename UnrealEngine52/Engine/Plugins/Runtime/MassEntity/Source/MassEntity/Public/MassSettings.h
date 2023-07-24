// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "MassSettings.generated.h"


/** 
 * A common parrent for Mass's per-module settings. Classes extending this class will automatically get registered 
 * with- and show under Mass settings in Project Settings.
 */
UCLASS(Abstract, config = Mass, defaultconfig, collapseCategories)
class MASSENTITY_API UMassModuleSettings : public UObject
{
	GENERATED_BODY()
protected:
	virtual void PostInitProperties() override;
};


UCLASS(config = Mass, defaultconfig, DisplayName = "Mass", AutoExpandCategories = "Mass")
class MASSENTITY_API UMassSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	void RegisterModuleSettings(UMassModuleSettings& SettingsCDO);

public:
	UPROPERTY(VisibleAnywhere, Category = "Mass", NoClear, EditFixedSize, meta = (EditInline))
	TMap<FName, TObjectPtr<UMassModuleSettings>> ModuleSettings;
};
