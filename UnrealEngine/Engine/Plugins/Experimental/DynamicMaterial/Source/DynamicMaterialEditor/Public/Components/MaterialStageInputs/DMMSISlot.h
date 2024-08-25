// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageInput.h"
#include "Templates/SubclassOf.h"
#include "DMMSISlot.generated.h"

class UDMMaterialLayerObject;
class UDMMaterialStage;
class UDynamicMaterialModel;
struct FDMMaterialBuildState;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageInputSlot : public UDMMaterialStageInput
{
	GENERATED_BODY()

public:
	static const FString SlotPathToken;

	static UDMMaterialStage* CreateStage(UDMMaterialSlot* InSourceSlot, EDMMaterialPropertyType InMaterialProperty, 
		UDMMaterialLayerObject* InLayer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputSlot* ChangeStageSource_Slot(UDMMaterialStage* InStage, UDMMaterialSlot* InSlot,
		EDMMaterialPropertyType InProperty);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputSlot* ChangeStageInput_Slot(UDMMaterialStage* InStage, int32 InInputIdx, int32 InInputChannel, 
		UDMMaterialSlot* InSlot, EDMMaterialPropertyType InProperty, int32 InOutputIdx, int32 InOutputChannel);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialSlot* GetSlot() const { return Slot; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMMaterialPropertyType GetMaterialProperty() const { return MaterialProperty; }

	virtual FText GetComponentDescription() const override;
	virtual FText GetChannelDescription(const FDMMaterialStageConnectorChannel& Channel) override;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetSlot(UDMMaterialSlot* InSlot);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetMaterialProperty(EDMMaterialPropertyType InMaterialProperty);

	virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;

	//~ Begin UObject
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
	//~ End UObject

	//~ Start UDMMaterialComponent
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

protected:
	UDMMaterialStageInputSlot();
	virtual ~UDMMaterialStageInputSlot() override = default;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TObjectPtr<UDMMaterialSlot> Slot;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	EDMMaterialPropertyType MaterialProperty;

	void OnSlotUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);
	void OnSlotConnectorsUpdated(UDMMaterialSlot* InSlot);
	void OnSlotRemoved(UDMMaterialComponent* InComponent, EDMComponentLifetimeState InLifetimeState);
	void OnParentSlotRemoved(UDMMaterialComponent* InComponent, EDMComponentLifetimeState InLifetimeState);

	virtual void UpdateOutputConnectors() override;

	void InitSlot();
	void DeinitSlot();

	//~ Begin UDMMaterialComponent
	virtual void OnComponentRemoved() override;
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	//~ End UDMMaterialComponent
};
