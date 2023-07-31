// Copyright Epic Games, Inc. All Rights Reserved.

#include "EmptyTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "GameplayBehaviorsTestSuite"

class FGameplayBehaviorsTestSuiteModule : public IGameplayBehaviorsTestSuiteModule
{
};

IMPLEMENT_MODULE(FGameplayBehaviorsTestSuiteModule, GameplayBehaviorsTestSuiteModule)

#undef LOCTEXT_NAMESPACE
