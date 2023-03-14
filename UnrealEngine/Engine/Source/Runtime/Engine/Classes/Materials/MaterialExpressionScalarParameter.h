// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpressionParameter.h"
#include "MaterialExpressionScalarParameter.generated.h"

struct FPropertyChangedEvent;

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionScalarParameter : public UMaterialExpressionParameter
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionScalarParameter, Meta = (ShowAsInputPin = "Primary"))
	float DefaultValue;

	UPROPERTY(EditAnywhere, Category=CustomPrimitiveData)
	bool bUseCustomPrimitiveData = false;

	UPROPERTY(EditAnywhere, Category=CustomPrimitiveData, meta=(ClampMin="0"))
	uint8 PrimitiveDataIndex = 0;
	/** 
	 * Sets the lower bound for the slider on this parameter in the material instance editor. 
	 */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionScalarParameter)
	float SliderMin;

	/** 
	 * Sets the upper bound for the slider on this parameter in the material instance editor. 
	 * The slider will be disabled if SliderMax <= SliderMin.
	 */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionScalarParameter)
	float SliderMax;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool GetParameterValue(FMaterialParameterMetadata& OutMeta) const override
	{
		OutMeta.Value = DefaultValue;
		
		if (bUseCustomPrimitiveData)
		{
			OutMeta.PrimitiveDataIndex = PrimitiveDataIndex;
		}

		OutMeta.ScalarMin = SliderMin;
		OutMeta.ScalarMax = SliderMax;
		return Super::GetParameterValue(OutMeta);
	}
	virtual bool SetParameterValue(const FName& Name, const FMaterialParameterMetadata& Meta, EMaterialExpressionSetParameterValueFlags Flags) override
	{
		if (Meta.Value.Type == EMaterialParameterType::Scalar)
		{
			if (SetParameterValue(Name, Meta.Value.AsScalar(), Flags))
			{
				if (EnumHasAnyFlags(Flags, EMaterialExpressionSetParameterValueFlags::AssignGroupAndSortPriority))
				{
					Group = Meta.Group;
					SortPriority = Meta.SortPriority;
				}
				return true;
			}
		}
		return false;
	}
#endif
	//~ End UMaterialExpression Interface

	UE_DEPRECATED(5.0, "Use GetParameterValue and/or GetParameterName")
	bool IsNamedParameter(const FHashedMaterialParameterInfo& ParameterInfo, float& OutValue) const;

#if WITH_EDITOR
	bool SetParameterValue(FName InParameterName, float InValue, EMaterialExpressionSetParameterValueFlags Flags = EMaterialExpressionSetParameterValueFlags::None);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void ValidateParameterName(const bool bAllowDuplicateName) override;
	virtual bool HasClassAndNameCollision(UMaterialExpression* OtherExpression) const override;
#endif

	virtual bool IsUsedAsAtlasPosition() const { return false; }

	UE_DEPRECATED(5.0, "Use GetAllParameterInfoOfType or GetAllParametersOfType")
	virtual void GetAllParameterInfo(TArray<FMaterialParameterInfo> &OutParameterInfo, TArray<FGuid> &OutParameterIds, const FMaterialParameterInfo& InBaseParameterInfo) const override;
};




