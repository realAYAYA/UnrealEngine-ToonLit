// Copyright Epic Games, Inc. All Rights Reserved.

#include "StructUtilsTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "StructUtilsTestSuite"

class FStructUtilsTestSuiteModule : public IStructUtilsTestSuiteModule
{
};

IMPLEMENT_MODULE(FStructUtilsTestSuiteModule, StructUtilsTestSuite)

#undef LOCTEXT_NAMESPACE
