// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageBlend.h"
#include "DMMaterialStageBlendFunction.generated.h"

class UMaterialExpression;
class UMaterialFunctionInterface;
struct FDMMaterialBuildState;

UCLASS(Abstract, BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageBlendFunction : public UDMMaterialStageBlend
{
	GENERATED_BODY()

public:
	virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;
	virtual void ConnectOutputToInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InInputIndex, UMaterialExpression* InSourceExpression,
		int32 InSourceOutputIndex, int32 InSourceOutputChannel) override;

protected:
	UPROPERTY()
	TObjectPtr<UMaterialFunctionInterface> MaterialFunction;

	UDMMaterialStageBlendFunction();
	UDMMaterialStageBlendFunction(const FText& InName, UMaterialFunctionInterface* InMaterialFunction);
	UDMMaterialStageBlendFunction(const FText& InName, const FName& InFunctionName, const FString& InFunctionPath);
};
