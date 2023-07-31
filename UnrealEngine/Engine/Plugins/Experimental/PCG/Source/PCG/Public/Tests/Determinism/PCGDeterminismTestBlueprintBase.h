// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tests/Determinism/PCGDeterminismTestsCommon.h"

#include "PCGDeterminismTestBlueprintBase.generated.h"

class UPCGNode;

UCLASS(Abstract, BlueprintType, Blueprintable, hidecategories = (Object))
class PCG_API UPCGDeterminismTestBlueprintBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, CallInEditor, Category = Determinism)
	void ExecuteTest(const UPCGNode* InPCGNode, UPARAM(ref)FDeterminismTestResult& InOutTestResult);
};