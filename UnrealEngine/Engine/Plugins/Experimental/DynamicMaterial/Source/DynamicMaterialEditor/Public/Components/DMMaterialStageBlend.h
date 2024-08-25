// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMMaterialStageThroughput.h"
#include "UObject/StrongObjectPtr.h"
#include "DMMaterialStageBlend.generated.h"

class FMenuBuilder;
class UDMMaterialLayerObject;
class UDMMaterialStageInput;
class UDMMaterialValueFloat1;
class UMaterial;
struct FDMMaterialBuildState;

/**
 * A node which represents a blend operation.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Stage Blend"))
class DYNAMICMATERIALEDITOR_API UDMMaterialStageBlend : public UDMMaterialStageThroughput
{
	GENERATED_BODY()

public:
	static constexpr int32 InputAlpha = 0;
	static constexpr int32 InputA = 1;
	static constexpr int32 InputB = 2;

	static UDMMaterialStage* CreateStage(TSubclassOf<UDMMaterialStageBlend> InMaterialStageBlendClass, UDMMaterialLayerObject* InLayer = nullptr);

	static const TArray<TStrongObjectPtr<UClass>>& GetAvailableBlends();

	UDMMaterialValueFloat1* GetInputAlpha() const;
	UDMMaterialStageInput* GetInputB() const;

	//~ Begin UDMMaterialStageThroughput
	virtual bool CanInputAcceptType(int32 InputIndex, EDMValueType ValueType) const override;
	virtual void AddDefaultInput(int32 InInputIndex) const override;
	virtual bool CanChangeInput(int32 InputIndex) const override;
	virtual bool CanChangeInputType(int32 InputIndex) const override;
	virtual bool IsInputVisible(int32 InputIndex) const override;
	virtual int32 ResolveInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InputIndex, FDMMaterialStageConnectorChannel& OutChannel,
		TArray<UMaterialExpression*>& OutExpressions) const override;
	//~ End UDMMaterialStageThroughput

	//~ Begin UDMMaterialStageSource
	virtual FText GetStageDescription() const override;
	virtual bool SupportsLayerMaskTextureUVLink() const override { return true; }
	virtual FDMExpressionInput GetLayerMaskLinkTextureUVInputExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const;
	virtual void GetMaskAlphaBlendNode(const TSharedRef<FDMMaterialBuildState>& InBuildState, UMaterialExpression*& OutExpression,
		int32& OutOutputIndex, int32& OutOutputChannel) const override;
	virtual bool UpdateStagePreviewMaterial(UDMMaterialStage* InStage, UMaterial* InPreviewMaterial, UMaterialExpression*& OutMaterialExpression,
		int32& OutputIndex) override;
	//~ End UDMMaterialStageSource

protected:
	static TArray<TStrongObjectPtr<UClass>> Blends;

	static void GenerateBlendList();

	UDMMaterialStageBlend();
	UDMMaterialStageBlend(const FText& InName);

	virtual void UpdatePreviewMaterial(UMaterial* InPreviewMaterial = nullptr) override;
};