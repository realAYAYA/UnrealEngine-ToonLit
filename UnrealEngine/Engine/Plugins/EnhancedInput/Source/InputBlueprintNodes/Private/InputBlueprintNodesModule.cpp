// Copyright Epic Games, Inc. All Rights Reserved.

#include "InputBlueprintNodesModule.h"

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "InputBlueprintNodes"

class FInputBlueprintNodesModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FInputBlueprintNodesModule, InputBlueprintNodes)

#undef LOCTEXT_NAMESPACE