// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassGameplayTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "MassTest"

class FMassGameplayTestSuiteModule : public IMassGameplayTestSuiteModule
{
};

IMPLEMENT_MODULE(FMassGameplayTestSuiteModule, MassGameplayTestSuite)

#undef LOCTEXT_NAMESPACE
