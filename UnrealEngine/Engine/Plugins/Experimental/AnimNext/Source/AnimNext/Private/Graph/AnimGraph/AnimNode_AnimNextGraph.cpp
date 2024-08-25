// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimGraph/AnimNode_AnimNextGraph.h"
#include "DataRegistry.h"
#include "ReferencePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "GameFramework/Actor.h"
#include "AnimationRuntime.h"
#include "DataRegistry.h"
#include "GenerationTools.h"
#include "ReferencePose.h"
#include "Graph/AnimNext_LODPose.h"
#include "Graph/AnimNextExecuteContext.h"
#include "Engine/SkeletalMesh.h"
#include "BoneContainer.h"
#include "Param/ParamStack.h"
#include "AnimGraphParamStackScope.h"
#include "Scheduler/ScheduleContext.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "EvaluationVM/EvaluationVM.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_AnimNextGraph)

#if WITH_EDITOR
#include "Editor.h"
#endif

TAutoConsoleVariable<int32> CVarAnimNextForceAnimBP(TEXT("a.AnimNextForceAnimBP"), 0, TEXT("If != 0, then we use the input pose of the AnimNext AnimBP node instead of the AnimNext graph."));

FAnimNode_AnimNextGraph::FAnimNode_AnimNextGraph()
	: FAnimNode_CustomProperty()
	, AnimNextGraph(nullptr)
	, LODThreshold(INDEX_NONE)
{
}

void FAnimNode_AnimNextGraph::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_CustomProperty::OnInitializeAnimInstance(InProxy, InAnimInstance);

	InitializeProperties(InAnimInstance, GetTargetClass());
}

void FAnimNode_AnimNextGraph::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	SourceLink.GatherDebugData(DebugData.BranchFlow(1.f));
}

void FAnimNode_AnimNextGraph::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	using namespace UE::AnimNext;

	SourceLink.Update(Context);

	if (IsLODEnabled(Context.AnimInstanceProxy) && !CVarAnimNextForceAnimBP.GetValueOnAnyThread() && GraphInstance.IsValid())
	{
		GetEvaluateGraphExposedInputs().Execute(Context);

		PropagateInputProperties(Context.AnimInstanceProxy->GetAnimInstanceObject());

		const int32 LODLevel = Context.AnimInstanceProxy->GetLODLevel();

		UE::AnimNext::FAnimGraphParamStackScope Scope(Context);

		FParamStack& ParamStack = FParamStack::Get();
		FParamStack::FPushedLayerHandle LayerHandle = ParamStack.PushValues(
			AnimNextGraph->GetCurrentLODParam(), LODLevel
		);

		UE::AnimNext::UpdateGraph(GraphInstance, Context.GetDeltaTime());

		ParamStack.PopLayer(LayerHandle);
	}

	FAnimNode_CustomProperty::Update_AnyThread(Context);

	//TRACE_ANIM_NODE_VALUE(Context, TEXT("Class"), *GetNameSafe(ControlRigClass.Get()));
}

void FAnimNode_AnimNextGraph::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	using namespace UE::AnimNext;

	SourceLink.Initialize(Context);

	if (!GraphInstance.IsValid() && AnimNextGraph)
	{
		// If we don't have an instance yet, create one
		UE::AnimNext::FAnimGraphParamStackScope Scope(Context);

		// Populate our param stack since our instance data might need it during construction
		const int32 LODLevel = Context.AnimInstanceProxy->GetLODLevel();

		FParamStack& ParamStack = FParamStack::Get();
		FParamStack::FPushedLayerHandle LayerHandle = ParamStack.PushValues(
			AnimNextGraph->GetCurrentLODParam(), LODLevel
		);

		AnimNextGraph->AllocateInstance(GraphInstance);

		ParamStack.PopLayer(LayerHandle);
	}

	FAnimNode_CustomProperty::Initialize_AnyThread(Context);
}

void FAnimNode_AnimNextGraph::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_CustomProperty::CacheBones_AnyThread(Context);

	SourceLink.CacheBones(Context);
}

void FAnimNode_AnimNextGraph::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	using namespace UE::AnimNext;

	if (!CVarAnimNextForceAnimBP.GetValueOnAnyThread() && GraphInstance.IsValid())
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Output.AnimInstanceProxy->GetSkelMeshComponent();
		check(SkeletalMeshComponent != nullptr);

		FDataHandle RefPoseHandle = FDataRegistry::Get()->GetOrGenerateReferencePose(SkeletalMeshComponent);
		FAnimNextGraphReferencePose GraphReferencePose(RefPoseHandle);

		const int32 LODLevel = Output.AnimInstanceProxy->GetLODLevel();

		const UE::AnimNext::FReferencePose& RefPose = RefPoseHandle.GetRef<UE::AnimNext::FReferencePose>();
		FAnimNextGraphLODPose ResultPose(FLODPoseHeap(RefPose, LODLevel, true, Output.ExpectsAdditivePose()));

		FAnimGraphParamStackScope Scope(Output);
		FParamStack& ParamStack = FParamStack::Get();
		FParamStack::FPushedLayerHandle LayerHandle = ParamStack.PushValues(
			AnimNextGraph->GetReferencePoseParam(), GraphReferencePose,
			AnimNextGraph->GetCurrentLODParam(), LODLevel
		);

		{
			const FEvaluationProgram EvaluationProgram = UE::AnimNext::EvaluateGraph(GraphInstance);

			FEvaluationVM EvaluationVM(EEvaluationFlags::All, RefPose, LODLevel);
			bool bHasValidOutput = false;

			if (!EvaluationProgram.IsEmpty())
			{
				EvaluationProgram.Execute(EvaluationVM);

				TUniquePtr<FKeyframeState> EvaluatedKeyframe;
				if (EvaluationVM.PopValue(KEYFRAME_STACK_NAME, EvaluatedKeyframe))
				{
					ResultPose.LODPose.CopyFrom(EvaluatedKeyframe->Pose);
					bHasValidOutput = true;
				}
			}

			if (!bHasValidOutput)
			{
				// We need to output a valid pose, generate one
				FKeyframeState ReferenceKeyframe = EvaluationVM.MakeReferenceKeyframe(Output.ExpectsAdditivePose());
				ResultPose.LODPose.CopyFrom(ReferenceKeyframe.Pose);
			}
		}

		FGenerationTools::RemapPose(ResultPose.LODPose, Output);

		ParamStack.PopLayer(LayerHandle);
	}
	else
	{
		if (SourceLink.GetLinkNode())
		{
			SourceLink.Evaluate(Output);
		}
	}

	FAnimNode_CustomProperty::Evaluate_AnyThread(Output);
}

void FAnimNode_AnimNextGraph::PostSerialize(const FArchive& Ar)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// after compile, we have to reinitialize
	// because it needs new execution code
	// since memory has changed
	if (Ar.IsObjectReferenceCollector())
	{
		if (AnimNextGraph)
		{
			//AnimNextGraph->Initialize();
		}
	}
}

void FAnimNode_AnimNextGraph::InitializeProperties(const UObject* InSourceInstance, UClass* InTargetClass)
{
	// Build property lists
	SourceProperties.Reset(SourcePropertyNames.Num());
	DestProperties.Reset(SourcePropertyNames.Num());

	check(SourcePropertyNames.Num() == DestPropertyNames.Num());

	for (int32 Idx = 0; Idx < SourcePropertyNames.Num(); ++Idx)
	{
		FName& SourceName = SourcePropertyNames[Idx];
		UClass* SourceClass = InSourceInstance->GetClass();

		FProperty* SourceProperty = FindFProperty<FProperty>(SourceClass, SourceName);
		SourceProperties.Add(SourceProperty);
		DestProperties.Add(nullptr);
	}
}

void FAnimNode_AnimNextGraph::PropagateInputProperties(const UObject* InSourceInstance)
{
	if (InSourceInstance)
	{
		// Assign value to the properties exposed as pins
		for (int32 PropIdx = 0; PropIdx < SourceProperties.Num(); ++PropIdx)
		{
		}
	}
}


#if WITH_EDITOR

void FAnimNode_AnimNextGraph::HandleObjectsReinstanced_Impl(UObject* InSourceObject, UObject* InTargetObject, const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	FAnimNode_CustomProperty::HandleObjectsReinstanced_Impl(InSourceObject, InTargetObject, OldToNewInstanceMap);
}

#endif
