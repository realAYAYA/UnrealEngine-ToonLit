// Copyright Epic Games, Inc. All Rights Reserved.

#include "HTNTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "HTNTestSuite"

class FHTNTestSuiteModule : public IHTNTestSuiteModule
{
};

IMPLEMENT_MODULE(FHTNTestSuiteModule, HTNTestSuite)

#undef LOCTEXT_NAMESPACE
