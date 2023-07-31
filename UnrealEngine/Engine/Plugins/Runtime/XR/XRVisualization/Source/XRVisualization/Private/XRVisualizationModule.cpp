// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRVisualizationModule.h"
#include "Modules/ModuleManager.h"

class FXRVisualizationModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FXRVisualizationModule, XRVisualization);

DEFINE_LOG_CATEGORY(LogXRVisual);
