// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialComponent.h"
#include "DMDefs.h"
#include "DMMaterialEffectStack.generated.h"

class UDMMaterialEffect;
class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UDMMaterialStage;
class UMaterialExpression;
enum class EDMMaterialEffectTarget : uint8;
struct FDMMaterialBuildState;

/**
 * Container for effects. Effects can be applied to either layers (on a per stage basis) or to slots.
 */
UCLASS(BlueprintType, ClassGroup = "Material Designer", Meta = (DisplayName = "Material Designer Effect Stack"))
class DYNAMICMATERIALEDITOR_API UDMMaterialEffectStack : public UDMMaterialComponent
{
	GENERATED_BODY()

public:
	static const FString EffectsPathToken;

	using FEffectCallbackFunc = TFunctionRef<void(UDMMaterialEffect*)>;

	UFUNCTION(BlueprintCallable, Category = "Material Designer", Meta = (DisplayName = "Create Effect Stack (For Slot)"))
	static UDMMaterialEffectStack* CreateEffectStackForSlot(UDMMaterialSlot* InSlot)
	{
		return CreateEffectStack(InSlot);
	}

	UFUNCTION(BlueprintCallable, Category = "Material Designer", Meta = (DisplayName = "Create Effect Stack (For Layer)"))
	static UDMMaterialEffectStack* CreateEffectStackForLayer(UDMMaterialLayerObject* InLayer)
	{
		return CreateEffectStack(InLayer);
	}

	static UDMMaterialEffectStack* CreateEffectStack(UDMMaterialSlot* InSlot);
	static UDMMaterialEffectStack* CreateEffectStack(UDMMaterialLayerObject* InLayer);

	UDMMaterialEffectStack();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialSlot* GetSlot() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialLayerObject* GetLayer() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool IsEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetEnabled(bool bInIsEnabled);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialEffect* GetEffect(int32 InIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer", Meta = (DisplayName = "Get Effects"))
	TArray<UDMMaterialEffect*> BP_GetEffects() const;

	const TArray<TObjectPtr<UDMMaterialEffect>>& GetEffects() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool HasEffect(const UDMMaterialEffect* InEffect) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool AddEffect(UDMMaterialEffect* InEffect);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetEffect(int32 InIndex, UDMMaterialEffect* InEffect);

	UFUNCTION(BlueprintCallable, Category = "Material Designer", Meta = (DisplayName = "Move Effect (By Index)"))
	bool BP_MoveEffectByIndex(int32 InIndex, int32 InNewIndex)
	{
		return MoveEffect(InIndex, InNewIndex);
	}

	bool MoveEffect(int32 InIndex, int32 InNewIndex);

	UFUNCTION(BlueprintCallable, Category = "Material Designer", Meta = (DisplayName = "Move Effect (By Value)"))
	bool BP_MoveEffectByValue(UDMMaterialEffect* InEffect, int32 InNewIndex)
	{
		return MoveEffect(InEffect, InNewIndex);
	}

	bool MoveEffect(UDMMaterialEffect* InEffect, int32 InNewIndex);

	UFUNCTION(BlueprintCallable, Category = "Material Designer", Meta = (DisplayName = "Remove Effect (By Index)"))
	bool BP_RemoveEffectByIndex(int32 InIndex)
	{
		return RemoveEffect(InIndex);
	}

	bool RemoveEffect(int32 InIndex);

	UFUNCTION(BlueprintCallable, Category = "Material Designer", Meta = (DisplayName = "Remove Effect (By Value)"))
	bool BP_RemoveEffectByValue(UDMMaterialEffect* InEffect)
	{
		return RemoveEffect(InEffect);
	}

	bool RemoveEffect(UDMMaterialEffect* InEffect);

	bool ApplyEffects(const TSharedRef<FDMMaterialBuildState>& InBuildState, EDMMaterialEffectTarget InEffectTarget,
		TArray<UMaterialExpression*>& InOutStageExpressions, int32& InOutLastExpressionOutputChannel, int32& InOutLastExpressionOutputIndex) const;

	//~ Begin UDMMaterialComponent
	virtual UDMMaterialComponent* GetParentComponent() const override;
	virtual FString GetComponentPathComponent() const override;
	virtual FText GetComponentDescription() const override;
	virtual void Update(EDMUpdateType InUpdateType) override;
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

	//~ Begin UObject
	virtual bool Modify(bool bInAlwaysMarkDirty = true) override;
	virtual void PostEditUndo() override;
	//~ End UObject

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bEnabled;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TArray<TObjectPtr<UDMMaterialEffect>> Effects;

	//~ Begin UDMMaterialComponent
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	virtual void OnComponentAdded() override;
	virtual void OnComponentRemoved() override;
	//~ End UDMMaterialComponent
};
