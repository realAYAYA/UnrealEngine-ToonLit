// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsRewards.h"

#include "LearningAgentsTrainer.h"
#include "LearningAgentsManager.h"
#include "LearningArray.h"
#include "LearningArrayMap.h"
#include "LearningRewardObject.h"
#include "LearningLog.h"

#include "GameFramework/Actor.h"

namespace UE::Learning::Agents::Rewards::Private
{
	template<typename RewardUObject, typename RewardFObject, typename... InArgTypes>
	RewardUObject* AddReward(ULearningAgentsTrainer* InAgentTrainer, const FName Name, const TCHAR* FunctionName, InArgTypes&& ...Args)
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

		const FName UniqueName = MakeUniqueObjectName(InAgentTrainer, RewardUObject::StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);

		RewardUObject* Reward = NewObject<RewardUObject>(InAgentTrainer, UniqueName);
		Reward->Init(InAgentTrainer->GetAgentManager()->GetMaxAgentNum());
		Reward->AgentTrainer = InAgentTrainer;
		Reward->RewardObject = MakeShared<RewardFObject>(
			Reward->GetFName(),
			InAgentTrainer->GetAgentManager()->GetInstanceData().ToSharedRef(),
			InAgentTrainer->GetAgentManager()->GetMaxAgentNum(),
			Forward<InArgTypes>(Args)...);

		InAgentTrainer->AddReward(Reward, Reward->RewardObject.ToSharedRef());

		return Reward;
	}
}

//------------------------------------------------------------------

void ULearningAgentsReward::Init(const int32 MaxAgentNum)
{
	AgentIteration.SetNumUninitialized({ MaxAgentNum });
	UE::Learning::Array::Set<1, uint64>(AgentIteration, INDEX_NONE);
}

void ULearningAgentsReward::OnAgentsAdded(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentIteration, 0, AgentIds);
}

void ULearningAgentsReward::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentIteration, INDEX_NONE, AgentIds);
}

void ULearningAgentsReward::OnAgentsReset(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentIteration, 0, AgentIds);
}

uint64 ULearningAgentsReward::GetAgentIteration(const int32 AgentId) const
{
	return AgentIteration[AgentId];
}

//------------------------------------------------------------------

UFloatReward* UFloatReward::AddFloatReward(ULearningAgentsTrainer* InAgentTrainer, const FName Name, const float Weight)
{
	return UE::Learning::Agents::Rewards::Private::AddReward<UFloatReward, UE::Learning::FFloatReward>(InAgentTrainer, Name, TEXT("AddFloatReward"), Weight);
}

void UFloatReward::SetFloatReward(const int32 AgentId, const float Reward)
{
	if (!AgentTrainer->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	RewardObject->InstanceData->View(RewardObject->ValueHandle)[AgentId] = Reward;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UFloatReward::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UFloatReward::VisualLog);

	const TLearningArrayView<1, const float> ValueView = RewardObject->InstanceData->ConstView(RewardObject->ValueHandle);
	const TLearningArrayView<1, const float> WeightView = RewardObject->InstanceData->ConstView(RewardObject->WeightHandle);
	const TLearningArrayView<1, const float> RewardView = RewardObject->InstanceData->ConstView(RewardObject->RewardHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(AgentTrainer->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nWeight: [% 6.2f]\nValue: [% 6.2f]\nReward: [% 6.3f]"),
				Instance,
				WeightView[Instance],
				ValueView[Instance],
				RewardView[Instance]);
		}
	}
}
#endif

//------------------------------------------------------------------

UConditionalReward* UConditionalReward::AddConditionalReward(ULearningAgentsTrainer* InAgentTrainer, const FName Name, const float Value)
{
	return UE::Learning::Agents::Rewards::Private::AddReward<UConditionalReward, UE::Learning::FConditionalConstantReward>(InAgentTrainer, Name, TEXT("AddConditionalReward"), Value);
}

void UConditionalReward::SetConditionalReward(const int32 AgentId, const bool bCondition)
{
	if (!AgentTrainer->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	RewardObject->InstanceData->View(RewardObject->ConditionHandle)[AgentId] = bCondition;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UConditionalReward::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UBoolReward::VisualLog);

	const TLearningArrayView<1, const bool> ConditionView = RewardObject->InstanceData->ConstView(RewardObject->ConditionHandle);
	const TLearningArrayView<1, const float> RewardView = RewardObject->InstanceData->ConstView(RewardObject->RewardHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(AgentTrainer->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nValue: [% 6.2f]\nCondition: %s\nReward: [% 6.3f]"),
				Instance,
				RewardObject->Value,
				ConditionView[Instance] ? TEXT("true") : TEXT("false"),
				RewardView[Instance]);
		}
	}
}
#endif

//------------------------------------------------------------------

UScalarVelocityReward* UScalarVelocityReward::AddScalarVelocityReward(ULearningAgentsTrainer* InAgentTrainer, const FName Name, const float Weight, const float Scale)
{
	return UE::Learning::Agents::Rewards::Private::AddReward<UScalarVelocityReward, UE::Learning::FScalarVelocityReward>(InAgentTrainer, Name, TEXT("AddScalarVelocityReward"), Weight, Scale);
}

void UScalarVelocityReward::SetScalarVelocityReward(int32 AgentId, float Velocity)
{
	if (!AgentTrainer->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	RewardObject->InstanceData->View(RewardObject->VelocityHandle)[AgentId] = Velocity;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UScalarVelocityReward::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UScalarVelocityReward::VisualLog);

	const TLearningArrayView<1, const float> VelocityView = RewardObject->InstanceData->ConstView(RewardObject->VelocityHandle);
	const TLearningArrayView<1, const float> WeightView = RewardObject->InstanceData->ConstView(RewardObject->WeightHandle);
	const TLearningArrayView<1, const float> ScaleView = RewardObject->InstanceData->ConstView(RewardObject->ScaleHandle);
	const TLearningArrayView<1, const float> RewardView = RewardObject->InstanceData->ConstView(RewardObject->RewardHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<const AActor>(AgentTrainer->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nWeight: [% 6.2f]\nScale: [% 6.2f]\nVelocity: [% 6.2f]\nReward: [% 6.3f]"),
				Instance,
				WeightView[Instance],
				ScaleView[Instance],
				VelocityView[Instance],
				RewardView[Instance]);
		}
	}
}
#endif

ULocalDirectionalVelocityReward* ULocalDirectionalVelocityReward::AddLocalDirectionalVelocityReward(ULearningAgentsTrainer* InAgentTrainer, const FName Name, const float Weight, const float Scale, const FVector Axis)
{
	return UE::Learning::Agents::Rewards::Private::AddReward<ULocalDirectionalVelocityReward, UE::Learning::FLocalDirectionalVelocityReward>(InAgentTrainer, Name, TEXT("AddLocalDirectionalVelocityReward"), Weight, Scale, Axis.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));
}

void ULocalDirectionalVelocityReward::SetLocalDirectionalVelocityReward(const int32 AgentId, const FVector Velocity, const FRotator RelativeRotation)
{
	if (!AgentTrainer->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	RewardObject->InstanceData->View(RewardObject->VelocityHandle)[AgentId] = Velocity;
	RewardObject->InstanceData->View(RewardObject->RelativeRotationHandle)[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void ULocalDirectionalVelocityReward::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULocalDirectionalVelocityReward::VisualLog);

	const TLearningArrayView<1, const FVector> VelocityView = RewardObject->InstanceData->View(RewardObject->VelocityHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = RewardObject->InstanceData->View(RewardObject->RelativeRotationHandle);
	const TLearningArrayView<1, const float> WeightView = RewardObject->InstanceData->ConstView(RewardObject->WeightHandle);
	const TLearningArrayView<1, const float> ScaleView = RewardObject->InstanceData->ConstView(RewardObject->ScaleHandle);
	const TLearningArrayView<1, const float> RewardView = RewardObject->InstanceData->ConstView(RewardObject->RewardHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(AgentTrainer->GetAgent(Instance)))
		{
			const FVector Velocity = VelocityView[Instance];
			const FQuat RelativeRotation = RelativeRotationView[Instance];
			const FVector LocalVelocity = RelativeRotation.UnrotateVector(Velocity);
			const FVector Direction = RelativeRotation.RotateVector(RewardObject->Axis);

			UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
				Actor->GetActorLocation(),
				Actor->GetActorLocation() + Velocity,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation() + Velocity,
				VisualLogColor.ToFColor(true),
				TEXT("Velocity: [% 6.3f % 6.3f % 6.3f]\nLocal Velocity: [% 6.3f % 6.3f % 6.3f]"),
				Velocity.X, Velocity.Y, Velocity.Z,
				LocalVelocity.X, LocalVelocity.Y, LocalVelocity.Z);

			UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
				Actor->GetActorLocation(),
				Actor->GetActorLocation() + 100.0f * Direction,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation() + 100.0f * Direction,
				VisualLogColor.ToFColor(true),
				TEXT("Direction: [% 6.3f % 6.3f % 6.3f]\nLocal Direction: [% 6.3f % 6.3f % 6.3f]"),
				Direction.X, Direction.Y, Direction.Z,
				RewardObject->Axis.X, RewardObject->Axis.Y, RewardObject->Axis.Z);

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nDot Product: [% 6.3f]\nWeight: [% 6.2f]\nScale: [% 6.2f]\nReward: [% 6.3f]"),
				Instance,
				LocalVelocity.Dot(RewardObject->Axis),
				WeightView[Instance],
				ScaleView[Instance],
				RewardView[Instance]);
		}
	}
}
#endif

//------------------------------------------------------------------

UPlanarPositionDifferencePenalty* UPlanarPositionDifferencePenalty::AddPlanarPositionDifferencePenalty(
	ULearningAgentsTrainer* InAgentTrainer,
	const FName Name,
	const float Weight,
	const float Scale,
	const float Threshold,
	const FVector Axis0,
	const FVector Axis1)
{
	return UE::Learning::Agents::Rewards::Private::AddReward<UPlanarPositionDifferencePenalty, UE::Learning::FPlanarPositionDifferencePenalty>(
		InAgentTrainer, 
		Name, 
		TEXT("AddPlanarPositionDifferencePenalty"), 
		Weight, 
		Scale, 
		Threshold, 
		Axis0.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector),
		Axis1.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
}

void UPlanarPositionDifferencePenalty::SetPlanarPositionDifferencePenalty(const int32 AgentId, const FVector Position0, const FVector Position1)
{
	if (!AgentTrainer->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	RewardObject->InstanceData->View(RewardObject->Position0Handle)[AgentId] = Position0;
	RewardObject->InstanceData->View(RewardObject->Position1Handle)[AgentId] = Position1;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPlanarPositionDifferencePenalty::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPlanarPositionDifferencePenalty::VisualLog);

	const TLearningArrayView<1, const FVector> Position0View = RewardObject->InstanceData->ConstView(RewardObject->Position0Handle);
	const TLearningArrayView<1, const FVector> Position1View = RewardObject->InstanceData->ConstView(RewardObject->Position1Handle);
	const TLearningArrayView<1, const float> WeightView = RewardObject->InstanceData->ConstView(RewardObject->WeightHandle);
	const TLearningArrayView<1, const float> ScaleView = RewardObject->InstanceData->ConstView(RewardObject->ScaleHandle);
	const TLearningArrayView<1, const float> ThresholdView = RewardObject->InstanceData->ConstView(RewardObject->ThresholdHandle);
	const TLearningArrayView<1, const float> RewardView = RewardObject->InstanceData->ConstView(RewardObject->RewardHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(AgentTrainer->GetAgent(Instance)))
		{
			const FVector Position0 = Position0View[Instance];
			const FVector Position1 = Position1View[Instance];

			const FVector PlanarPosition0 = FVector(RewardObject->Axis0.Dot(Position0), RewardObject->Axis1.Dot(Position0), 0.0f);
			const FVector PlanarPosition1 = FVector(RewardObject->Axis0.Dot(Position1), RewardObject->Axis1.Dot(Position1), 0.0f);

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
				RewardObject->Axis0,
				RewardObject->Axis1,
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
				RewardObject->Axis0,
				RewardObject->Axis1,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
				Position0,
				Position1,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nDistance: [% 6.3f]\nPlanar Distance: [% 6.3f]\nWeight: [% 6.2f]\nScale: [% 6.2f]\nThreshold: [% 6.2f]\nReward: [% 6.3f]"),
				Instance,
				FVector::Distance(Position0, Position1),
				FVector::Distance(PlanarPosition0, PlanarPosition1),
				WeightView[Instance],
				ScaleView[Instance],
				ThresholdView[Instance],
				RewardView[Instance]);
		}
	}
}
#endif

//------------------------------------------------------------------

UPositionArraySimilarityReward* UPositionArraySimilarityReward::AddPositionArraySimilarityReward(
	ULearningAgentsTrainer* InAgentTrainer,
	const FName Name,
	const int32 PositionNum,
	const float Weight,
	const float Scale)
{
	if (PositionNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddPositionArraySimilarityReward: Number of elements in array must be at least 1, got %i."), PositionNum);
		return nullptr;
	}

	return UE::Learning::Agents::Rewards::Private::AddReward<UPositionArraySimilarityReward, UE::Learning::FPositionArraySimilarityReward>(InAgentTrainer, Name, TEXT("AddPositionArraySimilarityReward"), PositionNum, Weight, Scale);
}

void UPositionArraySimilarityReward::SetPositionArraySimilarityReward(
	const int32 AgentId,
	const TArray<FVector>& Positions0,
	const TArray<FVector>& Positions1,
	const FVector RelativePosition0,
	const FVector RelativePosition1,
	const FRotator RelativeRotation0,
	const FRotator RelativeRotation1)
{
	if (!AgentTrainer->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, FVector> Positions0View = RewardObject->InstanceData->View(RewardObject->Positions0Handle);
	const TLearningArrayView<2, FVector> Positions1View = RewardObject->InstanceData->View(RewardObject->Positions1Handle);
	const TLearningArrayView<1, FVector> RelativePosition0View = RewardObject->InstanceData->View(RewardObject->RelativePosition0Handle);
	const TLearningArrayView<1, FVector> RelativePosition1View = RewardObject->InstanceData->View(RewardObject->RelativePosition1Handle);
	const TLearningArrayView<1, FQuat> RelativeRotation0View = RewardObject->InstanceData->View(RewardObject->RelativeRotation0Handle);
	const TLearningArrayView<1, FQuat> RelativeRotation1View = RewardObject->InstanceData->View(RewardObject->RelativeRotation1Handle);

	if (Positions0.Num() != Positions0View.Num<1>() || Positions1.Num() != Positions0View.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i and %i."), *GetName(), Positions0View.Num<1>(), Positions0.Num(), Positions1.Num());
		return;
	}

	RelativePosition0View[AgentId] = RelativePosition0;
	RelativePosition1View[AgentId] = RelativePosition1;
	RelativeRotation0View[AgentId] = RelativeRotation0.Quaternion();
	RelativeRotation1View[AgentId] = RelativeRotation1.Quaternion();
	UE::Learning::Array::Copy<1, FVector>(Positions0View[AgentId], Positions0);
	UE::Learning::Array::Copy<1, FVector>(Positions1View[AgentId], Positions1);
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPositionArraySimilarityReward::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPositionArraySimilarityReward::VisualLog);

	const TLearningArrayView<2, const FVector> Positions0View = RewardObject->InstanceData->ConstView(RewardObject->Positions0Handle);
	const TLearningArrayView<2, const FVector> Positions1View = RewardObject->InstanceData->ConstView(RewardObject->Positions1Handle);
	const TLearningArrayView<1, const FVector> RelativePosition0View = RewardObject->InstanceData->ConstView(RewardObject->RelativePosition0Handle);
	const TLearningArrayView<1, const FVector> RelativePosition1View = RewardObject->InstanceData->ConstView(RewardObject->RelativePosition1Handle);
	const TLearningArrayView<1, const FQuat> RelativeRotation0View = RewardObject->InstanceData->ConstView(RewardObject->RelativeRotation0Handle);
	const TLearningArrayView<1, const FQuat> RelativeRotation1View = RewardObject->InstanceData->ConstView(RewardObject->RelativeRotation1Handle);
	const TLearningArrayView<1, const float> WeightView = RewardObject->InstanceData->ConstView(RewardObject->WeightHandle);
	const TLearningArrayView<1, const float> ScaleView = RewardObject->InstanceData->ConstView(RewardObject->ScaleHandle);
	const TLearningArrayView<1, const float> ThresholdView = RewardObject->InstanceData->ConstView(RewardObject->ThresholdHandle);
	const TLearningArrayView<1, const float> RewardView = RewardObject->InstanceData->ConstView(RewardObject->RewardHandle);

	const int32 PositionNum = Positions0View.Num<1>();
		
	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(AgentTrainer->GetAgent(Instance)))
		{
			const FVector RelativePosition0 = RelativePosition0View[Instance];
			const FVector RelativePosition1 = RelativePosition1View[Instance];
			const FQuat RelativeRotation0 = RelativeRotation0View[Instance];
			const FQuat RelativeRotation1 = RelativeRotation1View[Instance];

			for (int32 PositionIdx = 0; PositionIdx < PositionNum; PositionIdx++)
			{
				const FVector Position0 = Positions0View[Instance][PositionIdx];
				const FVector Position1 = Positions1View[Instance][PositionIdx];

				const FVector LocalPosition0 = RelativeRotation0.UnrotateVector(Positions0View[Instance][PositionIdx] - RelativePosition0);
				const FVector LocalPosition1 = RelativeRotation1.UnrotateVector(Positions1View[Instance][PositionIdx] - RelativePosition1);

				UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
					Position0,
					10.0f,
					VisualLogColor.ToFColor(true),
					TEXT("Position0: [% 6.1f % 6.1f % 6.1f]\nLocal Position0: [% 6.1f % 6.1f % 6.1f]"),
					Position0.X, Position0.Y, Position0.Z,
					LocalPosition0.X, LocalPosition0.Y, LocalPosition0.Z);

				UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
					Position1,
					10.0f,
					VisualLogColor.ToFColor(true),
					TEXT("Position1: [% 6.1f % 6.1f % 6.1f]\nLocal Position1: [% 6.1f % 6.1f % 6.1f]"),
					Position1.X, Position1.Y, Position1.Z,
					LocalPosition1.X, LocalPosition1.Y, LocalPosition1.Z);

				UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
					Position0,
					Position1,
					VisualLogColor.ToFColor(true),
					TEXT("Distance: [% 6.1f]\nLocal Distance: [% 6.1f]"),
					FVector::Distance(Position0, Position1),
					FVector::Distance(LocalPosition0, LocalPosition1));
			}

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				RelativePosition0,
				RelativeRotation0,
				VisualLogColor.ToFColor(true),
				TEXT("Relative Transform 0"));

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				RelativePosition1,
				RelativeRotation1,
				VisualLogColor.ToFColor(true),
				TEXT("Relative Transform 1"));

			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nWeight: [% 6.2f]\nScale: [% 6.2f]\nThreshold: [% 6.2f]\nReward: [% 6.3f]"),
				Instance,
				WeightView[Instance],
				ScaleView[Instance],
				ThresholdView[Instance],
				RewardView[Instance]);
		}
	}
}
#endif
