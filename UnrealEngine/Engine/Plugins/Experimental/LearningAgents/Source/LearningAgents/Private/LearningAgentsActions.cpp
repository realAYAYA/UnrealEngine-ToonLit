// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsActions.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningArray.h"
#include "LearningArrayMap.h"
#include "LearningFeatureObject.h"
#include "LearningLog.h"

#include "GameFramework/Actor.h"

namespace UE::Learning::Agents::Actions::Private
{
	template<typename ActionUObject, typename ActionFObject, typename... InArgTypes>
	ActionUObject* AddAction(ULearningAgentsInteractor* InInteractor, const FName Name, const TCHAR* FunctionName, InArgTypes&& ...Args)
	{
		if (!InInteractor)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: InInteractor is nullptr."), FunctionName);
			return nullptr;
		}

		if (!InInteractor->HasAgentManager())
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Must be attached to a LearningAgentsManager Actor."), *InInteractor->GetName());
			return nullptr;
		}

		const FName UniqueName = MakeUniqueObjectName(InInteractor, ActionUObject::StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);

		ActionUObject* Action = NewObject<ActionUObject>(InInteractor, UniqueName);
		Action->Init(InInteractor->GetAgentManager()->GetMaxAgentNum());
		Action->Interactor = InInteractor;
		Action->FeatureObject = MakeShared<ActionFObject>(
			Action->GetFName(),
			InInteractor->GetAgentManager()->GetInstanceData().ToSharedRef(),
			InInteractor->GetAgentManager()->GetMaxAgentNum(),
			Forward<InArgTypes>(Args)...);

		// We assume all supported action feature objects can be encoded and decoded
		UE_LEARNING_CHECK(Action->FeatureObject->IsEncodable() && Action->FeatureObject->IsDecodable());

		InInteractor->AddAction(Action, Action->FeatureObject.ToSharedRef());

		return Action;
	}
}

//------------------------------------------------------------------

void ULearningAgentsAction::Init(const int32 MaxAgentNum)
{
	AgentGetIteration.SetNumUninitialized({ MaxAgentNum });
	AgentSetIteration.SetNumUninitialized({ MaxAgentNum });
	UE::Learning::Array::Set<1, uint64>(AgentGetIteration, INDEX_NONE);
	UE::Learning::Array::Set<1, uint64>(AgentSetIteration, INDEX_NONE);
}

void ULearningAgentsAction::OnAgentsAdded(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentGetIteration, 0, AgentIds);
	UE::Learning::Array::Set<1, uint64>(AgentSetIteration, 0, AgentIds);
}

void ULearningAgentsAction::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentGetIteration, INDEX_NONE, AgentIds);
	UE::Learning::Array::Set<1, uint64>(AgentSetIteration, INDEX_NONE, AgentIds);
}

void ULearningAgentsAction::OnAgentsReset(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentGetIteration, 0, AgentIds);
	UE::Learning::Array::Set<1, uint64>(AgentSetIteration, 0, AgentIds);
}

uint64 ULearningAgentsAction::GetAgentGetIteration(const int32 AgentId) const
{
	return AgentGetIteration[AgentId];
}

uint64 ULearningAgentsAction::GetAgentSetIteration(const int32 AgentId) const
{
	return AgentSetIteration[AgentId];
}

//------------------------------------------------------------------

UFloatAction* UFloatAction::AddFloatAction(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Actions::Private::AddAction<UFloatAction,UE::Learning::FFloatFeature>(InInteractor, Name, TEXT("AddFloatAction"), 1, Scale);
}

float UFloatAction::GetFloatAction(const int32 AgentId)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return 0.0f;
	}

	AgentGetIteration[AgentId]++;
	return FeatureObject->InstanceData->ConstView(FeatureObject->ValueHandle)[AgentId][0];
}

void UFloatAction::SetFloatAction(const int32 AgentId, const float Value)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->ValueHandle)[AgentId][0] = Value;
	AgentSetIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UFloatAction::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UFloatAction::VisualLog);

	const TLearningArrayView<2, const float> ValueView = FeatureObject->InstanceData->ConstView(FeatureObject->ValueHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nValue: %s\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(ValueView[Instance]),
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UFloatArrayAction* UFloatArrayAction::AddFloatArrayAction(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 Num, const float Scale)
{
	if (Num < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddFloatArrayAction: Number of elements in array must be at least 1, got %i."), Num);
		return nullptr;
	}

	return UE::Learning::Agents::Actions::Private::AddAction<UFloatArrayAction, UE::Learning::FFloatFeature>(InInteractor, Name, TEXT("AddFloatArrayAction"), Num, Scale);
}

void UFloatArrayAction::GetFloatArrayAction(const int32 AgentId, TArray<float>& OutValues)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutValues.Empty();
		return;
	}

	const TLearningArrayView<2, const float> View = FeatureObject->InstanceData->ConstView(FeatureObject->ValueHandle);

	AgentGetIteration[AgentId]++;

	OutValues.SetNumUninitialized(View.Num<1>());
	UE::Learning::Array::Copy<1, float>(OutValues, View[AgentId]);
}

void UFloatArrayAction::SetFloatArrayAction(const int32 AgentId, const TArray<float>& Values)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> View = FeatureObject->InstanceData->View(FeatureObject->ValueHandle);

	if (Values.Num() != View.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), View.Num<1>(), Values.Num());
		return;
	}

	UE::Learning::Array::Copy<1, float>(View[AgentId], Values);
	AgentSetIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UFloatArrayAction::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UFloatArrayAction::VisualLog);

	const TLearningArrayView<2, const float> ValueView = FeatureObject->InstanceData->ConstView(FeatureObject->ValueHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nValue: %s\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(ValueView[Instance]),
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

//------------------------------------------------------------------

UVectorAction* UVectorAction::AddVectorAction(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Actions::Private::AddAction<UVectorAction, UE::Learning::FFloatFeature>(InInteractor, Name, TEXT("AddVectorAction"), 3, Scale);
}

FVector UVectorAction::GetVectorAction(const int32 AgentId)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return FVector::ZeroVector;
	}

	const TLearningArrayView<2, const float> View = FeatureObject->InstanceData->ConstView(FeatureObject->ValueHandle);

	AgentGetIteration[AgentId]++;

	return FVector(View[AgentId][0], View[AgentId][1], View[AgentId][2]);
}

void UVectorAction::SetVectorAction(const int32 AgentId, const FVector InAction)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> View = FeatureObject->InstanceData->View(FeatureObject->ValueHandle);
	View[AgentId][0] = InAction.X;
	View[AgentId][1] = InAction.Y;
	View[AgentId][2] = InAction.Z;

	AgentSetIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UVectorAction::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UVectorAction::VisualLog);

	const TLearningArrayView<2, const float> ValueView = FeatureObject->InstanceData->ConstView(FeatureObject->ValueHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FVector Vector(ValueView[Instance][0], ValueView[Instance][1], ValueView[Instance][2]);

			UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
				Actor->GetActorLocation(),
				Actor->GetActorLocation() + Vector,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation() + Vector,
				VisualLogColor.ToFColor(true),
				TEXT("Vector: [% 6.4f % 6.4f % 6.4f]"),
				Vector.X, Vector.Y, Vector.Z);

			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: [% 6.3f % 6.3f % 6.3f]"),
				Instance,
				FeatureObject->Scale,
				FeatureView[Instance][0], FeatureView[Instance][1], FeatureView[Instance][2]);
		}
	}
}
#endif

UVectorArrayAction* UVectorArrayAction::AddVectorArrayAction(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 Num, const float Scale)
{
	if (Num < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddVectorArrayAction: Number of elements in array must be at least 1, got %i."), Num);
		return nullptr;
	}

	return UE::Learning::Agents::Actions::Private::AddAction<UVectorArrayAction, UE::Learning::FFloatFeature>(InInteractor, Name, TEXT("AddVectorArrayAction"), Num * 3, Scale);
}

void UVectorArrayAction::GetVectorArrayAction(const int32 AgentId, TArray<FVector>& OutVectors)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutVectors.Empty();
		return;
	}

	const TLearningArrayView<2, const float> View = FeatureObject->InstanceData->ConstView(FeatureObject->ValueHandle);

	AgentGetIteration[AgentId]++;

	OutVectors.SetNumUninitialized(View.Num<1>() / 3);

	for (int32 VectorIdx = 0; VectorIdx < View.Num<1>() / 3; VectorIdx++)
	{
		OutVectors[VectorIdx] = FVector(
			View[AgentId][VectorIdx * 3 + 0], 
			View[AgentId][VectorIdx * 3 + 1], 
			View[AgentId][VectorIdx * 3 + 2]);
	}
}

void UVectorArrayAction::SetVectorArrayAction(const int32 AgentId, const TArray<FVector>& Vectors)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> View = FeatureObject->InstanceData->View(FeatureObject->ValueHandle);

	if (Vectors.Num() != View.Num<1>() / 3)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), View.Num<1>() / 3, Vectors.Num());
		return;
	}

	for (int32 VectorIdx = 0; VectorIdx < Vectors.Num(); VectorIdx++)
	{
		View[AgentId][VectorIdx * 3 + 0] = Vectors[VectorIdx].X;
		View[AgentId][VectorIdx * 3 + 1] = Vectors[VectorIdx].Y;
		View[AgentId][VectorIdx * 3 + 2] = Vectors[VectorIdx].Z;
	}

	AgentSetIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UVectorArrayAction::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UVectorArrayAction::VisualLog);

	const TLearningArrayView<2, const float> ValueView = FeatureObject->InstanceData->ConstView(FeatureObject->ValueHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 VectorNum = ValueView.Num<1>() / 3;

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			for (int32 VectorIdx = 0; VectorIdx < VectorNum; VectorIdx++)
			{
				const FVector Vector(ValueView[Instance][VectorIdx * 3 + 0], ValueView[Instance][VectorIdx * 3 + 1], ValueView[Instance][VectorIdx * 3 + 2]);
				const FVector Offset = UE::Learning::Agents::Debug::GridOffsetForIndex(VectorIdx, VectorNum);

				UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset,
					Actor->GetActorLocation() + Offset + Vector,
					VisualLogColor.ToFColor(true),
					TEXT(""));

				UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset + Vector,
					VisualLogColor.ToFColor(true),
					TEXT("Vector %i: [% 6.4f % 6.4f % 6.4f]"),
					VectorIdx,
					Vector.X, Vector.Y, Vector.Z);
			}

			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

//------------------------------------------------------------------


UPlanarVelocityAction* UPlanarVelocityAction::AddPlanarVelocityAction(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale, const FVector Axis0, const FVector Axis1)
{
	return UE::Learning::Agents::Actions::Private::AddAction<UPlanarVelocityAction, UE::Learning::FPlanarVelocityFeature>(
		InInteractor, 
		Name, 
		TEXT("AddPlanarVelocityAction"), 
		1, 
		Scale,
		Axis0.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector),
		Axis1.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
}

FVector UPlanarVelocityAction::GetPlanarVelocityAction(const int32 AgentId)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return FVector::ZeroVector;
	}

	const TLearningArrayView<2, const FVector> View = FeatureObject->InstanceData->ConstView(FeatureObject->VelocityHandle);

	AgentGetIteration[AgentId]++;

	return View[AgentId][0];
}

void UPlanarVelocityAction::SetPlanarVelocityAction(const int32 AgentId, const FVector Velocity)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, FVector> View = FeatureObject->InstanceData->View(FeatureObject->VelocityHandle);
	View[AgentId][0] = Velocity;

	AgentSetIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPlanarVelocityAction::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPlanarVelocityAction::VisualLog);

	const TLearningArrayView<2, const FVector> VelocityView = FeatureObject->InstanceData->ConstView(FeatureObject->VelocityHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FVector Velocity = VelocityView[Instance][0];

			UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
				Actor->GetActorLocation(),
				Actor->GetActorLocation() + Velocity,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation() + Velocity,
				VisualLogColor.ToFColor(true),
				TEXT("Velocity: [% 6.3f % 6.3f % 6.3f]"),
				Velocity.X, Velocity.Y, Velocity.Z);

			UE_LEARNING_AGENTS_VLOG_PLANE(this, LogLearning, Display,
				Actor->GetActorLocation(),
				FQuat::Identity,
				FeatureObject->Axis0,
				FeatureObject->Axis1,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				FQuat::Identity,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

//------------------------------------------------------------------

URotationAction* URotationAction::AddRotationAction(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Actions::Private::AddAction<URotationAction, UE::Learning::FRotationVectorFeature>(InInteractor, Name, TEXT("AddRotationAction"), 1, Scale);
}

FRotator URotationAction::GetRotationAction(const int32 AgentId)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return FRotator::ZeroRotator;
	}

	const TLearningArrayView<2, const FVector> View = FeatureObject->InstanceData->ConstView(FeatureObject->RotationVectorsHandle);

	AgentGetIteration[AgentId]++;

	return FQuat::MakeFromRotationVector((UE_TWO_PI / 180.0f) * View[AgentId][0]).Rotator();
}

FVector URotationAction::GetRotationActionAsRotationVector(const int32 AgentId)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return FVector::ZeroVector;
	}

	return FeatureObject->InstanceData->ConstView(FeatureObject->RotationVectorsHandle)[AgentId][0];
}

FQuat URotationAction::GetRotationActionAsQuat(const int32 AgentId)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return FQuat::Identity;
	}

	const TLearningArrayView<2, const FVector> View = FeatureObject->InstanceData->ConstView(FeatureObject->RotationVectorsHandle);

	AgentGetIteration[AgentId]++;

	return FQuat::MakeFromRotationVector((UE_TWO_PI / 180.0f) * View[AgentId][0]);
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void URotationAction::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(URotationAction::VisualLog);

	const TLearningArrayView<2, const FVector> ValueView = FeatureObject->InstanceData->ConstView(FeatureObject->RotationVectorsHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 RotationVectorNum = ValueView.Num<1>();

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FQuat Rotation = FQuat::MakeFromRotationVector((UE_TWO_PI / 180.0f) * ValueView[Instance][0]);
			const FRotator Rotator = Rotation.Rotator();

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				Rotation,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nRotation: [% 6.1f % 6.1f % 6.1f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				Rotator.Pitch, Rotator.Roll, Rotator.Yaw,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif


URotationArrayAction* URotationArrayAction::AddRotationArrayAction(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 RotationNum, const float Scale)
{
	if (RotationNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddRotationArrayAction: Number of elements in array must be at least 1, got %i."), RotationNum);
		return nullptr;
	}

	return UE::Learning::Agents::Actions::Private::AddAction<URotationArrayAction, UE::Learning::FRotationVectorFeature>(InInteractor, Name, TEXT("AddRotationArrayAction"), RotationNum, Scale);
}

void URotationArrayAction::GetRotationArrayAction(const int32 AgentId, TArray<FRotator>& OutRotations)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, const FVector> View = FeatureObject->InstanceData->ConstView(FeatureObject->RotationVectorsHandle);

	AgentGetIteration[AgentId]++;

	OutRotations.SetNumUninitialized(View.Num<1>());
	for (int32 RotationVectorIdx = 0; RotationVectorIdx < View.Num<1>(); RotationVectorIdx++)
	{
		OutRotations[RotationVectorIdx] = FQuat::MakeFromRotationVector((UE_TWO_PI / 180.0f) * View[AgentId][RotationVectorIdx]).Rotator();
	}
}

void URotationArrayAction::GetRotationArrayActionAsRotationVectors(const int32 AgentId, TArray<FVector>& OutRotationVectors)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, const FVector> View = FeatureObject->InstanceData->ConstView(FeatureObject->RotationVectorsHandle);

	AgentGetIteration[AgentId]++;

	OutRotationVectors.SetNumUninitialized(View.Num<1>());
	for (int32 RotationVectorIdx = 0; RotationVectorIdx < View.Num<1>(); RotationVectorIdx++)
	{
		OutRotationVectors[RotationVectorIdx] = View[AgentId][RotationVectorIdx];
	}
}

void URotationArrayAction::GetRotationArrayActionAsQuats(const int32 AgentId, TArray<FQuat>& OutRotations)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, const FVector> View = FeatureObject->InstanceData->ConstView(FeatureObject->RotationVectorsHandle);

	AgentGetIteration[AgentId]++;

	OutRotations.SetNumUninitialized(View.Num<1>());
	for (int32 RotationVectorIdx = 0; RotationVectorIdx < View.Num<1>(); RotationVectorIdx++)
	{
		OutRotations[RotationVectorIdx] = FQuat::MakeFromRotationVector((UE_TWO_PI / 180.0f) * View[AgentId][RotationVectorIdx]);
	}
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void URotationArrayAction::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(URotationArrayAction::VisualLog);

	const TLearningArrayView<2, const FVector> ValueView = FeatureObject->InstanceData->ConstView(FeatureObject->RotationVectorsHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 RotationNum = ValueView.Num<1>();

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			for (int32 RotationIdx = 0; RotationIdx < RotationNum; RotationIdx++)
			{
				const FVector Offset = UE::Learning::Agents::Debug::GridOffsetForIndex(RotationIdx, RotationNum);
				const FQuat Rotation = FQuat::MakeFromRotationVector((UE_TWO_PI / 180.0f) * ValueView[Instance][RotationIdx]);
				const FRotator Rotator = Rotation.Rotator();

				UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset,
					Rotation,
					VisualLogColor.ToFColor(true),
					TEXT("Rotation %i: [% 6.3f % 6.3f % 6.3f]"),
					RotationIdx,
					Rotator.Pitch, Rotator.Roll, Rotator.Yaw);
			}

			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif
