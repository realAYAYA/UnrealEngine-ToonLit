// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "MassTest"

class FMassEntityTestSuiteModule : public IMassEntityTestSuiteModule
{
};

IMPLEMENT_MODULE(FMassEntityTestSuiteModule, MassEntityTestSuite)

#undef LOCTEXT_NAMESPACE
