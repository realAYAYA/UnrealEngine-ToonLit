// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "SoundFields.h"

class FSoundFieldsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

private:
	FAmbisonicsSoundfieldFormat SFFactory;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#endif
