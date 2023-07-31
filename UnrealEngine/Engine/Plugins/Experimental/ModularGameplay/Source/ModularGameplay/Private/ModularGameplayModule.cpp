// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularGameplayModule.h"
#include "ModularGameplayLogs.h"

class FModularGameplayModule : public IModularGameplayModule
{
};

IMPLEMENT_MODULE(FModularGameplayModule, ModularGameplay)

DEFINE_LOG_CATEGORY(LogModularGameplay);