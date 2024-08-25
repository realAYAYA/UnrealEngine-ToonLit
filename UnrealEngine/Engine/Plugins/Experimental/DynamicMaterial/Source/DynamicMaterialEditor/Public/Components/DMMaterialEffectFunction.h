// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialEffect.h"
#include "DMMaterialEffectFunction.generated.h"

class UMaterialFunctionInterface;

UCLASS(BlueprintType, ClassGroup = "Material Designer", Meta = (DisplayName = "Material Designer Effect Function"))
class DYNAMICMATERIALEDITOR_API UDMMaterialEffectFunction : public UDMMaterialEffect
{
	GENERATED_BODY()

public:
	static const FString InputsPathToken;

	UDMMaterialEffectFunction();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterialFunctionInterface* GetMaterialFunction() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetMaterialFunction(UMaterialFunctionInterface* InFunction);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialValue* GetInputValue(int32 InIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer", Meta = (DisplayName = "Get Input Values"))
	TArray<UDMMaterialValue*> BP_GetInputValues() const;

	const TArray<TObjectPtr<UDMMaterialValue>>& GetInputValues() const;

	//~ Begin UDMMaterialEffect
	virtual FText GetEffectName() const override;
	virtual FText GetEffectDescription() const override;
	virtual void ApplyTo(const TSharedRef<FDMMaterialBuildState>& InBuildState, TArray<UMaterialExpression*>& InOutExpressions,
		int32& InOutLastExpressionOutputChannel, int32& InOutLastExpressionOutputIndex) const override;
	//~ End UDMMaterialEffect

	//~ Begin UDMMaterialComponent
	virtual FText GetComponentDescription() const override;
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

	//~ Begin UObject
	virtual bool Modify(bool bInAlwaysMarkDirty = true) override;
	virtual void PostLoad() override;
	//~ End UObject

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TObjectPtr<UMaterialFunctionInterface> MaterialFunctionPtr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TArray<TObjectPtr<UDMMaterialValue>> InputValues;

	void OnMaterialFunctionChanged();

	void InitFunction();

	void DeinitFunction();

	bool NeedsFunctionInit() const;

	//~ Begin UDMMaterialComponent
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	virtual void OnComponentAdded() override;
	virtual void OnComponentRemoved() override;
	//~ End UDMMaterialComponent
};
