// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangeManager.h"
#include "InterchangeImportTestStepBase.generated.h"

class FAutomationTestExecutionInfo;
struct FInterchangeImportTestData;
struct FInterchangeTestFunction;


struct FTestStepResults
{
	bool bTestStepSuccess = true;
	bool bTriggerGC = false;
};


UCLASS(BlueprintType, EditInlineNew, Abstract, autoExpandCategories = (General, Test))
class INTERCHANGETESTS_API UInterchangeImportTestStepBase : public UObject
{
	GENERATED_BODY()

public:
	/** An array of results to check against */
	UPROPERTY(EditAnywhere, Category = General)
	TArray<FInterchangeTestFunction> Tests;

public:
	virtual TTuple<UE::Interchange::FAssetImportResultPtr, UE::Interchange::FSceneImportResultPtr>
		StartStep(FInterchangeImportTestData& Data) PURE_VIRTUAL(UInterchangeImportTestStepBase::StartStep, return {}; );
	virtual FTestStepResults FinishStep(FInterchangeImportTestData& Data, FAutomationTestExecutionInfo& ExecutionInfo) PURE_VIRTUAL(UInterchangeImportTestStepBase::FinishStep, return {}; );
	virtual FString GetContextString() const PURE_VIRTUAL(UInterchangeImportTestStepBase::GetContextString, return {}; );

	bool PerformTests(FInterchangeImportTestData& Data, FAutomationTestExecutionInfo& ExecutionInfo);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "InterchangeImportTestData.h"
#include "InterchangeTestFunction.h"
#include "Misc/AutomationTest.h"
#endif
