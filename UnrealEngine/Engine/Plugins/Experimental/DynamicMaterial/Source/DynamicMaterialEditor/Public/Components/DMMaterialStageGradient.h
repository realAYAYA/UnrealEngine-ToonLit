// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMMaterialStageThroughput.h"
#include "UObject/StrongObjectPtr.h"
#include "DMMaterialStageGradient.generated.h"

class FMenuBuilder;
class UDMMaterialLayerObject;
class UDMMaterialStageInput;
struct FDMMaterialBuildState;

/**
 * A node which represents UV-based gradient.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Stage Gradient"))
class DYNAMICMATERIALEDITOR_API UDMMaterialStageGradient : public UDMMaterialStageThroughput
{
	GENERATED_BODY()

public:
	static UDMMaterialStage* CreateStage(TSubclassOf<UDMMaterialStageGradient> InMaterialStageGradientClass, UDMMaterialLayerObject* InLayer = nullptr);

	static const TArray<TStrongObjectPtr<UClass>>& GetAvailableGradients();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageGradient* ChangeStageSource_Gradient(UDMMaterialStage* InStage,
		TSubclassOf<UDMMaterialStageGradient> InGradientClass);

	template<typename InGradientClass>
	static UDMMaterialStageGradient* ChangeStageSource_Gradient(UDMMaterialStage* InStage)
	{
		return ChangeStageSource_Gradient(InStage, InGradientClass::StaticClass());
	}

	//~ Begin UDMMaterialStageThroughput
	virtual bool CanChangeInputType(int32 InputIndex) const override;
	//~ End UDMMaterialStageThroughput
	
	//~ Begin UDMMaterialStageSource
	virtual bool SupportsLayerMaskTextureUVLink() const override { return true; }
	virtual int32 GetLayerMaskTextureUVLinkInputIndex() const override { return 0; }
	//~ End UDMMaterialStageSource
	
protected:
	static TArray<TStrongObjectPtr<UClass>> Gradients;

	static void GenerateGradientList();

	UDMMaterialStageGradient();
	UDMMaterialStageGradient(const FText& InName);

};