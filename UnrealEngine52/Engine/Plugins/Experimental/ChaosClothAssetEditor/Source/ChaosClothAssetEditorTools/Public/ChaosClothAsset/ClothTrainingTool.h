// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "SingleSelectionTool.h"
#include "InteractiveToolBuilder.h"
#include "Animation/AnimSequence.h"
#include "ModelingOperators.h"
#include "MeshOpPreviewHelpers.h"
#include "Misc/SlowTask.h"

#include "ClothTrainingTool.generated.h"

class UChaosClothComponent;
class UClothTrainingTool;


// @@@@@@@@@ TODO: Change this to whatever makes sense for output
struct FSkinnedMeshVertices
{
	TArray<FVector3f> Vertices;
};


UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothTrainingToolProperties : public UObject
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = Files)
	TObjectPtr<UAnimSequence> AnimationSequence;

	UPROPERTY(EditAnywhere, Category = Files)
	FText OutputBufferLocation;
};

UENUM()
enum class EClothTrainingToolActions
{
	NoAction,
	Train
};

UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothTrainingToolActionProperties : public UObject
{
	GENERATED_BODY()
public:

	TWeakObjectPtr<UClothTrainingTool> ParentTool;

	void Initialize(UClothTrainingTool* ParentToolIn) { ParentTool = ParentToolIn; }

	void PostAction(EClothTrainingToolActions Action);

	UFUNCTION(CallInEditor, Category = Actions, meta = (DisplayName = "Begin Training", DisplayPriority = 1))
		void StartTraining()
	{
		PostAction(EClothTrainingToolActions::Train);
	}

};


UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothTrainingToolBuilder : public UInteractiveToolWithToolTargetsBuilder
{
	GENERATED_BODY()

protected:

	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;

public:

	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};


UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothTrainingTool : public USingleSelectionTool
{
	GENERATED_BODY()
public:

	// UInteractiveTool
	virtual void Setup() override;
	virtual void OnTick(float DeltaTime) override;

private:

	friend class UClothTrainingToolActionProperties;

	UPROPERTY()
	TObjectPtr<UClothTrainingToolProperties> ToolProperties;

	UPROPERTY()
	TObjectPtr<UClothTrainingToolActionProperties> ActionProperties;

	EClothTrainingToolActions PendingAction = EClothTrainingToolActions::NoAction;

	UPROPERTY()
	TObjectPtr<const UChaosClothComponent> ClothComponent;

	void RequestAction(EClothTrainingToolActions ActionType);

	void RunTraining();

};


