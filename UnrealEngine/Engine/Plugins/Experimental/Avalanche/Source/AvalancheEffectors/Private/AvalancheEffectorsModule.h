// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Modules/ModuleInterface.h"

class AActor;

class FAvalancheEffectorsModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

protected:
	void RegisterCustomActorResolver();
	void UnregisterCustomActorResolver();

	TArray<AActor*> GetOrderedChildrenActors(const AActor* InParentActor);
};
