// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraph/AnimNode_AnimNextInterfaceGraph.h"
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "GameFramework/Actor.h"
#include "Animation/NodeMappingContainer.h"
#include "AnimationRuntime.h"
#include "AnimNextInterfaceContext.h"
#include "AnimNextInterfaceParamStorage.h"
#include "AnimNextInterfaceTypes.h"
#include "AnimNextInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_AnimNextInterfaceGraph)

#if WITH_EDITOR
#include "Editor.h"
#endif

FAnimNode_AnimNextInterfaceGraph::FAnimNode_AnimNextInterfaceGraph()
	: FAnimNode_CustomProperty()
	, AnimNextInterfaceGraph(nullptr)
	, LODThreshold(INDEX_NONE)
{
}

FAnimNode_AnimNextInterfaceGraph::~FAnimNode_AnimNextInterfaceGraph()
{
}

void FAnimNode_AnimNextInterfaceGraph::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_CustomProperty::OnInitializeAnimInstance(InProxy, InAnimInstance);

	InitializeProperties(InAnimInstance, GetTargetClass());
}

void FAnimNode_AnimNextInterfaceGraph::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Source.GatherDebugData(DebugData.BranchFlow(1.f));
}

void FAnimNode_AnimNextInterfaceGraph::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Source.Update(Context);

	if (IsLODEnabled(Context.AnimInstanceProxy))
	{
		GetEvaluateGraphExposedInputs().Execute(Context);

		PropagateInputProperties(Context.AnimInstanceProxy->GetAnimInstanceObject());
	}

	FAnimNode_CustomProperty::Update_AnyThread(Context);

	//TRACE_ANIM_NODE_VALUE(Context, TEXT("Class"), *GetNameSafe(ControlRigClass.Get()));
}

void FAnimNode_AnimNextInterfaceGraph::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Source.Initialize(Context);

	FAnimNode_CustomProperty::Initialize_AnyThread(Context);
}

void FAnimNode_AnimNextInterfaceGraph::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_CustomProperty::CacheBones_AnyThread(Context);

	Source.CacheBones(Context);
}

void FAnimNode_AnimNextInterfaceGraph::Evaluate_AnyThread(FPoseContext & Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FPoseContext SourcePose(Output);

	if (Source.GetLinkNode())
	{
		Source.Evaluate(SourcePose);
	}
	else
	{
		SourcePose.ResetToRefPose();
	}

	// This is just a simple test to execute the graph and obtain a float result
	// We have to create the Pose API, in order to be able to pass / receive the animation data
	if (AnimNextInterfaceGraph->GetReturnTypeName() == TNameOf<float>::GetName())
	{
		UE::AnimNext::Interface::FState RootState(1); // TODO : Get rid of num elements ? Let the param itself have it
		UE::AnimNext::Interface::FParamStorage ParamStorage(1, 16, 1);	// TODO : see if we can have state and params together and optional. Also, default size
		UE::AnimNext::Interface::FContext RootContext(0.f, RootState, ParamStorage);
		TScriptInterface<IAnimNextInterface> ScriptInterface = AnimNextInterfaceGraph;

		float ReturnValue = 0.f;
		const bool bOk = UE::AnimNext::Interface::GetDataSafe(ScriptInterface, RootContext, ReturnValue);
	}

	// TODO
	//ExecuteAnimNextInterface (SourcePose);

	Output = SourcePose;

	// evaluate 
	FAnimNode_CustomProperty::Evaluate_AnyThread(Output);
}

void FAnimNode_AnimNextInterfaceGraph::PostSerialize(const FArchive& Ar)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// after compile, we have to reinitialize
	// because it needs new execution code
	// since memory has changed
	if (Ar.IsObjectReferenceCollector())
	{
		if (AnimNextInterfaceGraph)
		{
			//AnimNextInterfaceGraph->Initialize();
		}
	}
}

void FAnimNode_AnimNextInterfaceGraph::InitializeProperties(const UObject* InSourceInstance, UClass* InTargetClass)
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

void FAnimNode_AnimNextInterfaceGraph::PropagateInputProperties(const UObject* InSourceInstance)
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

void FAnimNode_AnimNextInterfaceGraph::HandleObjectsReinstanced_Impl(UObject* InSourceObject, UObject* InTargetObject, const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	FAnimNode_CustomProperty::HandleObjectsReinstanced_Impl(InSourceObject, InTargetObject, OldToNewInstanceMap);
}

#endif
