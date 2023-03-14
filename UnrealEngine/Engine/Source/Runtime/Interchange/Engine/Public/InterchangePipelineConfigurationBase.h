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

UCLASS(BlueprintType, Blueprintable, Experimental)
class INTERCHANGEENGINE_API UInterchangePipelineConfigurationBase : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Non virtual helper to allow blueprint to implement event base function to implement the ShowPipelineConfigurationDialog,
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Translator")
	EInterchangePipelineConfigurationDialogResult ScriptedShowPipelineConfigurationDialog(UInterchangeSourceData* SourceData);

	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual ExecuteImportPipeline */
	EInterchangePipelineConfigurationDialogResult ScriptedShowPipelineConfigurationDialog_Implementation(UInterchangeSourceData* SourceData)
	{
		//By default we call the virtual import pipeline execution
		return ShowPipelineConfigurationDialog(SourceData);
	}

	/**
	 * Non virtual helper to allow blueprint to implement event base function to implement the ShowPipelineConfigurationDialog,
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Translator")
	EInterchangePipelineConfigurationDialogResult ScriptedShowScenePipelineConfigurationDialog(UInterchangeSourceData* SourceData);

	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual ExecuteImportPipeline */
	EInterchangePipelineConfigurationDialogResult ScriptedShowScenePipelineConfigurationDialog_Implementation(UInterchangeSourceData* SourceData)
	{
		//By default we call the virtual import pipeline execution
		return ShowScenePipelineConfigurationDialog(SourceData);
	}

	/**
	 * Non virtual helper to allow blueprint to implement event base function to implement the ShowPipelineConfigurationDialog,
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Translator")
	EInterchangePipelineConfigurationDialogResult ScriptedShowReimportPipelineConfigurationDialog(TArray<UInterchangePipelineBase*>& PipelineStack, UInterchangeSourceData* SourceData);

	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual ExecuteImportPipeline */
	EInterchangePipelineConfigurationDialogResult ScriptedShowReimportPipelineConfigurationDialog_Implementation(TArray<UInterchangePipelineBase*>& PipelineStack, UInterchangeSourceData* SourceData)
	{
		//By default we call the virtual import pipeline execution
		return ShowReimportPipelineConfigurationDialog(PipelineStack, SourceData);
	}

protected:

	/**
	 * This function show a dialog use to configure pipeline stacks and return a stack name that tell the caller the user choice.
	 */
	virtual EInterchangePipelineConfigurationDialogResult ShowPipelineConfigurationDialog(TWeakObjectPtr<UInterchangeSourceData> SourceData)
	{ 
		//Not implemented
		return EInterchangePipelineConfigurationDialogResult::Cancel;
	}

	/**
	 * This function show a dialog use to configure pipeline stacks and return a stack name that tell the caller the user choice.
	 */
	virtual EInterchangePipelineConfigurationDialogResult ShowScenePipelineConfigurationDialog(TWeakObjectPtr<UInterchangeSourceData> SourceData)
	{ 
		//Not implemented
		return EInterchangePipelineConfigurationDialogResult::Cancel;
	}

	/**
	 * This function show a dialog use to configure pipeline stacks and return a stack name that tell the caller the user choice.
	 */
	virtual EInterchangePipelineConfigurationDialogResult ShowReimportPipelineConfigurationDialog(TArray<UInterchangePipelineBase*>& PipelineStack, TWeakObjectPtr<UInterchangeSourceData> SourceData)
	{
		//Not implemented
		return EInterchangePipelineConfigurationDialogResult::Cancel;
	}
};
