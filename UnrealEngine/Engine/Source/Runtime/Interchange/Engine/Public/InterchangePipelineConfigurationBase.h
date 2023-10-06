// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "InterchangeSourceData.h"

#include "InterchangePipelineConfigurationBase.generated.h"

UENUM(BlueprintType, Experimental)
enum class EInterchangePipelineConfigurationDialogResult : uint8
{
	Cancel		UMETA(DisplayName = "Cancel"),
	Import		UMETA(DisplayName = "Import"),
	ImportAll	UMETA(DisplayName = "Import All"),
};

USTRUCT(BlueprintType)
struct FInterchangeStackInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange | Translator")
	FName StackName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange | Translator")
	TArray<TObjectPtr<UInterchangePipelineBase>> Pipelines;
};

UCLASS(BlueprintType, Blueprintable, Experimental, MinimalAPI)
class UInterchangePipelineConfigurationBase : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Non virtual helper to allow blueprint to implement event base function to implement the ShowPipelineConfigurationDialog,
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Translator")
	INTERCHANGEENGINE_API EInterchangePipelineConfigurationDialogResult ScriptedShowPipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, UInterchangeSourceData* SourceData);

	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual ExecuteImportPipeline */
	EInterchangePipelineConfigurationDialogResult ScriptedShowPipelineConfigurationDialog_Implementation(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, UInterchangeSourceData* SourceData)
	{
		//By default we call the virtual import pipeline execution
		return ShowPipelineConfigurationDialog(PipelineStacks, OutPipelines, SourceData);
	}

	/**
	 * Non virtual helper to allow blueprint to implement event base function to implement the ShowPipelineConfigurationDialog,
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Translator")
	INTERCHANGEENGINE_API EInterchangePipelineConfigurationDialogResult ScriptedShowScenePipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, UInterchangeSourceData* SourceData);

	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual ExecuteImportPipeline */
	EInterchangePipelineConfigurationDialogResult ScriptedShowScenePipelineConfigurationDialog_Implementation(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, UInterchangeSourceData* SourceData)
	{
		//By default we call the virtual import pipeline execution
		return ShowScenePipelineConfigurationDialog(PipelineStacks, OutPipelines, SourceData);
	}

	/**
	 * Non virtual helper to allow blueprint to implement event base function to implement the ShowPipelineConfigurationDialog,
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Translator")
	INTERCHANGEENGINE_API EInterchangePipelineConfigurationDialogResult ScriptedShowReimportPipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, UInterchangeSourceData* SourceData);

	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual ExecuteImportPipeline */
	EInterchangePipelineConfigurationDialogResult ScriptedShowReimportPipelineConfigurationDialog_Implementation(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, UInterchangeSourceData* SourceData)
	{
		//By default we call the virtual import pipeline execution
		return ShowReimportPipelineConfigurationDialog(PipelineStacks, OutPipelines, SourceData);
	}

protected:

	/**
	 * This function show a dialog use to configure pipeline stacks and return a stack name that tell the caller the user choice.
	 */
	virtual EInterchangePipelineConfigurationDialogResult ShowPipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, TWeakObjectPtr<UInterchangeSourceData> SourceData)
	{ 
		//Not implemented
		return EInterchangePipelineConfigurationDialogResult::Cancel;
	}

	/**
	 * This function show a dialog use to configure pipeline stacks and return a stack name that tell the caller the user choice.
	 */
	virtual EInterchangePipelineConfigurationDialogResult ShowScenePipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, TWeakObjectPtr<UInterchangeSourceData> SourceData)
	{ 
		//Not implemented
		return EInterchangePipelineConfigurationDialogResult::Cancel;
	}

	/**
	 * This function show a dialog use to configure pipeline stacks and return a stack name that tell the caller the user choice.
	 */
	virtual EInterchangePipelineConfigurationDialogResult ShowReimportPipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, TWeakObjectPtr<UInterchangeSourceData> SourceData)
	{
		//Not implemented
		return EInterchangePipelineConfigurationDialogResult::Cancel;
	}
};
