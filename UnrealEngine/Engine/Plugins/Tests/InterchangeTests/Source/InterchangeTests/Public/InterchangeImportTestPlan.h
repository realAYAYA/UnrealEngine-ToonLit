// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "InterchangeImportTestPlan.generated.h"

class UInterchangeImportTestStepBase;


/**
* Define a test plan
*/
UCLASS(BlueprintType, EditInlineNew, PrioritizeCategories = Definition) // @TODO: can't find a way to force Run to be below Definition
class INTERCHANGETESTS_API UInterchangeImportTestPlan : public UObject
{
	GENERATED_BODY()

public:

	/** Deserialize a test plan from Json */
	bool ReadFromJson(const FString& Filename);

	/* Serialize a test plan to Json */
	void WriteToJson(const FString& Filename);

public:
	/** Set of steps to perform to carry out this test plan */
	UPROPERTY(EditAnywhere, Instanced, Category = Definition, Meta = (DisplayPriority = 0))
	TArray<TObjectPtr<UInterchangeImportTestStepBase>> Steps;

	/** Whether or not this test plan is currently enabled */
	UPROPERTY(EditAnywhere, Category = Activation)
	bool bIsEnabledInAutomationTests = true;

	/** Why is the test plan disabled. */
	UPROPERTY(EditAnywhere, Category = Activation, Meta = (EditCondition = "!bIsEnabledInAutomationTests"))
	FString DisabledTestReason;

	/** Click here to immediately run this single test through the automation framework */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = Run, Meta = (DisplayPriority = 1))
	void RunThisTest();
};
