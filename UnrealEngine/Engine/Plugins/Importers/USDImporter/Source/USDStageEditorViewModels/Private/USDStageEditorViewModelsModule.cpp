// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDStageEditorViewModelsModule.h"

#include "USDMemory.h"

#include "Modules/ModuleManager.h"

class FUsdStageEditorViewModelsModule : public IUsdStageEditorViewModelsModule
{
public:

};

IMPLEMENT_MODULE_USD( FUsdStageEditorViewModelsModule, USDStageEditorViewModels );
