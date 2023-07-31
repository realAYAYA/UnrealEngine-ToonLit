// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangePipelineBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"


#include "InterchangePythonPipelineBase.generated.h"

/*
 * This class represent a python pipeline. It is use by the TSoftClassPtr of the PythonPipeline asset.
 *
 */
UCLASS(BlueprintType, Abstract, Experimental)
class INTERCHANGEENGINE_API UInterchangePythonPipelineBase : public UInterchangePipelineBase
{
	GENERATED_BODY()
public:

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};


USTRUCT()
struct FPropertyData
{
	GENERATED_BODY()

	FString DefaultValue;
	bool bLocked = false;
};

/*
 * This class is a helper class for python pipeline. It allow picker to filter correctly the content browser instance we want to support
 * in the pipeline stack.
 *
 * Use this class factory to be able to create an instance of a python pipeline in the content browser.
 * A python pipeline do not have any locked properties and all default value must be set in the python script.
 * This restriction exist because python class are transient, so any assets create from a python class cannot be save.
 * 
 */
UCLASS(BlueprintType, Experimental)
class INTERCHANGEENGINE_API UInterchangePythonPipelineAsset : public UObject
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void GeneratePipeline();

	void SetupFromPipeline(const UInterchangePythonPipelineBase* PythonPipeline, const bool bRegeneratePipeline = true);

	/** The python class we want to use has a pipeline */
	UPROPERTY(EditAnywhere, Category = "Interchange|Python")
	TSoftClassPtr<UInterchangePythonPipelineBase> PythonClass;

	/** The transient pipeline we generate when we load the python pipeline */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Interchange|Python")
	TObjectPtr<UInterchangePythonPipelineBase> GeneratedPipeline;

	UPROPERTY(VisibleAnywhere, Category = "Interchange|Python")
	FString JsonDefaultProperties;
};

