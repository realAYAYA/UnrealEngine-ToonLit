// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialStageInputs/DMMSIThroughput.h"
#include "Templates/SubclassOf.h"
#include "UObject/StrongObjectPtr.h"
#include "DMMSIExpression.generated.h"

class UDMMaterialLayerObject;
class UDMMaterialStageExpression;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageInputExpression : public UDMMaterialStageInputThroughput
{
	GENERATED_BODY()

public:
	static UDMMaterialStage* CreateStage(TSubclassOf<UDMMaterialStageExpression> InMaterialStageExpressionClass, UDMMaterialLayerObject* InLayer = nullptr);

	static const TArray<TStrongObjectPtr<UClass>>& GetAvailableInputExpressions();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputExpression* ChangeStageSource_Expression(UDMMaterialStage* InStage, 
		TSubclassOf<UDMMaterialStageExpression> InExpressionClass);

	template<typename InExpressionClass>
	static UDMMaterialStageInputExpression* ChangeStageSource_Expression(UDMMaterialStage* InStage)
	{
		return ChangeStageSource_Expression(InStage, InExpressionClass::StaticClass());
	}

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputExpression* ChangeStageInput_Expression(UDMMaterialStage* InStage, 
		TSubclassOf<UDMMaterialStageExpression> InExpressionClass, int32 InInputIdx, int32 InInputChannel, int32 InOutputIdx,
		int32 InOutputChannel);

	template<typename InExpressionClass>
	static UDMMaterialStageInputExpression* ChangeStageInput_Expression(UDMMaterialStage* InStage, int32 InInputIdx, 
		int32 InInputChannel, int32 InOutputIdx, int32 InOutputChannel)
	{
		return ChangeStageSource_Expression(InStage, InExpressionClass::StaticClass(), InInputIdx, InInputChannel, InOutputIdx,
			InOutputChannel);
	}

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TSubclassOf<UDMMaterialStageExpression> GetMaterialStageExpressionClass() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetMaterialStageExpressionClass(TSubclassOf<UDMMaterialStageExpression> InMaterialStageExpressionClass);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialStageExpression* GetMaterialStageExpression() const;

protected:
	static TArray<TStrongObjectPtr<UClass>> InputExpressions;

	static void GenerateExpressionList();

	UDMMaterialStageInputExpression() = default;
};
