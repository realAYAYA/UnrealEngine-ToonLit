// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldConditionsTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "WorldConditionsTestSuite"

class FWorldConditionsTestSuiteModule : public IWorldConditionsTestSuiteModule
{
};

IMPLEMENT_MODULE(FWorldConditionsTestSuiteModule, WorldConditionsTestSuite)

#undef LOCTEXT_NAMESPACE
