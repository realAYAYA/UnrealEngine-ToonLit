// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class AUsdStageActor;
class UWorld;

class IUsdStageModule : public IModuleInterface
{
public:
	virtual AUsdStageActor& GetUsdStageActor( UWorld* World ) = 0;
	virtual AUsdStageActor* FindUsdStageActor( UWorld* World ) = 0;
};
