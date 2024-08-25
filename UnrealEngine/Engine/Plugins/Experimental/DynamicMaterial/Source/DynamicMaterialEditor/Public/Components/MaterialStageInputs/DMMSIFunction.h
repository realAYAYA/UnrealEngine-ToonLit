// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialStageInputs/DMMSIThroughput.h"
#include "DMMSIFunction.generated.h"

class UDMMaterialLayerObject;
class UDMMaterialStageFunction;
class UMaterialFunctionInterface;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageInputFunction : public UDMMaterialStageInputThroughput
{
	GENERATED_BODY()

public:
	static UDMMaterialStage* CreateStage(UDMMaterialLayerObject* InLayer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputFunction* ChangeStageSource_Function(UDMMaterialStage* InStage,
		UMaterialFunctionInterface* InMaterialFunction);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputFunction* ChangeStageInput_Function(UDMMaterialStage* InStage,
		UMaterialFunctionInterface* InMaterialFunction, int32 InInputIdx, int32 InInputChannel, int32 InOutputIdx,
		int32 InOutputChannel);

	void Init();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialStageFunction* GetMaterialStageFunction() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterialFunctionInterface* GetMaterialFunction() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetMaterialFunction(UMaterialFunctionInterface* InMaterialFunction);

protected:
	UDMMaterialStageInputFunction() = default;
};
