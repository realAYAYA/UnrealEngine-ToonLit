// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLAdapterTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "MLAdapterTestSuite"

class FMLAdapterTestSuiteModule : public IMLAdapterTestSuiteModule
{
};

IMPLEMENT_MODULE(FMLAdapterTestSuiteModule, MLAdapterTestSuite)

#undef LOCTEXT_NAMESPACE
