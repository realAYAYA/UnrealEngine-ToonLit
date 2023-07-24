// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangePipelineConfigurationBase.h"

#include "InterchangePipelineConfigurationGeneric.generated.h"

class UInterchangeSourceData;

UCLASS(BlueprintType, editinlinenew, Experimental)
class INTERCHANGEEDITORPIPELINES_API UInterchangePipelineConfigurationGeneric : public UInterchangePipelineConfigurationBase
{
	GENERATED_BODY()

public:

protected:

	virtual EInterchangePipelineConfigurationDialogResult ShowPipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, TWeakObjectPtr<UInterchangeSourceData> SourceData) override;
	virtual EInterchangePipelineConfigurationDialogResult ShowScenePipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, TWeakObjectPtr<UInterchangeSourceData> SourceData) override;
	virtual EInterchangePipelineConfigurationDialogResult ShowReimportPipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, TWeakObjectPtr<UInterchangeSourceData> SourceData) override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
