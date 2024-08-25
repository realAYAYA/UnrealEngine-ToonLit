// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "Components/DMMaterialComponent.h"
#include "Math/MathFwd.h"
#include "Templates/SharedPointerFwd.h"
#include "DMMaterialProperty.generated.h"

class FText;
class UDMMaterialSlot;
class UDynamicMaterialModel;
class UDynamicMaterialModelEditorOnlyData;
class UMaterialExpression;
class UMaterialFunctionInterface;
struct FDMMaterialBuildState;

/**
 * Base Color, Specular, Opacity, etc
 */
UCLASS(BlueprintType, ClassGroup = "Material Designer", Meta = (DisplayName = "Material Designer Property"))
class DYNAMICMATERIALEDITOR_API UDMMaterialProperty : public UDMMaterialComponent
{
	GENERATED_BODY()

public:
	static UDMMaterialProperty* CreateCustomMaterialPropertyDefaultSubobject(UDynamicMaterialModelEditorOnlyData* InModelEditorOnlyData, 
		EDMMaterialPropertyType InMaterialProperty, const FName& InSubObjName);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDynamicMaterialModelEditorOnlyData* GetMaterialModelEditorOnlyData() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMMaterialPropertyType GetMaterialProperty() const { return MaterialProperty; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	FText GetDescription() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMValueType GetInputConnectorType() const { return InputConnectorType; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FDMMaterialStageConnection& GetInputConnectionMap() const { return InputConnectionMap; }

	FDMMaterialStageConnection& GetInputConnectionMap() { return InputConnectionMap; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterialFunctionInterface* GetOutputProcessor() const { return OutputProcessor; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetOutputProcessor(UMaterialFunctionInterface* InFunction);

	virtual bool IsValidForModel(UDynamicMaterialModelEditorOnlyData& InMaterialModel) const { return true; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	virtual void ResetInputConnectionMap();

	virtual UMaterialExpression* GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const;

	//~ Begin UDMMaterialComponent
	virtual void Update(EDMUpdateType InUpdateType) override;
	virtual bool IsRootComponent() const override { return true; }
	virtual FString GetComponentPathComponent() const override;
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	virtual void PreEditChange(FProperty* InPropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	//~ End UDMMaterialComponent

	void LoadDeprecatedModelData(UDMMaterialProperty* InOldProperty);

protected:
	static UMaterialExpression* CreateConstant(const TSharedRef<FDMMaterialBuildState>& InBuildState, float InDefaultValue);
	static UMaterialExpression* CreateConstant(const TSharedRef<FDMMaterialBuildState>& InBuildState, const FVector2d& InDefaultValue);
	static UMaterialExpression* CreateConstant(const TSharedRef<FDMMaterialBuildState>& InBuildState, const FVector3d& InDefaultValue);
	static UMaterialExpression* CreateConstant(const TSharedRef<FDMMaterialBuildState>& InBuildState, const FVector4d& InDefaultValue);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	EDMMaterialPropertyType MaterialProperty;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	EDMValueType InputConnectorType;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	FDMMaterialStageConnection InputConnectionMap;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TObjectPtr<UMaterialFunctionInterface> OutputProcessor;

	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TObjectPtr<UMaterialFunctionInterface> OutputProcessor_PreUpdate;

	UDMMaterialProperty();
	UDMMaterialProperty(EDMMaterialPropertyType InMaterialProperty, EDMValueType InInputConnectorType);

	void OnOutputProcessorUpdated();
};
