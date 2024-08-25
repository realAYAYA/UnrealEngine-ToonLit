// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageExpression.h"
#include "DMMSETextureSampleBase.generated.h"

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageExpressionTextureSampleBase : public UDMMaterialStageExpression
{
	GENERATED_BODY()

public:
	UDMMaterialStageExpressionTextureSampleBase(const FText& InName, TSubclassOf<UMaterialExpression> InClass);

	//~ Begin UDMMaterialStageSource
	virtual void OnComponentAdded() override;
	virtual bool IsPropertyVisible(FName Property) const override;
	virtual void AddExpressionProperties(const TArray<UMaterialExpression*>& InExpressions) const override;
	virtual int32 GetInnateMaskOutput(int32 InOutputIndex, int32 InOutputChannels) const override;
	virtual int32 GetOutputChannelOverride(int32 InOutputIndex) const override;
	//~ End UDMMaterialStageSource

	//~ Begin UDMMaterialStageThroughput
	virtual bool CanChangeInputType(int32 InInputIndex) const override;
	virtual bool SupportsLayerMaskTextureUVLink() const override { return true; }
	virtual int32 GetLayerMaskTextureUVLinkInputIndex() const override { return 1; }
	virtual void InputUpdated(int32 InInputIndex, EDMUpdateType InUpdateType) override;
	//~ End UDMMaterialStageThroughput

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual bool IsClampTextureEnabled() const { return bClampTexture; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	virtual void SetClampTextureEnabled(bool bInValue);

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter=SetClampTextureEnabled, Category = "Material Designer", 
		meta=(ToolTip="Forces a material rebuild.", LowPriority, NotKeyframeable, AllowPrivateAccess = "true"))
	bool bClampTexture;

	UDMMaterialStageExpressionTextureSampleBase();

	void UpdateMask();
};
