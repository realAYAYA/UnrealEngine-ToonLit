// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImportTestFunctionsBase.h"
#include "InterchangeTestFunction.h"
#include "AnimationImportTestFunctions.generated.h"

struct FInterchangeTestFunctionResult;


UCLASS()
class INTERCHANGETESTS_API UAnimationImportTestFunctions : public UImportTestFunctionsBase
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;
};
