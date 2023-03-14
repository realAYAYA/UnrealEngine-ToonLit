// Copyright Epic Games, Inc. All Rights Reserved.

#include "LinearTimecodePlugin.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogLinearTimecode);

class FLinearTimecodeModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FLinearTimecodeModule, LinearTimecode)
