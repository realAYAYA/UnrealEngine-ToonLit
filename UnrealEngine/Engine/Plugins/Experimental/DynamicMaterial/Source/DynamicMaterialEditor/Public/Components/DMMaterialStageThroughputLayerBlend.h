// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageThroughput.h"
#include "DMMaterialStageThroughputLayerBlend.generated.h"

class UDMMaterialLayerObject;
class UMaterial;
class UMaterialExpression;

UENUM(BlueprintType)
enum class EAvaColorChannel : uint8
{
	None =  0,
	Red =   1,
	Green = 2,
	Blue =  3,
	Alpha = 4
};

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageThroughputLayerBlend : public UDMMaterialStageThroughput
{
	GENERATED_BODY()

public:
	static constexpr int32 InputPreviousLayer = 0;
	static constexpr int32 InputBaseStage = 1;
	static constexpr int32 InputMaskSource = 2;

	static UDMMaterialStage* CreateStage(UDMMaterialLayerObject* InLayer = nullptr);

	UDMMaterialStageThroughputLayerBlend();

	UDMMaterialStageInput* GetInputMask() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	EAvaColorChannel GetMaskChannelOverride() const;

	UFUNCTION(BlueprintCallable, Category="Material Designer")
	void SetMaskChannelOverride(EAvaColorChannel InMaskChannel);

	//~ Begin UDMMaterialComponent
	virtual void OnComponentAdded() override;
	virtual void Update(EDMUpdateType InUpdateType) override;
	virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	virtual bool IsPropertyVisible(FName InProperty) const override;
	//~ End UDMMaterialComponent

	//~ Begin UDMMaterialStageSource
	virtual FText GetStageDescription() const override;
	virtual bool UpdateStagePreviewMaterial(UDMMaterialStage* InStage, UMaterial* InPreviewMaterial, UMaterialExpression*& OutMaterialExpression,
		int32& OutputIndex) override;
	//~ End UDMMaterialStageSource

	//~ Begin FNotifyHook
	virtual void NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, class FEditPropertyChain* InPropertyThatChanged) override;
	//~ End FNotifyHook

	//~ Begin UDMMaterialStageThroughput
	virtual void AddDefaultInput(int32 InInputIndex) const override;
	virtual bool CanChangeInput(int32 InputIndex) const override;
	virtual bool IsInputVisible(int32 InputIndex) const override;
	virtual int32 ResolveInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InputIndex, FDMMaterialStageConnectorChannel& OutChannel,
		TArray<UMaterialExpression*>& OutExpressions) const override;
	virtual void ConnectOutputToInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InInputIndex, UMaterialExpression* InSourceExpression,
		int32 InSourceOutputIndex, int32 InSourceOutputChannel) override;
	//~ End UDMMaterialStageThroughput

	void GetMaskOutput(const TSharedRef<FDMMaterialBuildState>& InBuildState, UMaterialExpression*& OutExpression, int32& OutOutputIndex, int32& OutOutputChannel) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool UsePremultiplyAlpha() const { return bPremultiplyAlpha; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetPremultiplyAlpha(bool bInValue);

	//~ Begin UObject
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
	//~ End UObject

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Getter=GetMaskChannelOverride, Setter=SetMaskChannelOverride, BlueprintGetter=GetMaskChannelOverride,
		Category = "Material Designer", DisplayName = "ChannelOverride",
		meta=(NotKeyframeable, ToolTip="Changes the output channel of the mask input."))
	mutable EAvaColorChannel MaskChannelOverride;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer", 
		meta=(NotKeyframeable, ToolTip="Additionally multiplies the output of the RGB Stage by the output from this Stage."))
	bool bPremultiplyAlpha;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer",
		meta=(NotKeyframeable))
	bool bIsAlphaOnlyBlend;

	bool bBlockUpdate;

	virtual int32 ResolveMaskInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InputIndex, FDMMaterialStageConnectorChannel& OutChannel,
		TArray<UMaterialExpression*>& OutExpressions) const;
	
	virtual void UpdatePreviewMaterial(UMaterial* InPreviewMaterial = nullptr) override;

	virtual void UpdateAlphaOnlyMaskStatus();
	virtual void OnStageUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);
	virtual void UpdateAlphaOnlyMasks(EDMUpdateType InUpdateType);

	void InitBlendStage();

	void GenerateMainExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const;
	void GeneratePreviewExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const;

	void UpdateLinkedInputStage(EDMUpdateType InUpdateType);

	/* Returns true if there are any outputs on the mask input that have more than 1 channel. */
	bool CanUseMaskChannelOverride() const;

	/* Returns the first output on the mask input that has more than 1 channel. */
	int32 GetDefaultMaskChannelOverrideOutputIndex() const;

	/* Returns true if the given mask output supports more than 1 channel. */
	bool IsValidMaskChannelOverrideOutputIndex(int32 InIndex) const;

	/* Reads the current output setting from the input map. */
	void PullMaskChannelOverride() const;

	/* Takes the override setting and applies it to the input map. */
	void PushMaskChannelOverride();
};
