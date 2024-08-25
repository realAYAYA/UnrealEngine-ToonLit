// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyBindingUtilsTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "PropertyBindingUtilsTestSuite"

class FPropertyBindingUtilsTestSuiteModule : public IPropertyBindingUtilsTestSuiteModule
{
};

IMPLEMENT_MODULE(FPropertyBindingUtilsTestSuiteModule, PropertyBindingUtilsTestSuite)

#undef LOCTEXT_NAMESPACE
