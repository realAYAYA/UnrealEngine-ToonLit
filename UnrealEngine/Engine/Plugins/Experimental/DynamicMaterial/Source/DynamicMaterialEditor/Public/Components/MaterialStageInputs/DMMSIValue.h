// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageInput.h"
#include "Templates/SubclassOf.h"
#include "DMMSIValue.generated.h"

class UDMMaterialLayerObject;
class UDMMaterialStage;
class UDynamicMaterialModel;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageInputValue : public UDMMaterialStageInput
{
	GENERATED_BODY()
		
public:
	static const FString ValuePathToken;

	static FName GetValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMMaterialStageInputValue, Value); }

	static UDMMaterialStage* CreateStage(UDMMaterialValue* InValue, UDMMaterialLayerObject* InLayer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputValue* ChangeStageSource_NewLocalValue(UDMMaterialStage* InStage, EDMValueType InType);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputValue* ChangeStageSource_Value(UDMMaterialStage* InStage, UDMMaterialValue* InValue);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputValue* ChangeStageSource_NewValue(UDMMaterialStage* InStage, EDMValueType InType);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputValue* ChangeStageInput_NewLocalValue(UDMMaterialStage* InStage, int32 InInputIdx, int32 InInputChannel,
		EDMValueType InType, int32 InOutputChannel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputValue* ChangeStageInput_Value(UDMMaterialStage* InStage, int32 InInputIdx, int32 InInputChannel,
		UDMMaterialValue* InValue, int32 InOutputChannel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputValue* ChangeStageInput_NewValue(UDMMaterialStage* InStage, int32 InInputIdx, int32 InInputChannel,
		EDMValueType InType, int32 InOutputChannel);

	virtual FText GetComponentDescription() const override;
	virtual FText GetChannelDescription(const FDMMaterialStageConnectorChannel& Channel) override;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialValue* GetValue() const { return Value; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetValue(UDMMaterialValue* InValue);

	virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;

	virtual int32 GetInnateMaskOutput(int32 OutputIndex, int32 OutputChannels) const override;

	//~ Begin UObject
	virtual bool Modify(bool bInAlwaysMarkDirty = true) override;
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
	//~ End UObject

	//~ Start UDMMaterialComponent
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

protected:
	UDMMaterialStageInputValue();
	virtual ~UDMMaterialStageInputValue() override = default;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TObjectPtr<UDMMaterialValue> Value;

	void OnValueUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);

	virtual void UpdateOutputConnectors() override;

	void InitInputValue();
	void DeinitInputValue();

	//~ Begin UDMMaterialStageSource
	virtual void OnComponentAdded() override;
	virtual void OnComponentRemoved() override;
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	//~ End UDMMaterialStageSource

	bool IsSharedStageValue() const;

	void ApplyWholeLayerValue();
};
