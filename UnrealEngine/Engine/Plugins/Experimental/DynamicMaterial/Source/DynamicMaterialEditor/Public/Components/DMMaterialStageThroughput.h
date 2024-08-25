// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMMaterialStageSource.h"
#include "UObject/StrongObjectPtr.h"
#include "DMMaterialStageThroughput.generated.h"

class FMenuBuilder;
class UDMMaterialStageInput;
class UMaterialExpression;
struct FDMExpressionInput;
struct FDMMaterialBuildState;

struct FDMExpressionInput
{
	TArray<UMaterialExpression*> OutputExpressions = {};
	int32 OutputIndex = INDEX_NONE;
	int32 OutputChannel = INDEX_NONE;

	bool IsValid()
	{
		return OutputExpressions.IsEmpty() == false && OutputIndex != INDEX_NONE && OutputChannel != INDEX_NONE;
	}
};

/**
 * A node which take one or more inputs and produces an output (e.g. Multiply)
 */
UCLASS(Abstract, BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Stage Throughput"))
class DYNAMICMATERIALEDITOR_API UDMMaterialStageThroughput : public UDMMaterialStageSource
{
	GENERATED_BODY()

public:
	static const TArray<TStrongObjectPtr<UClass>>& GetAvailableThroughputs();

	virtual FText GetComponentDescription() const override { return GetDescription(); }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FText& GetDescription() const { return Name; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsInputRequired() const { return bInputRequired; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool AllowsNestedInputs() const { return bAllowNestedInputs; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const TArray<FDMMaterialStageConnector>& GetInputConnectors() const { return InputConnectors; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	virtual bool CanInputAcceptType(int32 InputIndex, EDMValueType ValueType) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	virtual bool CanInputConnectTo(int32 InputIndex, const FDMMaterialStageConnector& OutputConnector, int32 OutputChannel,
		bool bCheckSingleFloat = false);

	virtual bool CanChangeInput(int32 InputIndex) const;
	virtual bool CanChangeInputType(int32 InputIndex) const;

	virtual bool IsInputVisible(int32 InputIndex) const;

	virtual void ConnectOutputToInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InInputIndex, UMaterialExpression* InSourceExpression,
		int32 InSourceOutputIndex, int32 InSourceOutputChannel);

	/** Returns true if the layer and mask can have their Texture UV linked. */
	virtual bool SupportsLayerMaskTextureUVLink() const { return false; }

	/** Returns the input index for the default implementation of the below method. */
	virtual int32 GetLayerMaskTextureUVLinkInputIndex() const;

	/** 
	 * Returns all the material nodes requires to create this node's Texture UV input. 
	 * If you override this method, you do not need to override GetLayerMaskTextureUVLinkInputIndex.
	 */
	virtual FDMExpressionInput GetLayerMaskLinkTextureUVInputExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const;

	/**
	 * Override this to redirect inputs to other nodes.
	 * Returns the first node in the array by default
	 * --> In [ ]-[ ]-[ ] Out -->
	 */
	virtual UMaterialExpression* GetExpressionForInput(const TArray<UMaterialExpression*>& StageSourceExpressions, int32 InputIdx);

	virtual void AddDefaultInput(int32 InInputIndex) const;

	/** Returns the actual output index of the material expression */
	virtual int32 ResolveInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InputIndex, FDMMaterialStageConnectorChannel& OutChannel,
		TArray<UMaterialExpression*>& OutExpressions) const;

	virtual int32 ResolveLayerMaskTextureUVLinkInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InputIndex, 
		FDMMaterialStageConnectorChannel& OutChannel, TArray<UMaterialExpression*>& OutExpressions) const;

	virtual void InputUpdated(int32 InInputIndex, EDMUpdateType InUpdateType) { }

protected:
	static TArray<TStrongObjectPtr<UClass>> Throughputs;

	static void GenerateThroughputList();

	static int32 ResolveLayerMaskTextureUVLinkInputImpl(const TSharedRef<FDMMaterialBuildState>& InBuildState, const UDMMaterialStageSource* StageSource,
		FDMMaterialStageConnectorChannel& OutChannel, TArray<UMaterialExpression*>& OutExpressions);

	UDMMaterialStageThroughput();
	UDMMaterialStageThroughput(const FText& InName);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	FText Name;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bInputRequired;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bAllowNestedInputs;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TArray<FDMMaterialStageConnector> InputConnectors;

	virtual bool ShouldKeepInput(int32 InInputIdx);

	void ConnectOutputToInput_Internal(const TSharedRef<FDMMaterialBuildState>& InBuildState, UMaterialExpression* TargetExpression,
		int32 InputIndex, UMaterialExpression* SourceExpression, int32 SourceOutputIndex, int32 SourceOutputChannel) const;

	virtual int32 ResolveInputChannel(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InputIndex, int32 ChannelIndex, 
		FDMMaterialStageConnectorChannel& OutChannel, TArray<UMaterialExpression*>& OutExpressions) const;

	virtual void UpdatePreviewMaterial(UMaterial* InPreviewMaterial = nullptr);

	//~ Begin UDMMaterialComponent
	virtual void OnComponentAdded() override;
	//~ End UDMMaterialComponent
};
