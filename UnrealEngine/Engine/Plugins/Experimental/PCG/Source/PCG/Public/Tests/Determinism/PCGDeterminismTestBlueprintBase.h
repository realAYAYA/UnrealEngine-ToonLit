// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "PCGDeterminismTestBlueprintBase.generated.h"

struct FDeterminismTestResult;

class UPCGNode;

UCLASS(Abstract, BlueprintType, Blueprintable, hidecategories = (Object))
class PCG_API UPCGDeterminismTestBlueprintBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, CallInEditor, Category = Determinism)
	void ExecuteTest(const UPCGNode* InPCGNode, UPARAM(ref)FDeterminismTestResult& InOutTestResult);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Tests/Determinism/PCGDeterminismTestsCommon.h"
#endif
