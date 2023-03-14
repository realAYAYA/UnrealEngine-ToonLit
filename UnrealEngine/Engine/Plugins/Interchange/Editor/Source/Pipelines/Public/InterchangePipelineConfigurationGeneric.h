// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangePipelineConfigurationBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangePipelineConfigurationGeneric.generated.h"

class UInterchangeSourceData;

UCLASS(BlueprintType, editinlinenew, Experimental)
class INTERCHANGEEDITORPIPELINES_API UInterchangePipelineConfigurationGeneric : public UInterchangePipelineConfigurationBase
{
	GENERATED_BODY()

public:

protected:

	virtual EInterchangePipelineConfigurationDialogResult ShowPipelineConfigurationDialog(TWeakObjectPtr<UInterchangeSourceData> SourceData) override;
	virtual EInterchangePipelineConfigurationDialogResult ShowScenePipelineConfigurationDialog(TWeakObjectPtr<UInterchangeSourceData> SourceData) override;
	virtual EInterchangePipelineConfigurationDialogResult ShowReimportPipelineConfigurationDialog(TArray<UInterchangePipelineBase*>& PipelineStack, TWeakObjectPtr<UInterchangeSourceData> SourceData) override;
};
