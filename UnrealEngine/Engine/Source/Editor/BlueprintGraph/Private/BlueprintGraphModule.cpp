// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphModule.h"

#include "EdGraphSchema_K2.h"
#include "BlueprintTypePromotion.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE( FBlueprintGraphModule, BlueprintGraph );

void FBlueprintGraphModule::ShutdownModule()
{
	UEdGraphSchema_K2::Shutdown();
	FTypePromotion::Shutdown();
}