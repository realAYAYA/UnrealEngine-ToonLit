// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

namespace UE::AnimNext::Tests
{

class FModule : public IModuleInterface
{
public:
};

}

IMPLEMENT_MODULE(UE::AnimNext::Tests::FModule, AnimNextTestSuite)
