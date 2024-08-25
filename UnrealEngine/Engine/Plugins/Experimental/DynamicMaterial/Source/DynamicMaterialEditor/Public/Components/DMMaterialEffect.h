// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialComponent.h"
#include "Components/DMMaterialStage.h"
#include "DMDefs.h"
#include "DMMaterialEffect.generated.h"

class UDMMaterialEffect;
class UDMMaterialEffectStack;
class UDMMaterialLayerObject;
class UDMMaterialValue;
class UMaterialExpression;
struct FDMMaterialBuildState;

UENUM(BlueprintType)
enum class EDMMaterialEffectTarget : uint8
{
	None      = 0,
	BaseStage = 1 << 0,
	MaskStage = 1 << 1,
	TextureUV = 1 << 2,
	Slot      = 1 << 3
};

UCLASS(Abstract, BlueprintType, ClassGroup = "Material Designer", Meta = (DisplayName = "Material Designer Effect"))
class DYNAMICMATERIALEDITOR_API UDMMaterialEffect : public UDMMaterialComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	static EDMMaterialEffectTarget StageTypeToEffectType(EDMMaterialLayerStage InStageType);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialEffect* CreateEffect(UDMMaterialEffectStack* InEffectStack, TSubclassOf<UDMMaterialEffect> InEffectClass);

	template<typename InEffectClass>
	static InEffectClass* CreateEffect(UDMMaterialEffectStack* InEffectStack)
	{
		return Cast<InEffectClass>(CreateEffect(InEffectStack, InEffectClass::StaticClass()));
	}

	UDMMaterialEffect();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialEffectStack* GetEffectStack() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	int32 FindIndex() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetEnabled(bool bInIsEnabled);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMMaterialEffectTarget GetEffectTarget() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual FText GetEffectName() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual FText GetEffectDescription() const PURE_VIRTUAL(UDMMaterialEffect::ApplyTo, return FText::GetEmpty();)

	virtual void ApplyTo(const TSharedRef<FDMMaterialBuildState>& InBuildState, TArray<UMaterialExpression*>& InOutExpressions, 
		int32& InOutLastExpressionOutputChannel, int32& InLastExpressionOutputIndex) const PURE_VIRTUAL(UDMMaterialEffect::ApplyTo)

	//~ Begin UDMMaterialComponent
	virtual UDMMaterialComponent* GetParentComponent() const override;
	virtual FString GetComponentPathComponent() const override;
	virtual FText GetComponentDescription() const override;
	virtual void Update(EDMUpdateType InUpdateType) override;
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

	//~ Begin UObject
	virtual void PostEditUndo() override;
	//~ End UObject

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	EDMMaterialEffectTarget EffectTarget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bEnabled;
};
