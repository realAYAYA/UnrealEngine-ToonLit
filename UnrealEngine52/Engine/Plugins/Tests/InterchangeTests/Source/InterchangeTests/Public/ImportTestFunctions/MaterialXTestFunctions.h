// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImportTestFunctions/ImportTestFunctionsBase.h"

#include "MaterialXTestFunctions.generated.h"

class UImportTestFunctionsBase;

struct FInterchangeTestFunctionResult;

class UMaterialInterface;

UCLASS()
class INTERCHANGETESTS_API UMaterialXTestFunctions : public UImportTestFunctionsBase
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the expected number of inputs are connected to the MX_StandardSurface material function */
	UFUNCTION(Exec, meta = (DisplayName = "MX: Check Connected Input Count"))
	static FInterchangeTestFunctionResult CheckConnectedInputCount(const UMaterialInterface* MaterialInterface, int32 ExpectedNumber);

	/** Check whether a specific input of the MX_StandardSurface material function is connected or not */
	UFUNCTION(Exec, meta = (DisplayName = "MX: Check Input Is Connected"))
	static FInterchangeTestFunctionResult CheckInputConnected(const UMaterialInterface* MaterialInterface, const FString& InputName, bool bIsConnected);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "ImportTestFunctionsBase.h"
#include "InterchangeTestFunction.h"
#endif
