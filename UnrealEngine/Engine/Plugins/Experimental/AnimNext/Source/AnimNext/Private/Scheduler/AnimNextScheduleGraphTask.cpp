// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextScheduleGraphTask.h"

#include "Scheduler/ScheduleContext.h"
#include "Graph/AnimNextGraph.h"
#include "Context.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "EvaluationVM/EvaluationVM.h"
#include "Graph/AnimNextExecuteContext.h"
#include "Graph/AnimNext_LODPose.h"
#include "AnimNextStats.h"
#include "Logging/StructuredLog.h"
#include "Param/AnimNextParam.h"

DEFINE_STAT(STAT_AnimNext_Task_Graph);

UAnimNextGraph* FAnimNextScheduleGraphTask::GetGraphToRun(UE::AnimNext::FParamStack& ParamStack) const
{
	UAnimNextGraph* GraphToRun = Graph;
	if (GraphToRun == nullptr && DynamicGraph != NAME_None)
	{
		if(const TObjectPtr<UAnimNextGraph>* FoundGraph = ParamStack.GetParamPtr<TObjectPtr<UAnimNextGraph>>(DynamicGraph))
		{
			GraphToRun = *FoundGraph;
		}
	}

	return GraphToRun;
}

void FAnimNextScheduleGraphTask::VerifyRequiredParameters(UAnimNextGraph* InGraphToRun) const
{
	if(SuppliedParametersHash != InGraphToRun->RequiredParametersHash)
	{
		bool bWarningOutput = false;

		for(const FAnimNextParam& RequiredParameter : InGraphToRun->RequiredParameters)
		{
			bool bFound = false;
			bool bFoundCorrectType = true;
			FAnimNextParamType SuppliedParameterType;
			for(const FAnimNextParam& SuppliedParameter : SuppliedParameters)
			{
				if(RequiredParameter.Name == SuppliedParameter.Name)
				{
					if(RequiredParameter.Type != SuppliedParameter.Type)
					{
						SuppliedParameterType = SuppliedParameter.Type;
						bFoundCorrectType = false;
					}
					bFound = true;
					break;
				}
			}

			if(!bWarningOutput && (!bFound || !bFoundCorrectType))
			{
				UE_LOGFMT(LogAnimation, Warning, "AnimNext: Graph {GraphToRun} has different required parameters, it may not run correctly.", InGraphToRun->GetFName());
				bWarningOutput = true;
			}
			
			if(!bFound)
			{
				UE_LOGFMT(LogAnimation, Warning, "    Not Found: {Name}", RequiredParameter.Name);
			}
			else if(!bFoundCorrectType)
			{
				UE_LOGFMT(LogAnimation, Warning, "    Incorrect Type: {Name} ({RequiredType} vs {SuppliedType})", RequiredParameter.Name, RequiredParameter.Type.ToString(), SuppliedParameterType.ToString());
			}
		}
	}
}

void FAnimNextScheduleGraphTask::RunGraph(const UE::AnimNext::FScheduleContext& InContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_Graph);

	using namespace UE::AnimNext;

	FParamStack& ParamStack = FParamStack::Get();

	UAnimNextGraph* GraphToRun = GetGraphToRun(ParamStack);
	if(GraphToRun == nullptr)
	{
		return;
	}

	FScheduleInstanceData& InstanceData = InContext.GetInstanceData();
	FScheduleInstanceData::FGraphCache& GraphCache = InstanceData.GraphCaches[TaskIndex];

	// Check if we are running the correct graph and release (and any term mapping layers) it if not
	if(GraphCache.GraphInstanceData.IsValid() && !GraphCache.GraphInstanceData.UsesGraph(GraphToRun))
	{
		GraphCache.GraphInstanceData.Release();
		GraphCache.GraphTermLayer.Invalidate();
	}

	// Allocate our graph instance data
	if (!GraphCache.GraphInstanceData.IsValid())
	{
		GraphToRun->AllocateInstance(GraphCache.GraphInstanceData, EntryPoint);

		// Only do dynamic verification for dynamic graphs. Static graphs get verified at compile time. 
		if (Graph == nullptr && DynamicGraph != NAME_None)
		{
			VerifyRequiredParameters(GraphToRun);
		}
	}

	const FAnimNextGraphReferencePose* GraphReferencePose = ParamStack.GetParamPtr<FAnimNextGraphReferencePose>(GraphToRun->GetReferencePoseParam());
	if(GraphReferencePose == nullptr || !GraphReferencePose->ReferencePose.IsValid())
	{
		return;
	}

	const int32* GraphLODLevel = ParamStack.GetParamPtr<int32>(GraphToRun->GetCurrentLODParam());
	if(GraphLODLevel == nullptr)
	{
		return;
	}

	// Check and allocate remapped term layer
	if(!GraphCache.GraphTermLayer.IsValid())
	{
		TConstArrayView<FScheduleTerm> GraphTerms = GraphToRun->GetTerms();
		check(Terms.Num() == GraphTerms.Num());

		TMap<FName, FName> Mapping;
		Mapping.Reserve(GraphTerms.Num());
		for(int32 TermIndex = 0; TermIndex < Terms.Num(); ++TermIndex)
		{
			uint32 IntermediateTermIndex = Terms[TermIndex];
			const FPropertyBagPropertyDesc& PropertyDesc = InstanceData.IntermediatesData.GetPropertyBagStruct()->GetPropertyDescs()[IntermediateTermIndex];
			Mapping.Add(PropertyDesc.Name, GraphTerms[TermIndex].GetName());
		}

		GraphCache.GraphTermLayer = FParamStack::MakeRemappedLayer(InstanceData.IntermediatesLayer, Mapping);
	}

	// TODO: This should not be fixed at arg 0, we should define this in the graph asset
	FAnimNextGraphLODPose* OutputPose = GraphCache.GraphTermLayer.GetMutableParamPtr<FAnimNextGraphLODPose>(GraphToRun->GetTerms()[0].GetId());
	if(OutputPose == nullptr)
	{
		return;
	}
	
	const UE::AnimNext::FReferencePose& RefPose = GraphReferencePose->ReferencePose.GetRef<UE::AnimNext::FReferencePose>();

	// Create or update our result pose
	// TODO: Currently forcing additive flag to false here
	if (OutputPose->LODPose.ShouldPrepareForLOD(RefPose, *GraphLODLevel, false))
	{
		OutputPose->LODPose.PrepareForLOD(RefPose, *GraphLODLevel, true, false);
	}

	check(OutputPose->LODPose.LODLevel == *GraphLODLevel);

	// Internally we use memstack allocation, so we need a mark here
	FMemStack& MemStack = FMemStack::Get();
	FMemMark MemMark(MemStack);

	// We allocate a dummy buffer to trigger the allocation of a large chunk if this is the first mark
	// This reduces churn internally by avoiding a chunk to be repeatedly allocated and freed as we push/pop marks
	MemStack.Alloc(size_t(FPageAllocator::SmallPageSize) + 1, 16);

	UE::AnimNext::UpdateGraph(GraphCache.GraphInstanceData, InContext.GetDeltaTime());

	{
		const FEvaluationProgram EvaluationProgram = UE::AnimNext::EvaluateGraph(GraphCache.GraphInstanceData);

		FEvaluationVM EvaluationVM(EEvaluationFlags::All, RefPose, *GraphLODLevel);
		bool bHasValidOutput = false;

		if (!EvaluationProgram.IsEmpty())
		{
			EvaluationProgram.Execute(EvaluationVM);

			TUniquePtr<FKeyframeState> EvaluatedKeyframe;
			if (EvaluationVM.PopValue(KEYFRAME_STACK_NAME, EvaluatedKeyframe))
			{
				OutputPose->LODPose.CopyFrom(EvaluatedKeyframe->Pose);
				bHasValidOutput = true;
			}
		}

		if (!bHasValidOutput)
		{
			// We need to output a valid pose, generate one
			FKeyframeState ReferenceKeyframe = EvaluationVM.MakeReferenceKeyframe(false);
			OutputPose->LODPose.CopyFrom(ReferenceKeyframe.Pose);
		}
	}
}
