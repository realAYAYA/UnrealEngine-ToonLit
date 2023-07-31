// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleInterface.h"
#include "WaterEditorServices.h"

//////////////////////////////////////////////////////////////////////////
// IWaterModuleInterface

WATER_API DECLARE_LOG_CATEGORY_EXTERN(LogWater, Log, All);


class IWaterModuleInterface : public IModuleInterface
{
public:
#if WITH_EDITOR
	virtual WATER_API void SetWaterEditorServices(IWaterEditorServices* InWaterEditorServices) = 0;
	virtual WATER_API IWaterEditorServices* GetWaterEditorServices() const = 0;
#endif // WITH_EDITOR
};