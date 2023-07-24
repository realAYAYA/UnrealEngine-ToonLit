// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothTrainingTool.h"
#include "CoreMinimal.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ModelingOperators.h"
#include "Misc/ScopedSlowTask.h"
#include "ContextObjectStore.h"
#include "InteractiveToolManager.h"
#include "ToolTargetManager.h"
#include "ChaosClothAsset/ClothComponentToolTarget.h"

#define LOCTEXT_NAMESPACE "ClothTrainingTool"

// ------------------- Operator -------------------

class FTrainClothOp : public UE::Geometry::TGenericDataOperator<FSkinnedMeshVertices>
{
public: 

	TObjectPtr<UAnimSequence> AnimSequence;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};


void FTrainClothOp::CalculateResult(FProgressCancel* Progress)
{
	// @@@@@@@@@ TODO: Do the posing, simulation, getting outputs, etc. Check for Progress->Cancelled() regularly if possible.

	FSkinnedMeshVertices OutputVertices;
	
	bool bCancelled = false;

	for (int i = 0; i < 10; ++i)
	{
		FPlatformProcess::Sleep(1.0f);

		if (Progress)
		{
			if (Progress->Cancelled())
			{
				bCancelled = true;
				break;
			}

			OutputVertices.Vertices.Add(FVector3f(i,i,i));

			Progress->AdvanceCurrentScopeProgressBy(0.1f);
		}
	}

	if (!bCancelled)
	{
		SetResult(MakeUnique<FSkinnedMeshVertices>(OutputVertices));
	}
}


// ------------------- Properties -------------------

void UClothTrainingToolActionProperties::PostAction(EClothTrainingToolActions Action)
{
	if (ParentTool.IsValid())
	{
		ParentTool->RequestAction(Action);
	}
}


// ------------------- Builder -------------------

const FToolTargetTypeRequirements& UClothTrainingToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({ 
		UPrimitiveComponentBackedTarget::StaticClass(),
		UClothAssetBackedTarget::StaticClass()
		});
	return TypeRequirements;
}

bool UClothTrainingToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return (SceneState.TargetManager->CountSelectedAndTargetable(SceneState, GetTargetRequirements()) == 1);
}

UInteractiveTool* UClothTrainingToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UClothTrainingTool* NewTool = NewObject<UClothTrainingTool>();
	
	UToolTarget* Target = SceneState.TargetManager->BuildFirstSelectedTargetable(SceneState, GetTargetRequirements());
	NewTool->SetTarget(Target);

	return NewTool;
}

// ------------------- Tool -------------------


void UClothTrainingTool::Setup()
{
	UInteractiveTool::Setup();

	if (const UClothComponentToolTarget* const ClothComponentTarget = Cast<UClothComponentToolTarget>(Target))
	{
		const UChaosClothComponent* const TargetClothComponent = ClothComponentTarget->GetClothComponent();
		ClothComponent = TargetClothComponent;
	}

	ToolProperties = NewObject<UClothTrainingToolProperties>(this);
	AddToolPropertySource(ToolProperties);

	ActionProperties = NewObject<UClothTrainingToolActionProperties>(this);
	ActionProperties->ParentTool = this;
	AddToolPropertySource(ActionProperties);
}

void UClothTrainingTool::RunTraining()
{
	const FText DefaultMessage(LOCTEXT("ClothTrainingMessage", "Training..."));
	
	using FTaskType = UE::Geometry::TModelingOpTask<FTrainClothOp>;
	using FExecuterType = UE::Geometry::FAsyncTaskExecuterWithProgressCancel<FTaskType>;

	TUniquePtr<FTrainClothOp> NewOp = MakeUnique<FTrainClothOp>();
	NewOp->AnimSequence = ToolProperties->AnimationSequence;
	// @@@@@@@@@@@ TODO: Add anything else needed by the Operator


	TUniquePtr<FExecuterType> BackgroundTaskExecuter = MakeUnique<FExecuterType>(MoveTemp(NewOp));
	BackgroundTaskExecuter->StartBackgroundTask();

	FScopedSlowTask SlowTask(1, DefaultMessage);
	SlowTask.MakeDialog(true);

	bool bSuccess = false;
	while (true)
	{
		if (SlowTask.ShouldCancel())
		{
			// Release ownership to the TDeleterTask that is spawned by CancelAndDelete()
			BackgroundTaskExecuter.Release()->CancelAndDelete();
			break;
		}
		if (BackgroundTaskExecuter->IsDone())
		{
			bSuccess = !BackgroundTaskExecuter->GetTask().IsAborted();
			break;
		}
		FPlatformProcess::Sleep(.2); // SlowTask::ShouldCancel will throttle any updates faster than .2 seconds
		float ProgressFrac;
		FText ProgressMessage;
		bool bMadeProgress = BackgroundTaskExecuter->PollProgress(ProgressFrac, ProgressMessage);
		if (bMadeProgress)
		{
			// SlowTask expects progress to be reported before it happens; we work around this by directly updating the progress amount
			SlowTask.CompletedWork = ProgressFrac;
			SlowTask.EnterProgressFrame(0, ProgressMessage);
		}
		else
		{
			SlowTask.TickProgress(); // Still tick the UI when we don't get new progress frames
		}
	}

	if (bSuccess)
	{
		check(BackgroundTaskExecuter != nullptr && BackgroundTaskExecuter->IsDone());
		TUniquePtr<FTrainClothOp> Op = BackgroundTaskExecuter->GetTask().ExtractOperator();

		TUniquePtr<FSkinnedMeshVertices> Result = Op->ExtractResult();

		// @@@@@@@@@@@ TODO: Do something with the result

	}

}


void UClothTrainingTool::OnTick(float DeltaTime)
{
	if (PendingAction != EClothTrainingToolActions::NoAction)
	{
		if (PendingAction == EClothTrainingToolActions::Train)
		{
			RunTraining();
		}
		PendingAction = EClothTrainingToolActions::NoAction;
	}
}


void UClothTrainingTool::RequestAction(EClothTrainingToolActions ActionType)
{
	if (PendingAction != EClothTrainingToolActions::NoAction)
	{
		return;
	}
	PendingAction = ActionType;
}

#undef LOCTEXT_NAMESPACE
