// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageThroughput.h"
#include "DMMaterialStageFunction.generated.h"

class UDMMaterialLayerObject;
class UMaterialFunctionInterface;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageFunction : public UDMMaterialStageThroughput
{
	GENERATED_BODY()

public:
	static constexpr int32 InputPreviousStage = 0;

	static UDMMaterialStage* CreateStage(UDMMaterialLayerObject* InLayer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageFunction* ChangeStageSource_Function(UDMMaterialStage* InStage,
		UMaterialFunctionInterface* InMaterialFunction);

	static UMaterialFunctionInterface* GetNoOpFunction();

	UDMMaterialStageFunction();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterialFunctionInterface* GetMaterialFunction() const { return MaterialFunction.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetMaterialFunction(UMaterialFunctionInterface* InMaterialFunction);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialValue* GetInputValue(int32 InIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	TArray<UDMMaterialValue*> GetInputValues() const;

	//~ Begin UDMMaterialStageThroughput
	virtual void AddDefaultInput(int32 InInputIndex) const override;
	virtual bool CanChangeInput(int32 InputIndex) const override;
	virtual bool CanChangeInputType(int32 InputIndex) const override;
	virtual bool IsInputVisible(int32 InputIndex) const override;
	//~ End UDMMaterialStageThroughput

	//~ Begin UDMMaterialStageSource
	virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;
	//~ End UDMMaterialStageSource

	//~ Begin UDMMaterialComponent
	virtual FText GetComponentDescription() const override;
	//~ End UDMMaterialComponent

	//~ Begin UObject
	virtual void PreEditChange(FEditPropertyChain& PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	//~ End UObject

protected:
	static TSoftObjectPtr<UMaterialFunctionInterface> NoOp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = GetMaterialFunction, Setter = SetMaterialFunction, BlueprintSetter = SetMaterialFunction, Category = "Material Designer",
		meta = (DisplayThumbnail = true, AllowPrivateAccess = "true", HighPriority, NotKeyframeable))
	TObjectPtr<UMaterialFunctionInterface> MaterialFunction;

	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TObjectPtr<UMaterialFunctionInterface> MaterialFunction_PreEdit;

	void OnMaterialFunctionChanged();

	void InitFunction();

	void DeinitFunction();

	bool NeedsFunctionInit() const;

	//~ Begin UDMMaterialComponent
	virtual void OnComponentAdded() override;
	virtual void OnComponentRemoved() override;
	//~ End UDMMaterialComponent
};
