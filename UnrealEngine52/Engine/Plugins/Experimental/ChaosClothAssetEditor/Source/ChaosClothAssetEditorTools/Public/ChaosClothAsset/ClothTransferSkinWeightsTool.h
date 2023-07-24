// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "SingleSelectionTool.h"
#include "InteractiveToolBuilder.h"

#include "ClothTransferSkinWeightsTool.generated.h"

class UChaosClothComponent;
class UClothTransferSkinWeightsTool;
class USkeletalMesh;

UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothTransferSkinWeightsToolProperties : public UObject
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = Files)
	TObjectPtr<USkeletalMesh> SourceMesh;
};

UENUM()
enum class EClothTransferSkinWeightsToolActions
{
	NoAction,
	Transfer
};

UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothTransferSkinWeightsToolActionProperties : public UObject
{
	GENERATED_BODY()
public:

	TWeakObjectPtr<UClothTransferSkinWeightsTool> ParentTool;

	void Initialize(UClothTransferSkinWeightsTool* ParentToolIn) { ParentTool = ParentToolIn; }

	void PostAction(EClothTransferSkinWeightsToolActions Action);

	UFUNCTION(CallInEditor, Category = Actions, meta = (DisplayName = "Transfer weights", DisplayPriority = 1))
	void TransferWeights()
	{
		PostAction(EClothTransferSkinWeightsToolActions::Transfer);
	}

};


UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothTransferSkinWeightsToolBuilder : public UInteractiveToolWithToolTargetsBuilder
{
	GENERATED_BODY()

protected:

	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;

public:

	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};


UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothTransferSkinWeightsTool : public USingleSelectionTool
{
	GENERATED_BODY()
public:

	// UInteractiveTool
	virtual void Setup() override;
	virtual void OnTick(float DeltaTime) override;

private:

	friend class UClothTransferSkinWeightsToolActionProperties;

	UPROPERTY()
	TObjectPtr<UClothTransferSkinWeightsToolProperties> ToolProperties;

	UPROPERTY()
	TObjectPtr<UClothTransferSkinWeightsToolActionProperties> ActionProperties;

	EClothTransferSkinWeightsToolActions PendingAction = EClothTransferSkinWeightsToolActions::NoAction;

	UPROPERTY()
	TObjectPtr<const UChaosClothComponent> ClothComponent;

	void RequestAction(EClothTransferSkinWeightsToolActions ActionType);

	void TransferWeights();

};


