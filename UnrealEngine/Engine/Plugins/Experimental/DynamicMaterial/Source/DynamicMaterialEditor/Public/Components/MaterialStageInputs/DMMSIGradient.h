// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialStageInputs/DMMSIThroughput.h"
#include "Templates/SubclassOf.h"
#include "UObject/StrongObjectPtr.h"
#include "DMMSIGradient.generated.h"

class UDMMaterialLayerObject;
class UDMMaterialStageGradient;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageInputGradient : public UDMMaterialStageInputThroughput
{
	GENERATED_BODY()

public:
	static UDMMaterialStage* CreateStage(TSubclassOf<UDMMaterialStageGradient> InMaterialStageGradientClass, UDMMaterialLayerObject* InLayer = nullptr);

	static const TArray<TStrongObjectPtr<UClass>>& GetAvailableGradients();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputGradient* ChangeStageSource_Gradient(UDMMaterialStage* InStage,
		TSubclassOf<UDMMaterialStageGradient> InGradientClass);

	template<typename InGradientClass>
	static UDMMaterialStageInputGradient* ChangeStageSource_Gradient(UDMMaterialStage* InStage)
	{
		return ChangeStageSource_Gradient(InStage, InGradientClass::StaticClass());
	}

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputGradient* ChangeStageInput_Gradient(UDMMaterialStage* InStage,
		TSubclassOf<UDMMaterialStageGradient> InGradientClass, int32 InInputIdx, int32 InInputChannel, int32 InOutputChannel);

	template<typename InGradientClass>
	static UDMMaterialStageInputGradient* ChangeStageInput_Gradient(UDMMaterialStage* InStage, int32 InInputIdx,
		int32 InInputChannel, int32 InOutputChannel)
	{
		return ChangeStageSource_Gradient(InStage, InGradientClass::StaticClass(), InInputIdx, InInputChannel, InOutputChannel);
	}

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TSubclassOf<UDMMaterialStageGradient> GetMaterialStageGradientClass() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetMaterialStageGradientClass(TSubclassOf<UDMMaterialStageGradient> InMaterialStageGradientClass);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialStageGradient* GetMaterialStageGradient() const;

protected:
	static TArray<TStrongObjectPtr<UClass>> Gradients;

	static void GenerateGradientList();

	UDMMaterialStageInputGradient() = default;
};
