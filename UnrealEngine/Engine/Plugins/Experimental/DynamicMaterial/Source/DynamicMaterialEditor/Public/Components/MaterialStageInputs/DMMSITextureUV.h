// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageInput.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "DMMSITextureUV.generated.h"

class SWidget;
class UDMMaterialLayerObject;
class UDMMaterialParameter;
class UDMMaterialStage;
class UDMTextureUV;
class UDynamicMaterialModel;
class UMaterialExpressionScalarParameter;
class UMaterialInstanceDynamic;
struct FDMMaterialBuildState;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageInputTextureUV : public UDMMaterialStageInput
{
	GENERATED_BODY()

public:
	static const FString TextureUVPathToken;

	static UDMMaterialStage* CreateStage(UDynamicMaterialModel* InMaterialModel, UDMMaterialLayerObject* InLayer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputTextureUV* ChangeStageSource_UV(UDMMaterialStage* InStage, bool bInDoUpdate);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputTextureUV* ChangeStageInput_UV(UDMMaterialStage* InStage, int32 InInputIdx, int32 InInputChannel, 
		int32 InOutputChannel);

	virtual ~UDMMaterialStageInputTextureUV() override = default;

	virtual FText GetComponentDescription() const override;
	virtual FText GetChannelDescription(const FDMMaterialStageConnectorChannel& Channel) override;

	void Init(UDynamicMaterialModel* InMaterialModel);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMTextureUV* GetTextureUV() { return TextureUV; }

	virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;

	//~ Begin UObject
	virtual bool Modify(bool bInAlwaysMarkDirty = true) override;
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
	//~ End UObject

	//~ Start UDMMaterialComponent
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

protected:
	static UMaterialExpressionScalarParameter* CreateScalarParameter(const TSharedRef<FDMMaterialBuildState>& InBuildState, FName InParamName, float InValue = 0.f);
	static TArray<UMaterialExpression*> CreateTextureUVExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState, UDMTextureUV* InTextureUV);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TObjectPtr<UDMTextureUV> TextureUV;

	UDMMaterialStageInputTextureUV();

	virtual void UpdateOutputConnectors() override;

	void OnTextureUVUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);

	void InitTextureUV();

	void AddEffects(const TSharedRef<FDMMaterialBuildState>& InBuildState, TArray<UMaterialExpression*>& InOutExpressions) const;

	//~ Begin UDMMaterialComponent
	virtual void OnComponentAdded() override;
	virtual void OnComponentRemoved() override;
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	//~ End UDMMaterialComponent
};
