// Copyright Epic Games, Inc. All Rights Reserved.

#include "IInstancedActorsTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "InstancedActorsTestSuite"

class FInstancedActorsTestSuiteModule : public IInstancedActorsTestSuiteModule
{
};

IMPLEMENT_MODULE(FInstancedActorsTestSuiteModule, InstancedActorsTestSuite)

#undef LOCTEXT_NAMESPACE
