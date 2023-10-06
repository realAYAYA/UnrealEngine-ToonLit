// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsCompletions.h"

#include "LearningAgentsTrainer.h"
#include "LearningAgentsManager.h"
#include "LearningArray.h"
#include "LearningArrayMap.h"
#include "LearningCompletion.h"
#include "LearningCompletionObject.h"
#include "LearningLog.h"

#include "GameFramework/Actor.h"

namespace UE::Learning::Agents::Completions::Private
{
	template<typename CompletionUObject, typename CompletionFObject, typename... InArgTypes>
	CompletionUObject* AddCompletion(ULearningAgentsTrainer* InAgentTrainer, const FName Name, const TCHAR* FunctionName, InArgTypes&& ...Args)
	{
		if (!InAgentTrainer)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: InAgentTrainer is nullptr."), FunctionName);
			return nullptr;
		}

		if (!InAgentTrainer->HasAgentManager())
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Must be attached to a LearningAgentsManager Actor."), *InAgentTrainer->GetName());
			return nullptr;
		}

		const FName UniqueName = MakeUniqueObjectName(InAgentTrainer, CompletionUObject::StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);

		CompletionUObject* Completion = NewObject<CompletionUObject>(InAgentTrainer, UniqueName);
		Completion->Init(InAgentTrainer->GetAgentManager()->GetMaxAgentNum());
		Completion->AgentTrainer = InAgentTrainer;
		Completion->CompletionObject = MakeShared<CompletionFObject>(
			Completion->GetFName(),
			InAgentTrainer->GetAgentManager()->GetInstanceData().ToSharedRef(),
			InAgentTrainer->GetAgentManager()->GetMaxAgentNum(),
			Forward<InArgTypes>(Args)...);

		InAgentTrainer->AddCompletion(Completion, Completion->CompletionObject.ToSharedRef());

		return Completion;
	}
}

//------------------------------------------------------------------

void ULearningAgentsCompletion::Init(const int32 MaxAgentNum)
{
	AgentIteration.SetNumUninitialized({ MaxAgentNum });
	UE::Learning::Array::Set<1, uint64>(AgentIteration, INDEX_NONE);
}

void ULearningAgentsCompletion::OnAgentsAdded(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentIteration, 0, AgentIds);
}

void ULearningAgentsCompletion::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentIteration, INDEX_NONE, AgentIds);
}

void ULearningAgentsCompletion::OnAgentsReset(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentIteration, 0, AgentIds);
}

uint64 ULearningAgentsCompletion::GetAgentIteration(const int32 AgentId) const
{
	return AgentIteration[AgentId];
}

//------------------------------------------------------------------

UConditionalCompletion* UConditionalCompletion::AddConditionalCompletion(ULearningAgentsTrainer* InAgentTrainer, const FName Name, const ELearningAgentsCompletion InCompletionMode)
{
	return UE::Learning::Agents::Completions::Private::AddCompletion<UConditionalCompletion, UE::Learning::FConditionalCompletion>(InAgentTrainer, Name, TEXT("AddConditionalCompletion"), UE::Learning::Agents::GetCompletionMode(InCompletionMode));
}

void UConditionalCompletion::SetConditionalCompletion(const int32 AgentId, const bool bIsComplete)
{
	if (!AgentTrainer->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	CompletionObject->InstanceData->View(CompletionObject->ConditionHandle)[AgentId] = bIsComplete;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UConditionalCompletion::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UConditionalCompletion::VisualLog);

	const TLearningArrayView<1, const bool> ConditionView = CompletionObject->InstanceData->ConstView(CompletionObject->ConditionHandle);
	const TLearningArrayView<1, const UE::Learning::ECompletionMode> CompletionView = CompletionObject->InstanceData->ConstView(CompletionObject->CompletionHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(AgentTrainer->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nCondition: %s\nCompletion: %s"),
				Instance,
				ConditionView[Instance] ? TEXT("true") : TEXT("false"),
				UE::Learning::Completion::CompletionModeString(CompletionView[Instance]));
		}
	}
}
#endif

//------------------------------------------------------------------

UTimeElapsedCompletion* UTimeElapsedCompletion::AddTimeElapsedCompletion(ULearningAgentsTrainer* InAgentTrainer, const FName Name, const float Threshold, const ELearningAgentsCompletion InCompletionMode)
{
	return UE::Learning::Agents::Completions::Private::AddCompletion<UTimeElapsedCompletion, UE::Learning::FTimeElapsedCompletion>(InAgentTrainer, Name, TEXT("AddTimeElapsedCompletion"), Threshold, UE::Learning::Agents::GetCompletionMode(InCompletionMode));
}

void UTimeElapsedCompletion::SetTimeElapsedCompletion(const int32 AgentId, const float Time)
{
	if (!AgentTrainer->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	CompletionObject->InstanceData->View(CompletionObject->TimeHandle)[AgentId] = Time;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UTimeElapsedCompletion::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UTimeElapsedCompletion::VisualLog);

	const TLearningArrayView<1, const float> TimeView = CompletionObject->InstanceData->ConstView(CompletionObject->TimeHandle);
	const TLearningArrayView<1, const float> ThresholdView = CompletionObject->InstanceData->ConstView(CompletionObject->ThresholdHandle);
	const TLearningArrayView<1, const UE::Learning::ECompletionMode> CompletionView = CompletionObject->InstanceData->ConstView(CompletionObject->CompletionHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(AgentTrainer->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nThreshold: [%6.3f]\nTime: [%6.3f]\nCompletion: %s"),
				Instance,
				ThresholdView[Instance],
				TimeView[Instance],
				UE::Learning::Completion::CompletionModeString(CompletionView[Instance]));
		}
	}
}
#endif

//------------------------------------------------------------------

UPlanarPositionDifferenceCompletion* UPlanarPositionDifferenceCompletion::AddPlanarPositionDifferenceCompletion(
	ULearningAgentsTrainer* InAgentTrainer,
	const FName Name,
	const float Threshold,
	const ELearningAgentsCompletion InCompletionMode,
	const FVector Axis0,
	const FVector Axis1)
{
	return UE::Learning::Agents::Completions::Private::AddCompletion<UPlanarPositionDifferenceCompletion, UE::Learning::FPlanarPositionDifferenceCompletion>(
		InAgentTrainer,
		Name,
		TEXT("AddPlanarPositionDifferenceCompletion"),
		1,
		Threshold,
		UE::Learning::Agents::GetCompletionMode(InCompletionMode),
		Axis0.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector),
		Axis1.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
}

void UPlanarPositionDifferenceCompletion::SetPlanarPositionDifferenceCompletion(const int32 AgentId, const FVector Position0, const FVector Position1)
{
	if (!AgentTrainer->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	CompletionObject->InstanceData->View(CompletionObject->Position0Handle)[AgentId][0] = Position0;
	CompletionObject->InstanceData->View(CompletionObject->Position1Handle)[AgentId][0] = Position1;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPlanarPositionDifferenceCompletion::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPlanarPositionDifferenceCompletion::VisualLog);

	const TLearningArrayView<2, const FVector> Position0View = CompletionObject->InstanceData->ConstView(CompletionObject->Position0Handle);
	const TLearningArrayView<2, const FVector> Position1View = CompletionObject->InstanceData->ConstView(CompletionObject->Position1Handle);
	const TLearningArrayView<1, const float> ThresholdView = CompletionObject->InstanceData->ConstView(CompletionObject->ThresholdHandle);
	const TLearningArrayView<1, const UE::Learning::ECompletionMode> CompletionView = CompletionObject->InstanceData->ConstView(CompletionObject->CompletionHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(AgentTrainer->GetAgent(Instance)))
		{
			const FVector Position0 = Position0View[Instance][0];
			const FVector Position1 = Position1View[Instance][0];

			const FVector PlanarPosition0 = FVector(CompletionObject->Axis0.Dot(Position0), CompletionObject->Axis1.Dot(Position0), 0.0f);
			const FVector PlanarPosition1 = FVector(CompletionObject->Axis0.Dot(Position1), CompletionObject->Axis1.Dot(Position1), 0.0f);

			UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
				Position0,
				10.0f,
				VisualLogColor.ToFColor(true),
				TEXT("Position0: [% 6.1f % 6.1f % 6.1f]\nPlanar Position0: [% 6.1f % 6.1f]"),
				Position0.X, Position0.Y, Position0.Z,
				PlanarPosition0.X, PlanarPosition0.Y);

			UE_LEARNING_AGENTS_VLOG_PLANE(this, LogLearning, Display,
				Position0,
				FQuat::Identity,
				CompletionObject->Axis0,
				CompletionObject->Axis1,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
				Position1,
				10.0f,
				VisualLogColor.ToFColor(true),
				TEXT("Position1: [% 6.1f % 6.1f % 6.1f]\nPlanar Position1: [% 6.1f % 6.1f]"),
				Position1.X, Position1.Y, Position1.Z,
				PlanarPosition1.X, PlanarPosition1.Y);

			UE_LEARNING_AGENTS_VLOG_PLANE(this, LogLearning, Display,
				Position1,
				FQuat::Identity,
				CompletionObject->Axis0,
				CompletionObject->Axis1,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
				Position0,
				Position1,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nDistance: [% 6.3f]\nPlanar Distance: [% 6.3f]\nThreshold: [% 6.2f]\nCompletion: %s"),
				Instance,
				FVector::Distance(Position0, Position1),
				FVector::Distance(PlanarPosition0, PlanarPosition1),
				ThresholdView[Instance],
				UE::Learning::Completion::CompletionModeString(CompletionView[Instance]));
		}
	}
}
#endif


UPlanarPositionSimilarityCompletion* UPlanarPositionSimilarityCompletion::AddPlanarPositionSimilarityCompletion(
	ULearningAgentsTrainer* InAgentTrainer,
	const FName Name,
	const float Threshold,
	const ELearningAgentsCompletion InCompletionMode,
	const FVector Axis0,
	const FVector Axis1)
{
	return UE::Learning::Agents::Completions::Private::AddCompletion<UPlanarPositionSimilarityCompletion, UE::Learning::FPlanarPositionSimilarityCompletion>(
		InAgentTrainer,
		Name,
		TEXT("AddPlanarPositionSimilarityCompletion"),
		1,
		Threshold,
		UE::Learning::Agents::GetCompletionMode(InCompletionMode),
		Axis0.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector),
		Axis1.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
}

void UPlanarPositionSimilarityCompletion::SetPlanarPositionSimilarityCompletion(const int32 AgentId, const FVector Position0, const FVector Position1)
{
	if (!AgentTrainer->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	CompletionObject->InstanceData->View(CompletionObject->Position0Handle)[AgentId][0] = Position0;
	CompletionObject->InstanceData->View(CompletionObject->Position1Handle)[AgentId][0] = Position1;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPlanarPositionSimilarityCompletion::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPlanarPositionSimilarityCompletion::VisualLog);

	const TLearningArrayView<2, const FVector> Position0View = CompletionObject->InstanceData->ConstView(CompletionObject->Position0Handle);
	const TLearningArrayView<2, const FVector> Position1View = CompletionObject->InstanceData->ConstView(CompletionObject->Position1Handle);
	const TLearningArrayView<1, const float> ThresholdView = CompletionObject->InstanceData->ConstView(CompletionObject->ThresholdHandle);
	const TLearningArrayView<1, const UE::Learning::ECompletionMode> CompletionView = CompletionObject->InstanceData->ConstView(CompletionObject->CompletionHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(AgentTrainer->GetAgent(Instance)))
		{
			const FVector Position0 = Position0View[Instance][0];
			const FVector Position1 = Position1View[Instance][0];

			const FVector PlanarPosition0 = FVector(CompletionObject->Axis0.Dot(Position0), CompletionObject->Axis1.Dot(Position0), 0.0f);
			const FVector PlanarPosition1 = FVector(CompletionObject->Axis0.Dot(Position1), CompletionObject->Axis1.Dot(Position1), 0.0f);

			UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
				Position0,
				10.0f,
				VisualLogColor.ToFColor(true),
				TEXT("Position0: [% 6.1f % 6.1f % 6.1f]\nPlanar Position0: [% 6.1f % 6.1f]"),
				Position0.X, Position0.Y, Position0.Z,
				PlanarPosition0.X, PlanarPosition0.Y);

			UE_LEARNING_AGENTS_VLOG_PLANE(this, LogLearning, Display,
				Position0,
				FQuat::Identity,
				CompletionObject->Axis0,
				CompletionObject->Axis1,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
				Position1,
				10.0f,
				VisualLogColor.ToFColor(true),
				TEXT("Position1: [% 6.1f % 6.1f % 6.1f]\nPlanar Position1: [% 6.1f % 6.1f]"),
				Position1.X, Position1.Y, Position1.Z,
				PlanarPosition1.X, PlanarPosition1.Y);

			UE_LEARNING_AGENTS_VLOG_PLANE(this, LogLearning, Display,
				Position1,
				FQuat::Identity,
				CompletionObject->Axis0,
				CompletionObject->Axis1,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
				Position0,
				Position1,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nDistance: [% 6.3f]\nPlanar Distance: [% 6.3f]\nThreshold: [% 6.2f]\nCompletion: %s"),
				Instance,
				FVector::Distance(Position0, Position1),
				FVector::Distance(PlanarPosition0, PlanarPosition1),
				ThresholdView[Instance],
				UE::Learning::Completion::CompletionModeString(CompletionView[Instance]));
		}
	}
}
#endif
