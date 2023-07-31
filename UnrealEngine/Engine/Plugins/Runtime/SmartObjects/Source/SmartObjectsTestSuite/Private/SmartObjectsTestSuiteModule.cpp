// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectsTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "SmartObjectsTestSuite"

class FSmartObjectsTestSuiteModule : public ISmartObjectsTestSuiteModule
{
};

IMPLEMENT_MODULE(FSmartObjectsTestSuiteModule, SmartObjectsTestSuite)

#undef LOCTEXT_NAMESPACE
