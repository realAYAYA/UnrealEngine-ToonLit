// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "StateTreeTestSuite"

class FStateTreeTestSuiteModule : public IStateTreeTestSuiteModule
{
};

IMPLEMENT_MODULE(FStateTreeTestSuiteModule, StateTreeTestSuite)

#undef LOCTEXT_NAMESPACE
