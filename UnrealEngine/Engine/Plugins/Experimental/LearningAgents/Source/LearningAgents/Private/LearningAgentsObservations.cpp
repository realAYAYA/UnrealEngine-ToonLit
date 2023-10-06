// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsObservations.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningArray.h"
#include "LearningArrayMap.h"
#include "LearningFeatureObject.h"
#include "LearningLog.h"

#include "GameFramework/Actor.h"

namespace UE::Learning::Agents::Observations::Private
{
	template<typename ObservationUObject, typename ObservationFObject, typename... InArgTypes>
	ObservationUObject* AddObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const TCHAR* FunctionName, InArgTypes&& ...Args)
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

		const FName UniqueName = MakeUniqueObjectName(InInteractor, ObservationUObject::StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);

		ObservationUObject* Observation = NewObject<ObservationUObject>(InInteractor, UniqueName);

		Observation->Init(InInteractor->GetAgentManager()->GetMaxAgentNum());
		Observation->Interactor = InInteractor;
		Observation->FeatureObject = MakeShared<ObservationFObject>(
			Observation->GetFName(),
			InInteractor->GetAgentManager()->GetInstanceData().ToSharedRef(),
			InInteractor->GetAgentManager()->GetMaxAgentNum(),
			Forward<InArgTypes>(Args)...);

		// We assume all supported observation feature objects can be encoded
		UE_LEARNING_CHECK(Observation->FeatureObject->IsEncodable());

		InInteractor->AddObservation(Observation, Observation->FeatureObject.ToSharedRef());

		return Observation;
	}
}

//------------------------------------------------------------------

void ULearningAgentsObservation::Init(const int32 MaxAgentNum)
{
	AgentIteration.SetNumUninitialized({ MaxAgentNum });
	UE::Learning::Array::Set<1, uint64>(AgentIteration, INDEX_NONE);
}

void ULearningAgentsObservation::OnAgentsAdded(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentIteration, 0, AgentIds);
}

void ULearningAgentsObservation::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentIteration, INDEX_NONE, AgentIds);
}

void ULearningAgentsObservation::OnAgentsReset(const TArray<int32>& AgentIds)
{
	UE::Learning::Array::Set<1, uint64>(AgentIteration, 0, AgentIds);
}

uint64 ULearningAgentsObservation::GetAgentIteration(const int32 AgentId) const
{
	return AgentIteration[AgentId];
}

//------------------------------------------------------------------

UFloatObservation* UFloatObservation::AddFloatObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UFloatObservation, UE::Learning::FFloatFeature>(InInteractor, Name, TEXT("AddFloatObservation"), 1, Scale);
}

void UFloatObservation::SetFloatObservation(const int32 AgentId, const float Value)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->ValueHandle)[AgentId][0] = Value;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UFloatObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UFloatObservation::VisualLog);

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

UFloatArrayObservation* UFloatArrayObservation::AddFloatArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 Num, const float Scale)
{
	if (Num < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddFloatArrayObservation: Number of elements in array must be at least 1, got %i."), Num);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UFloatArrayObservation, UE::Learning::FFloatFeature>(InInteractor, Name, TEXT("AddFloatArrayObservation"), Num, Scale);
}

void UFloatArrayObservation::SetFloatArrayObservation(const int32 AgentId, const TArray<float>& Values)
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
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UFloatArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UFloatArrayObservation::VisualLog);

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

UVectorObservation* UVectorObservation::AddVectorObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UVectorObservation, UE::Learning::FFloatFeature>(InInteractor, Name, TEXT("AddVectorObservation"), 3, Scale);
}

void UVectorObservation::SetVectorObservation(const int32 AgentId, const FVector Vector)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> View = FeatureObject->InstanceData->View(FeatureObject->ValueHandle);

	View[AgentId][0] = Vector.X;
	View[AgentId][1] = Vector.Y;
	View[AgentId][2] = Vector.Z;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UVectorObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UVectorObservation::VisualLog);

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
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UVectorArrayObservation* UVectorArrayObservation::AddVectorArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 Num, const float Scale)
{
	if (Num < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddVectorArrayObservation: Number of elements in array must be at least 1, got %i."), Num);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UVectorArrayObservation, UE::Learning::FFloatFeature>(InInteractor, Name, TEXT("AddVectorArrayObservation"), Num * 3, Scale);
}

void UVectorArrayObservation::SetVectorArrayObservation(const int32 AgentId, const TArray<FVector>& Vectors)
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

	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UVectorArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UVectorArrayObservation::VisualLog);

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

UEnumObservation* UEnumObservation::AddEnumObservation(ULearningAgentsInteractor* InInteractor, const UEnum* EnumType, const FName Name)
{
	if (!EnumType)
	{
		UE_LOG(LogLearning, Error, TEXT("AddEnumObservation: Invalid Enum."));
		return nullptr;
	}

	if (EnumType->NumEnums() < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddEnumObservation: Enum requires at least one entry to be used as an observation."));
		return nullptr;
	}

	UEnumObservation* Observation = UE::Learning::Agents::Observations::Private::AddObservation<UEnumObservation, UE::Learning::FFloatFeature>(InInteractor, Name, TEXT("AddEnumObservation"), EnumType->NumEnums());
	if (Observation) { Observation->Enum = EnumType; }
	
	return Observation;
}

void UEnumObservation::SetEnumObservation(const int32 AgentId, const uint8 EnumValue)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const int32 Index = Enum->GetIndexByValue(EnumValue);

	if (Index == INDEX_NONE)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid enum value: %i."), *GetName(), EnumValue);
		return;
	}

	const TLearningArrayView<2, float> EnumView = FeatureObject->InstanceData->View(FeatureObject->ValueHandle);
	UE::Learning::Array::Zero(EnumView[AgentId]);
	EnumView[AgentId][Index] = 1.0f;

	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UEnumObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UEnumObservation::VisualLog);

	const TLearningArrayView<2, const float> ValueView = FeatureObject->InstanceData->ConstView(FeatureObject->ValueHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 EnumNum = ValueView.Num<1>();
		
	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			int32 EnumEntryIdx = INDEX_NONE;

			for (int32 EnumIdx = 0; EnumIdx < EnumNum; EnumIdx++)
			{
				if (ValueView[Instance][EnumIdx] != 0.0f)
				{
					EnumEntryIdx = EnumIdx;
					break;
				}
			}

			if (EnumEntryIdx != INDEX_NONE)
			{
				UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
					Actor->GetActorLocation(),
					VisualLogColor.ToFColor(true),
					TEXT("Agent %i\nValue: %i\nIndex: %i\nName: \"%s\"\nEncoded: %s"),
					Instance,
					Enum->GetValueByIndex(EnumEntryIdx),
					EnumEntryIdx,
					*Enum->GetDisplayNameTextByIndex(EnumEntryIdx).ToString(),
					*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
			}
			else
			{
				UE_LOG(LogLearning, Error, TEXT("Invalid Enum encoding."));
			}
		}
	}
}
#endif

UEnumArrayObservation* UEnumArrayObservation::AddEnumArrayObservation(ULearningAgentsInteractor* InInteractor, const UEnum* EnumType, const FName Name, const int32 EnumNum)
{
	if (!EnumType)
	{
		UE_LOG(LogLearning, Error, TEXT("AddEnumArrayObservation: Invalid Enum."));
		return nullptr;
	}

	if (EnumType->NumEnums() < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddEnumArrayObservation: Enum requires at least one entry to be used as an observation."));
		return nullptr;
	}

	if (EnumNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddEnumArrayObservation: Number of elements in array must be at least 1, got %i."), EnumNum);
		return nullptr;
	}

	UEnumArrayObservation* Observation = UE::Learning::Agents::Observations::Private::AddObservation<UEnumArrayObservation, UE::Learning::FFloatFeature>(InInteractor, Name, TEXT("AddEnumArrayObservation"), EnumNum * EnumType->NumEnums());
	if (Observation) { Observation->Enum = EnumType; }

	return Observation;
}

void UEnumArrayObservation::SetEnumArrayObservation(const int32 AgentId, const TArray<uint8>& EnumValues)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> EnumView = FeatureObject->InstanceData->View(FeatureObject->ValueHandle);

	const int32 EnumEntryNum = Enum->NumEnums();

	if (EnumValues.Num() != EnumView.Num<1>() / EnumEntryNum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), EnumView.Num<1>() / EnumEntryNum, EnumValues.Num());
		return;
	}

	for (int32 EnumIdx = 0; EnumIdx < EnumValues.Num(); EnumIdx++)
	{
		const TLearningArrayView<1, float> EnumViewSlice = EnumView[AgentId].Slice(EnumIdx * EnumEntryNum, EnumEntryNum);
		UE::Learning::Array::Zero(EnumViewSlice);

		const int32 Index = Enum->GetIndexByValue(EnumValues[EnumIdx]);

		if (Index == INDEX_NONE)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid enum value: %i"), *GetName(), EnumValues[EnumIdx]);
			continue;
		}

		EnumViewSlice[Index] = 1.0f;
	}

	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UEnumArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UEnumArrayObservation::VisualLog);

	const TLearningArrayView<2, const float> ValueView = FeatureObject->InstanceData->ConstView(FeatureObject->ValueHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nEncoded: %s"),
				Instance,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));

		}
	}
}
#endif

//------------------------------------------------------------------

UTimeObservation* UTimeObservation::AddTimeObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UTimeObservation, UE::Learning::FTimeFeature>(InInteractor, Name, TEXT("AddTimeObservation"), 1, Scale);
}

void UTimeObservation::SetTimeObservation(const int32 AgentId, const float Time, const float RelativeTime)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->TimeHandle)[AgentId][0] = Time;
	FeatureObject->InstanceData->View(FeatureObject->RelativeTimeHandle)[AgentId] = RelativeTime;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UTimeObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UTimeObservation::VisualLog);

	const TLearningArrayView<2, const float> TimeView = FeatureObject->InstanceData->ConstView(FeatureObject->TimeHandle);
	const TLearningArrayView<1, const float> RelativeTimeView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeTimeHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const float Time = TimeView[Instance][0];
			const float RelativeTime = RelativeTimeView[Instance];

			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nRelative Time: [% 6.3f]\nTime: [% 6.3f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				RelativeTime,
				Time,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UTimeArrayObservation* UTimeArrayObservation::AddTimeArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 TimeNum, const float Scale)
{
	if (TimeNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddTimeArrayObservation: Number of elements in array must be at least 1, got %i."), TimeNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UTimeArrayObservation, UE::Learning::FTimeFeature>(InInteractor, Name, TEXT("AddTimeArrayObservation"), TimeNum, Scale);
}

void UTimeArrayObservation::SetTimeArrayObservation(const int32 AgentId, const TArray<float>& Times, const float RelativeTime)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> TimeView = FeatureObject->InstanceData->View(FeatureObject->TimeHandle);
	const TLearningArrayView<1, float> RelativeTimeView = FeatureObject->InstanceData->View(FeatureObject->RelativeTimeHandle);

	if (Times.Num() != TimeView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), TimeView.Num<1>(), Times.Num());
		return;
	}

	UE::Learning::Array::Copy<1, float>(TimeView[AgentId], Times);
	RelativeTimeView[AgentId] = RelativeTime;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UTimeArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UTimeArrayObservation::VisualLog);

	const TLearningArrayView<2, const float> TimeView = FeatureObject->InstanceData->ConstView(FeatureObject->TimeHandle);
	const TLearningArrayView<1, const float> RelativeTimeView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeTimeHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nRelative Time: [% 6.3f]\nTimes: %s\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				RelativeTimeView[Instance],
				*UE::Learning::Array::FormatFloat(TimeView[Instance]),
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

//------------------------------------------------------------------

UAngleObservation* UAngleObservation::AddAngleObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UAngleObservation, UE::Learning::FAngleFeature>(InInteractor, Name, TEXT("AddAngleObservation"), 1, Scale);
}

void UAngleObservation::SetAngleObservation(const int32 AgentId, const float Angle, const float RelativeAngle)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->AngleHandle)[AgentId][0] = FMath::DegreesToRadians(Angle);
	FeatureObject->InstanceData->View(FeatureObject->RelativeAngleHandle)[AgentId] = FMath::DegreesToRadians(RelativeAngle);
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UAngleObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UAngleObservation::VisualLog);

	const TLearningArrayView<2, const float> AngleView = FeatureObject->InstanceData->ConstView(FeatureObject->AngleHandle);
	const TLearningArrayView<1, const float> RelativeAngleView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeAngleHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const float Angle = AngleView[Instance][0];
			const float RelativeAngle = RelativeAngleView[Instance];

			UE_LEARNING_AGENTS_VLOG_ANGLE(this, LogLearning, Display,
				Angle,
				0.0f,
				Actor->GetActorLocation(),
				50.0f,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nRelative Angle: [% 6.1f]\nAngle: [% 6.1f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				FMath::RadiansToDegrees(RelativeAngle),
				FMath::RadiansToDegrees(Angle),
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UAngleArrayObservation* UAngleArrayObservation::AddAngleArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 AngleNum, const float Scale)
{
	if (AngleNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddAngleArrayObservation: Number of elements in array must be at least 1, got %i."), AngleNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UAngleArrayObservation, UE::Learning::FAngleFeature>(InInteractor, Name, TEXT("AddAngleArrayObservation"), AngleNum, Scale);
}

void UAngleArrayObservation::SetAngleArrayObservation(const int32 AgentId, const TArray<float>& Angles, const float RelativeAngle)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> AngleView = FeatureObject->InstanceData->View(FeatureObject->AngleHandle);
	const TLearningArrayView<1, float> RelativeAngleView = FeatureObject->InstanceData->View(FeatureObject->RelativeAngleHandle);

	if (Angles.Num() != AngleView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), AngleView.Num<1>(), Angles.Num());
		return;
	}

	for (int32 AngleIdx = 0; AngleIdx < Angles.Num(); AngleIdx++)
	{
		AngleView[AgentId][AngleIdx] = FMath::DegreesToRadians(Angles[AngleIdx]);
	}
	
	RelativeAngleView[AgentId] = FMath::DegreesToRadians(RelativeAngle);
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UAngleArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UAngleArrayObservation::VisualLog);

	const TLearningArrayView<2, const float> AngleView = FeatureObject->InstanceData->ConstView(FeatureObject->AngleHandle);
	const TLearningArrayView<1, const float> RelativeAngleView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeAngleHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 AngleNum = AngleView.Num<1>();

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const float RelativeAngle = RelativeAngleView[Instance];

			for (int32 AngleIdx = 0; AngleIdx < AngleNum; AngleIdx++)
			{
				const float Angle = AngleView[Instance][AngleIdx];
				const FVector Offset = UE::Learning::Agents::Debug::GridOffsetForIndex(AngleIdx, AngleNum);

				UE_LEARNING_AGENTS_VLOG_ANGLE(this, LogLearning, Display,
					Angle,
					RelativeAngle,
					Actor->GetActorLocation() + Offset,
					5.0f,
					VisualLogColor.ToFColor(true),
					TEXT("Angle %i: [% 6.4f]"),
					AngleIdx,
					FMath::RadiansToDegrees(Angle));
			}

			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation() + FVector(0.0f, 0.0f, 20.0f),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nRelative Angle: [% 6.1f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				FMath::RadiansToDegrees(RelativeAngle),
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

//------------------------------------------------------------------

URotationObservation* URotationObservation::AddRotationObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<URotationObservation, UE::Learning::FRotationFeature>(InInteractor, Name, TEXT("AddRotationObservation"), 1, Scale);
}

void URotationObservation::SetRotationObservation(const int32 AgentId, const FRotator Rotation, const FRotator RelativeRotation)
{
	SetRotationObservationFromQuat(AgentId, Rotation.Quaternion(), RelativeRotation.Quaternion());
}

void URotationObservation::SetRotationObservationFromQuat(const int32 AgentId, const FQuat Rotation, const FQuat RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->RotationHandle)[AgentId][0] = Rotation;
	FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle)[AgentId] = RelativeRotation;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void URotationObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(URotationObservation::VisualLog);

	const TLearningArrayView<2, const FQuat> RotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RotationHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FQuat Rotation = RotationView[Instance][0];
			const FRotator Rotator = Rotation.Rotator();
			const FQuat RelativeRotation = RelativeRotationView[Instance];
			const FRotator RelativeRotator = RelativeRotation.Rotator();

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation() + FVector(20.0f, 0.0f, 0.0f),
				Rotation,
				VisualLogColor.ToFColor(true),
				TEXT("Relative Rotation: [% 6.1f % 6.1f % 6.1f]"),
				RelativeRotator.Pitch, RelativeRotator.Roll, RelativeRotator.Yaw);

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

URotationArrayObservation* URotationArrayObservation::AddRotationArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 RotationNum, const float Scale)
{
	if (RotationNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddRotationArrayObservation: Number of elements in array must be at least 1, got %i."), RotationNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<URotationArrayObservation, UE::Learning::FRotationFeature>(InInteractor, Name, TEXT("AddRotationArrayObservation"), RotationNum, Scale);
}

void URotationArrayObservation::SetRotationArrayObservation(const int32 AgentId, const TArray<FRotator>& Rotations, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, FQuat> RotationView = FeatureObject->InstanceData->View(FeatureObject->RotationHandle);
	const TLearningArrayView<1, FQuat> RelativeRotationView = FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle);

	if (Rotations.Num() != RotationView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), RotationView.Num<1>(), Rotations.Num());
		return;
	}

	for (int32 RotationIdx = 0; RotationIdx < Rotations.Num(); RotationIdx++)
	{
		RotationView[AgentId][RotationIdx] = Rotations[RotationIdx].Quaternion();
	}

	RelativeRotationView[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

void URotationArrayObservation::SetRotationArrayObservationFromQuats(const int32 AgentId, const TArray<FQuat>& Rotations, const FQuat RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, FQuat> RotationView = FeatureObject->InstanceData->View(FeatureObject->RotationHandle);
	const TLearningArrayView<1, FQuat> RelativeRotationView = FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle);

	if (Rotations.Num() != RotationView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), RotationView.Num<1>(), Rotations.Num());
		return;
	}

	UE::Learning::Array::Copy<1, FQuat>(RotationView[AgentId], Rotations);
	RelativeRotationView[AgentId] = RelativeRotation;
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void URotationArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(URotationArrayObservation::VisualLog);

	const TLearningArrayView<2, const FQuat> RotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RotationHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 RotationNum = RotationView.Num<1>();

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FQuat RelativeRotation = RelativeRotationView[Instance];
			const FRotator RelativeRotator = RelativeRotation.Rotator();

			for (int32 RotationIdx = 0; RotationIdx < RotationNum; RotationIdx++)
			{
				const FQuat Rotation = RotationView[Instance][RotationIdx];
				const FRotator Rotator = Rotation.Rotator();
				const FVector Offset = UE::Learning::Agents::Debug::GridOffsetForIndex(RotationIdx, RotationNum);

				UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset,
					Rotation,
					VisualLogColor.ToFColor(true),
					TEXT("Rotation %i: [% 6.3f % 6.3f % 6.3f]"),
					RotationIdx,
					Rotator.Pitch, Rotator.Roll, Rotator.Yaw);
			}

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nRelative Rotation: [% 6.3f % 6.3f % 6.3f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				RelativeRotator.Pitch, RelativeRotator.Roll, RelativeRotator.Yaw,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif


//------------------------------------------------------------------

UDirectionObservation* UDirectionObservation::AddDirectionObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UDirectionObservation, UE::Learning::FDirectionFeature>(InInteractor, Name, TEXT("AddDirectionObservation"), 1, Scale);
}

void UDirectionObservation::SetDirectionObservation(const int32 AgentId, const FVector Direction, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->DirectionHandle)[AgentId][0] = Direction;
	FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle)[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UDirectionObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UDirectionObservation::VisualLog);

	const TLearningArrayView<2, const FVector> DirectionView = FeatureObject->InstanceData->ConstView(FeatureObject->DirectionHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FVector Direction = DirectionView[Instance][0];
			const FQuat RelativeRotation = RelativeRotationView[Instance];
			const FVector LocalDirection = RelativeRotation.UnrotateVector(Direction);

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
				LocalDirection.X, LocalDirection.Y, LocalDirection.Z);

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UDirectionArrayObservation* UDirectionArrayObservation::AddDirectionArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 DirectionNum, const float Scale)
{
	if (DirectionNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddDirectionArrayObservation: Number of elements in array must be at least 1, got %i."), DirectionNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UDirectionArrayObservation, UE::Learning::FDirectionFeature>(InInteractor, Name, TEXT("AddDirectionArrayObservation"), DirectionNum, Scale);
}

void UDirectionArrayObservation::SetDirectionArrayObservation(const int32 AgentId, const TArray<FVector>& Directions, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, FVector> DirectionView = FeatureObject->InstanceData->View(FeatureObject->DirectionHandle);
	const TLearningArrayView<1, FQuat> RelativeRotationView = FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle);

	if (Directions.Num() != DirectionView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), DirectionView.Num<1>(), Directions.Num());
		return;
	}

	UE::Learning::Array::Copy<1, FVector>(DirectionView[AgentId], Directions);
	RelativeRotationView[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UDirectionArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UDirectionArrayObservation::VisualLog);

	const TLearningArrayView<2, const FVector> DirectionView = FeatureObject->InstanceData->ConstView(FeatureObject->DirectionHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 DirectionNum = DirectionView.Num<1>();

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FQuat RelativeRotation = RelativeRotationView[Instance];

			for (int32 DirectionIdx = 0; DirectionIdx < DirectionNum; DirectionIdx++)
			{
				const FVector Direction = DirectionView[Instance][0];
				const FVector LocalDirection = RelativeRotation.UnrotateVector(Direction);
				const FVector Offset = UE::Learning::Agents::Debug::GridOffsetForIndex(DirectionIdx, DirectionNum);

				UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset,
					Actor->GetActorLocation() + Offset + 10.0f * Direction,
					VisualLogColor.ToFColor(true),
					TEXT(""));

				UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset + 10.0f * Direction,
					VisualLogColor.ToFColor(true),
					TEXT("Direction %i: [% 6.3f % 6.3f % 6.3f]\nLocal Direction: [% 6.3f % 6.3f % 6.3f]"),
					DirectionIdx,
					Direction.X, Direction.Y, Direction.Z,
					LocalDirection.X, LocalDirection.Y, LocalDirection.Z);
			}

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UPlanarDirectionObservation* UPlanarDirectionObservation::AddPlanarDirectionObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale, const FVector Axis0, const FVector Axis1)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UPlanarDirectionObservation, UE::Learning::FPlanarDirectionFeature>(
		InInteractor, 
		Name, 
		TEXT("AddPlanarDirectionObservation"), 
		1, 
		Scale, 
		Axis0.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector), 
		Axis1.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
}

void UPlanarDirectionObservation::SetPlanarDirectionObservation(const int32 AgentId, const FVector Direction, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->DirectionHandle)[AgentId][0] = Direction;
	FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle)[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPlanarDirectionObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPlanarDirectionObservation::VisualLog);

	const TLearningArrayView<2, const FVector> DirectionView = FeatureObject->InstanceData->ConstView(FeatureObject->DirectionHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FVector Direction = DirectionView[Instance][0];
			const FQuat RelativeRotation = RelativeRotationView[Instance];
			const FVector LocalDirection = RelativeRotation.UnrotateVector(Direction);

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
				LocalDirection.X, LocalDirection.Y, LocalDirection.Z);

			UE_LEARNING_AGENTS_VLOG_PLANE(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				FeatureObject->Axis0,
				FeatureObject->Axis1,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UPlanarDirectionArrayObservation* UPlanarDirectionArrayObservation::AddPlanarDirectionArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 DirectionNum, const float Scale, const FVector Axis0, const FVector Axis1)
{
	if (DirectionNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddPlanarDirectionArrayObservation: Number of elements in array must be at least 1, got %i."), DirectionNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UPlanarDirectionArrayObservation, UE::Learning::FPlanarDirectionFeature>(
		InInteractor, 
		Name, 
		TEXT("AddPlanarDirectionArrayObservation"), 
		DirectionNum, 
		Scale, 
		Axis0.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector),
		Axis1.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
}

void UPlanarDirectionArrayObservation::SetPlanarDirectionArrayObservation(const int32 AgentId, const TArray<FVector>& Directions, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, FVector> DirectionView = FeatureObject->InstanceData->View(FeatureObject->DirectionHandle);
	const TLearningArrayView<1, FQuat> RelativeRotationView = FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle);

	if (Directions.Num() != DirectionView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), DirectionView.Num<1>(), Directions.Num());
		return;
	}

	UE::Learning::Array::Copy<1, FVector>(DirectionView[AgentId], Directions);
	RelativeRotationView[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPlanarDirectionArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPlanarDirectionArrayObservation::VisualLog);

	const TLearningArrayView<2, const FVector> DirectionView = FeatureObject->InstanceData->ConstView(FeatureObject->DirectionHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 DirectionNum = DirectionView.Num<1>();
	
	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FQuat RelativeRotation = RelativeRotationView[Instance];

			for (int32 DirectionIdx = 0; DirectionIdx < DirectionNum; DirectionIdx++)
			{
				const FVector Direction = DirectionView[Instance][0];
				const FVector LocalDirection = RelativeRotation.UnrotateVector(Direction);
				const FVector Offset = UE::Learning::Agents::Debug::GridOffsetForIndex(DirectionIdx, DirectionNum);

				UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset,
					Actor->GetActorLocation() + Offset + 10.0f * Direction,
					VisualLogColor.ToFColor(true),
					TEXT(""));

				UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset + 10.0f * Direction,
					VisualLogColor.ToFColor(true),
					TEXT("Direction %i: [% 6.3f % 6.3f % 6.3f]\nLocal Direction: [% 6.3f % 6.3f % 6.3f]"),
					DirectionIdx,
					Direction.X, Direction.Y, Direction.Z,
					LocalDirection.X, LocalDirection.Y, LocalDirection.Z);
			}

			UE_LEARNING_AGENTS_VLOG_PLANE(this, LogLearning, Display,
				Actor->GetActorLocation() + FVector(0.0f, 0.0f, 20.0f),
				RelativeRotation,
				FeatureObject->Axis0,
				FeatureObject->Axis1,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
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

UPositionObservation* UPositionObservation::AddPositionObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UPositionObservation, UE::Learning::FPositionFeature>(InInteractor, Name, TEXT("AddPositionObservation"), 1, Scale);
}

void UPositionObservation::SetPositionObservation(const int32 AgentId, const FVector Position, const FVector RelativePosition, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->PositionHandle)[AgentId][0] = Position;
	FeatureObject->InstanceData->View(FeatureObject->RelativePositionHandle)[AgentId] = RelativePosition;
	FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle)[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPositionObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPositionObservation::VisualLog);

	const TLearningArrayView<2, const FVector> PositionView = FeatureObject->InstanceData->ConstView(FeatureObject->PositionHandle);
	const TLearningArrayView<1, const FVector> RelativePositionView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativePositionHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		const FVector Position = PositionView[Instance][0];
		const FVector RelativePosition = RelativePositionView[Instance];
		const FQuat RelativeRotation = RelativeRotationView[Instance];
		const FVector LocalPosition = RelativeRotation.UnrotateVector(Position - RelativePosition);

		UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
			Position,
			10.0f,
			VisualLogColor.ToFColor(true),
			TEXT("Position: [% 6.1f % 6.1f % 6.1f]\nLocal Position: [% 6.1f % 6.1f % 6.1f]"),
			Position.X, Position.Y, Position.Z,
			LocalPosition.X, LocalPosition.Y, LocalPosition.Z);

		UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
			RelativePosition,
			Position,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
			RelativePosition,
			RelativeRotation,
			VisualLogColor.ToFColor(true),
			TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
			Instance,
			FeatureObject->Scale,
			*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
	}
}
#endif

UPositionArrayObservation* UPositionArrayObservation::AddPositionArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 PositionNum, const float Scale)
{
	if (PositionNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddPositionArrayObservation: Number of elements in array must be at least 1, got %i."), PositionNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UPositionArrayObservation, UE::Learning::FPositionFeature>(InInteractor, Name, TEXT("AddPositionArrayObservation"), PositionNum, Scale);
}

void UPositionArrayObservation::SetPositionArrayObservation(const int32 AgentId, const TArray<FVector>& Positions, const FVector RelativePosition, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, FVector> PositionView = FeatureObject->InstanceData->View(FeatureObject->PositionHandle);
	const TLearningArrayView<1, FVector> RelativePositionView = FeatureObject->InstanceData->View(FeatureObject->RelativePositionHandle);
	const TLearningArrayView<1, FQuat> RelativeRotationView = FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle);

	if (Positions.Num() != PositionView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), PositionView.Num<1>(), Positions.Num());
		return;
	}

	RelativePositionView[AgentId] = RelativePosition;
	RelativeRotationView[AgentId] = RelativeRotation.Quaternion();
	UE::Learning::Array::Copy<1, FVector>(PositionView[AgentId], Positions);
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPositionArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPositionArrayObservation::VisualLog);

	const TLearningArrayView<2, const FVector> PositionView = FeatureObject->InstanceData->ConstView(FeatureObject->PositionHandle);
	const TLearningArrayView<1, const FVector> RelativePositionView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativePositionHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 PositionNum = PositionView.Num<1>();

	for (const int32 Instance : Instances)
	{
		const FVector RelativePosition = RelativePositionView[Instance];
		const FQuat RelativeRotation = RelativeRotationView[Instance];

		for (int32 PositionIdx = 0; PositionIdx < PositionNum; PositionIdx++)
		{
			const FVector LocalPosition = RelativeRotation.UnrotateVector(PositionView[Instance][PositionIdx] - RelativePosition);

			UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
				PositionView[Instance][PositionIdx],
				10.0f,
				VisualLogColor.ToFColor(true),
				TEXT("Position: [% 6.1f % 6.1f % 6.1f]\nLocal Position: [% 6.1f % 6.1f % 6.1f]"),
				PositionView[Instance][PositionIdx].X,
				PositionView[Instance][PositionIdx].Y,
				PositionView[Instance][PositionIdx].Z,
				LocalPosition.X, LocalPosition.Y, LocalPosition.Z);

			UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
				RelativePosition,
				PositionView[Instance][PositionIdx],
				VisualLogColor.ToFColor(true),
				TEXT(""));
		}

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
			RelativePosition,
			RelativeRotation,
			VisualLogColor.ToFColor(true),
			TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
			Instance,
			FeatureObject->Scale,
			*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
	}
}
#endif

UScalarPositionObservation* UScalarPositionObservation::AddScalarPositionObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UScalarPositionObservation, UE::Learning::FScalarPositionFeature>(InInteractor, Name, TEXT("AddScalarPositionObservation"), 1, Scale);
}

void UScalarPositionObservation::SetScalarPositionObservation(const int32 AgentId, const float Position, const float RelativePosition)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->PositionHandle)[AgentId][0] = Position;
	FeatureObject->InstanceData->View(FeatureObject->RelativePositionHandle)[AgentId] = RelativePosition;
	AgentIteration[AgentId]++;
}

void UScalarPositionObservation::SetScalarPositionObservationWithAxis(const int32 AgentId, const FVector Position, const FVector RelativePosition, const FVector Axis)
{
	SetScalarPositionObservation(Position.Dot(Axis), RelativePosition.Dot(Axis));
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UScalarPositionObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UScalarPositionObservation::VisualLog);

	const TLearningArrayView<2, const float> PositionView = FeatureObject->InstanceData->ConstView(FeatureObject->PositionHandle);
	const TLearningArrayView<1, const float> RelativePositionView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativePositionHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nRelative Position [% 6.1f]\nLocal Position: [% 6.1f]\nPosition: [% 6.1f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				RelativePositionView[Instance],
				PositionView[Instance][0] - RelativePositionView[Instance],
				PositionView[Instance][0],
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UScalarPositionArrayObservation* UScalarPositionArrayObservation::AddScalarPositionArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 PositionNum, const float Scale)
{
	if (PositionNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddScalarPositionArrayObservation: Number of elements in array must be at least 1, got %i."), PositionNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UScalarPositionArrayObservation, UE::Learning::FScalarPositionFeature>(InInteractor, Name, TEXT("AddScalarPositionArrayObservation"), PositionNum, Scale);
}

void UScalarPositionArrayObservation::SetScalarPositionArrayObservation(const int32 AgentId, const TArray<float>& Positions, const float RelativePosition)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> PositionView = FeatureObject->InstanceData->View(FeatureObject->PositionHandle);
	const TLearningArrayView<1, float> RelativePositionView = FeatureObject->InstanceData->View(FeatureObject->RelativePositionHandle);

	if (Positions.Num() != PositionView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), PositionView.Num<1>(), Positions.Num());
		return;
	}

	RelativePositionView[AgentId] = RelativePosition;
	UE::Learning::Array::Copy<1, float>(PositionView[AgentId], Positions);
	AgentIteration[AgentId]++;
}

void UScalarPositionArrayObservation::SetScalarPositionArrayObservationWithAxis(const int32 AgentId, const TArray<FVector>& Positions, const FVector RelativePosition, const FVector Axis)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> PositionView = FeatureObject->InstanceData->View(FeatureObject->PositionHandle);
	const TLearningArrayView<1, float> RelativePositionView = FeatureObject->InstanceData->View(FeatureObject->RelativePositionHandle);

	if (Positions.Num() != PositionView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), PositionView.Num<1>(), Positions.Num());
		return;
	}

	RelativePositionView[AgentId] = RelativePosition.Dot(Axis);
	for (int32 PositionIdx = 0; PositionIdx < PositionView.Num<1>(); PositionIdx++)
	{
		PositionView[AgentId][PositionIdx] = Positions[PositionIdx].Dot(Axis);
	}

	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UScalarPositionArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UScalarPositionArrayObservation::VisualLog);

	const TLearningArrayView<2, const float> PositionView = FeatureObject->InstanceData->ConstView(FeatureObject->PositionHandle);
	const TLearningArrayView<1, const float> RelativePositionView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativePositionHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 PositionNum = PositionView.Num<1>();

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nRelative Position: [% 6.2f]\nPositions: %s\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				RelativePositionView[Instance],
				*UE::Learning::Array::FormatFloat(PositionView[Instance]),
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UPlanarPositionObservation* UPlanarPositionObservation::AddPlanarPositionObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale, const FVector Axis0, const FVector Axis1)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UPlanarPositionObservation, UE::Learning::FPlanarPositionFeature>(
		InInteractor, 
		Name, 
		TEXT("AddPlanarPositionObservation"), 
		1, 
		Scale, 
		Axis0.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector),
		Axis1.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
}

void UPlanarPositionObservation::SetPlanarPositionObservation(const int32 AgentId, const FVector Position, const FVector RelativePosition, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->PositionHandle)[AgentId][0] = Position;
	FeatureObject->InstanceData->View(FeatureObject->RelativePositionHandle)[AgentId] = RelativePosition;
	FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle)[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPlanarPositionObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPlanarPositionObservation::VisualLog);

	const TLearningArrayView<2, const FVector> PositionView = FeatureObject->InstanceData->ConstView(FeatureObject->PositionHandle);
	const TLearningArrayView<1, const FVector> RelativePositionView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativePositionHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		const FVector Position = PositionView[Instance][0];
		const FVector RelativePosition = RelativePositionView[Instance];
		const FQuat RelativeRotation = RelativeRotationView[Instance];
		const FVector LocalPosition = RelativeRotation.UnrotateVector(Position - RelativePosition);

		UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
			Position,
			10.0f,
			VisualLogColor.ToFColor(true),
			TEXT("Position: [% 6.1f % 6.1f % 6.1f]\nLocal Position: [% 6.1f % 6.1f % 6.1f]"),
			Position.X, Position.Y, Position.Z,
			LocalPosition.X, LocalPosition.Y, LocalPosition.Z);

		UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
			RelativePosition,
			Position,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_PLANE(this, LogLearning, Display,
			RelativePosition,
			RelativeRotation,
			FeatureObject->Axis0,
			FeatureObject->Axis1,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
			RelativePosition,
			RelativeRotation,
			VisualLogColor.ToFColor(true),
			TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
			Instance,
			FeatureObject->Scale,
			*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
	}
}
#endif

UPlanarPositionArrayObservation* UPlanarPositionArrayObservation::AddPlanarPositionArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 PositionNum, const float Scale, const FVector Axis0, const FVector Axis1)
{
	if (PositionNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddPlanarPositionArrayObservation: Number of elements in array must be at least 1, got %i."), PositionNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UPlanarPositionArrayObservation, UE::Learning::FPlanarPositionFeature>(
		InInteractor, 
		Name, 
		TEXT("AddPlanarPositionArrayObservation"), 
		PositionNum, 
		Scale, 
		Axis0.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector),
		Axis1.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
}

void UPlanarPositionArrayObservation::SetPlanarPositionArrayObservation(const int32 AgentId, const TArray<FVector>& Positions, const FVector RelativePosition, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, FVector> PositionView = FeatureObject->InstanceData->View(FeatureObject->PositionHandle);
	const TLearningArrayView<1, FVector> RelativePositionView = FeatureObject->InstanceData->View(FeatureObject->RelativePositionHandle);
	const TLearningArrayView<1, FQuat> RelativeRotationView = FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle);

	if (Positions.Num() != PositionView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), PositionView.Num<1>(), Positions.Num());
		return;
	}

	RelativePositionView[AgentId] = RelativePosition;
	RelativeRotationView[AgentId] = RelativeRotation.Quaternion();
	UE::Learning::Array::Copy<1, FVector>(PositionView[AgentId], Positions);
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPlanarPositionArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPlanarPositionArrayObservation::VisualLog);

	const TLearningArrayView<2, const FVector> PositionView = FeatureObject->InstanceData->ConstView(FeatureObject->PositionHandle);
	const TLearningArrayView<1, const FVector> RelativePositionView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativePositionHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 PositionNum = PositionView.Num<1>();

	for (const int32 Instance : Instances)
	{
		const FVector RelativePosition = RelativePositionView[Instance];
		const FQuat RelativeRotation = RelativeRotationView[Instance];

		for (int32 PositionIdx = 0; PositionIdx < PositionNum; PositionIdx++)
		{
			const FVector LocalPosition = RelativeRotation.UnrotateVector(PositionView[Instance][PositionIdx] - RelativePosition);

			UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
				PositionView[Instance][PositionIdx],
				10.0f,
				VisualLogColor.ToFColor(true),
				TEXT("Position: [% 6.1f % 6.1f % 6.1f]\nLocal Position: [% 6.1f % 6.1f % 6.1f]"),
				PositionView[Instance][PositionIdx].X, 
				PositionView[Instance][PositionIdx].Y, 
				PositionView[Instance][PositionIdx].Z,
				LocalPosition.X, LocalPosition.Y, LocalPosition.Z);

			UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
				RelativePosition,
				PositionView[Instance][PositionIdx],
				VisualLogColor.ToFColor(true),
				TEXT(""));
		}

		UE_LEARNING_AGENTS_VLOG_PLANE(this, LogLearning, Display,
			RelativePosition,
			RelativeRotation,
			FeatureObject->Axis0,
			FeatureObject->Axis1,
			VisualLogColor.ToFColor(true),
			TEXT(""));


		UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
			RelativePosition,
			RelativeRotation,
			VisualLogColor.ToFColor(true),
			TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
			Instance,
			FeatureObject->Scale,
			*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
	}
}
#endif

//------------------------------------------------------------------

UVelocityObservation* UVelocityObservation::AddVelocityObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UVelocityObservation, UE::Learning::FVelocityFeature>(InInteractor, Name, TEXT("AddVelocityObservation"), 1, Scale);
}

void UVelocityObservation::SetVelocityObservation(const int32 AgentId, const FVector Velocity, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->VelocityHandle)[AgentId][0] = Velocity;
	FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle)[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UVelocityObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UVelocityObservation::VisualLog);

	const TLearningArrayView<2, const FVector> VelocityView = FeatureObject->InstanceData->ConstView(FeatureObject->VelocityHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FVector Velocity = VelocityView[Instance][0];
			const FQuat RelativeRotation = RelativeRotationView[Instance];
			const FVector LocalVelocity = RelativeRotation.UnrotateVector(Velocity);

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

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UVelocityArrayObservation* UVelocityArrayObservation::AddVelocityArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 VelocityNum, const float Scale)
{
	if (VelocityNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddVelocityArrayObservation: Number of elements in array must be at least 1, got %i."), VelocityNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UVelocityArrayObservation, UE::Learning::FVelocityFeature>(InInteractor, Name, TEXT("AddVelocityArrayObservation"), VelocityNum, Scale);
}

void UVelocityArrayObservation::SetVelocityArrayObservation(const int32 AgentId, const TArray<FVector>& Velocities, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, FVector> VelocityView = FeatureObject->InstanceData->View(FeatureObject->VelocityHandle);
	const TLearningArrayView<1, FQuat> RelativeRotationView = FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle);

	if (Velocities.Num() != VelocityView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), VelocityView.Num<1>(), Velocities.Num());
		return;
	}

	UE::Learning::Array::Copy<1, FVector>(VelocityView[AgentId], Velocities);
	RelativeRotationView[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UVelocityArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UVelocityArrayObservation::VisualLog);

	const TLearningArrayView<2, const FVector> VelocityView = FeatureObject->InstanceData->ConstView(FeatureObject->VelocityHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 VelocityNum = VelocityView.Num<1>();

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FQuat RelativeRotation = RelativeRotationView[Instance];
				
			for (int32 VelocityIdx = 0; VelocityIdx < VelocityNum; VelocityIdx++)
			{
				const FVector Velocity = VelocityView[Instance][0];
				const FVector LocalVelocity = RelativeRotation.UnrotateVector(Velocity);
				const FVector Offset = UE::Learning::Agents::Debug::GridOffsetForIndex(VelocityIdx, VelocityNum);

				UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset,
					Actor->GetActorLocation() + Offset + Velocity,
					VisualLogColor.ToFColor(true),
					TEXT(""));

				UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset + Velocity,
					VisualLogColor.ToFColor(true),
					TEXT("Velocity %i: [% 6.3f % 6.3f % 6.3f]\nLocal Velocity: [% 6.3f % 6.3f % 6.3f]"),
					VelocityIdx,
					Velocity.X, Velocity.Y, Velocity.Z,
					LocalVelocity.X, LocalVelocity.Y, LocalVelocity.Z);
			}

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UScalarVelocityObservation* UScalarVelocityObservation::AddScalarVelocityObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UScalarVelocityObservation, UE::Learning::FScalarVelocityFeature>(InInteractor, Name, TEXT("AddScalarVelocityObservation"), 1, Scale);
}

void UScalarVelocityObservation::SetScalarVelocityObservation(const int32 AgentId, const float Velocity)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->VelocityHandle)[AgentId][0] = Velocity;
	AgentIteration[AgentId]++;
}

void UScalarVelocityObservation::SetScalarVelocityObservationWithAxis(const int32 AgentId, const FVector Velocity, const FVector Axis)
{
	SetScalarVelocityObservation(AgentId, Velocity.Dot(Axis));
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UScalarVelocityObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UScalarVelocityObservation::VisualLog);

	const TLearningArrayView<2, const float> VelocityView = FeatureObject->InstanceData->ConstView(FeatureObject->VelocityHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nVelocity: [% 6.1f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				VelocityView[Instance][0],
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif


UScalarVelocityArrayObservation* UScalarVelocityArrayObservation::AddScalarVelocityArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 VelocityNum, const float Scale)
{
	if (VelocityNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddScalarVelocityArrayObservation: Number of elements in array must be at least 1, got %i."), VelocityNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UScalarVelocityArrayObservation, UE::Learning::FScalarVelocityFeature>(InInteractor, Name, TEXT("AddScalarVelocityArrayObservation"), VelocityNum, Scale);
}

void UScalarVelocityArrayObservation::SetScalarVelocityArrayObservation(const int32 AgentId, const TArray<float>& Velocities)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> VelocityView = FeatureObject->InstanceData->View(FeatureObject->VelocityHandle);

	if (Velocities.Num() != VelocityView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), VelocityView.Num<1>(), Velocities.Num());
		return;
	}

	UE::Learning::Array::Copy<1, float>(VelocityView[AgentId], Velocities);
	AgentIteration[AgentId]++;
}

void UScalarVelocityArrayObservation::SetScalarVelocityArrayObservationWithAxis(const int32 AgentId, const TArray<FVector>& Velocities, const FVector Axis)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> VelocityView = FeatureObject->InstanceData->View(FeatureObject->VelocityHandle);

	if (Velocities.Num() != VelocityView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), VelocityView.Num<1>(), Velocities.Num());
		return;
	}

	for (int32 VelocityIdx = 0; VelocityIdx < VelocityView.Num<1>(); VelocityIdx++)
	{
		VelocityView[AgentId][VelocityIdx] = Velocities[VelocityIdx].Dot(Axis);
	}

	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UScalarVelocityArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UScalarVelocityArrayObservation::VisualLog);

	const TLearningArrayView<2, const float> VelocityView = FeatureObject->InstanceData->ConstView(FeatureObject->VelocityHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 VelocityNum = VelocityView.Num<1>();

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nVelocities: %s\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(VelocityView[Instance]),
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif


UPlanarVelocityObservation* UPlanarVelocityObservation::AddPlanarVelocityObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale, const FVector Axis0, const FVector Axis1)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UPlanarVelocityObservation, UE::Learning::FPlanarVelocityFeature>(
		InInteractor, 
		Name, 
		TEXT("AddPlanarVelocityObservation"), 
		1, 
		Scale, 
		Axis0.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector),
		Axis1.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
}

void UPlanarVelocityObservation::SetPlanarVelocityObservation(const int32 AgentId, const FVector Velocity, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->VelocityHandle)[AgentId][0] = Velocity;
	FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle)[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPlanarVelocityObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPlanarVelocityObservation::VisualLog);

	const TLearningArrayView<2, const FVector> VelocityView = FeatureObject->InstanceData->ConstView(FeatureObject->VelocityHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FVector Velocity = VelocityView[Instance][0];
			const FQuat RelativeRotation = RelativeRotationView[Instance];
			const FVector LocalVelocity = RelativeRotation.UnrotateVector(Velocity);

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

			UE_LEARNING_AGENTS_VLOG_PLANE(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				FeatureObject->Axis0,
				FeatureObject->Axis1,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif


UPlanarVelocityArrayObservation* UPlanarVelocityArrayObservation::AddPlanarVelocityArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 VelocityNum, const float Scale, const FVector Axis0, const FVector Axis1)
{
	if (VelocityNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddPlanarVelocityArrayObservation: Number of elements in array must be at least 1, got %i."), VelocityNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UPlanarVelocityArrayObservation, UE::Learning::FPlanarVelocityFeature>(
		InInteractor, 
		Name, 
		TEXT("AddPlanarVelocityArrayObservation"), 
		VelocityNum, 
		Scale, 
		Axis0.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector),
		Axis1.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
}

void UPlanarVelocityArrayObservation::SetPlanarVelocityArrayObservation(const int32 AgentId, const TArray<FVector>& Velocities, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, FVector> VelocityView = FeatureObject->InstanceData->View(FeatureObject->VelocityHandle);
	const TLearningArrayView<1, FQuat> RelativeRotationView = FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle);

	if (Velocities.Num() != VelocityView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), VelocityView.Num<1>(), Velocities.Num());
		return;
	}

	UE::Learning::Array::Copy<1, FVector>(VelocityView[AgentId], Velocities);
	RelativeRotationView[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UPlanarVelocityArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UPlanarVelocityArrayObservation::VisualLog);

	const TLearningArrayView<2, const FVector> VelocityView = FeatureObject->InstanceData->ConstView(FeatureObject->VelocityHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 VelocityNum = VelocityView.Num<1>();

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FQuat RelativeRotation = RelativeRotationView[Instance];

			for (int32 VelocityIdx = 0; VelocityIdx < VelocityNum; VelocityIdx++)
			{
				const FVector Velocity = VelocityView[Instance][0];
				const FVector LocalVelocity = RelativeRotation.UnrotateVector(Velocity);
				const FVector Offset = UE::Learning::Agents::Debug::GridOffsetForIndex(VelocityIdx, VelocityNum);

				UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
					Actor->GetActorLocation(),
					Actor->GetActorLocation() + Velocity,
					VisualLogColor.ToFColor(true),
					TEXT(""));

				UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
					Actor->GetActorLocation() + Velocity,
					VisualLogColor.ToFColor(true),
					TEXT("Velocity %i: [% 6.3f % 6.3f % 6.3f]\nLocal Velocity: [% 6.3f % 6.3f % 6.3f]"),
					VelocityIdx,
					Velocity.X, Velocity.Y, Velocity.Z,
					LocalVelocity.X, LocalVelocity.Y, LocalVelocity.Z);
			}

			UE_LEARNING_AGENTS_VLOG_PLANE(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				FeatureObject->Axis0,
				FeatureObject->Axis1,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
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

UAngularVelocityObservation* UAngularVelocityObservation::AddAngularVelocityObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UAngularVelocityObservation, UE::Learning::FAngularVelocityFeature>(InInteractor, Name, TEXT("AddAngularVelocityObservation"), 1, Scale);
}

void UAngularVelocityObservation::SetAngularVelocityObservation(const int32 AgentId, const FVector AngularVelocity, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->AngularVelocityHandle)[AgentId][0] = AngularVelocity;
	FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle)[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UAngularVelocityObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UAngularVelocityObservation::VisualLog);

	const TLearningArrayView<2, const FVector> AngularVelocityView = FeatureObject->InstanceData->ConstView(FeatureObject->AngularVelocityHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FVector AngularVelocity = AngularVelocityView[Instance][0];
			const FQuat RelativeRotation = RelativeRotationView[Instance];
			const FVector LocalAngularVelocity = RelativeRotation.UnrotateVector(AngularVelocity);

			UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
				Actor->GetActorLocation(),
				Actor->GetActorLocation() + AngularVelocity,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation() + AngularVelocity,
				VisualLogColor.ToFColor(true),
				TEXT("Angular Velocity: [% 6.3f % 6.3f % 6.3f]\nLocal Angular Velocity: [% 6.3f % 6.3f % 6.3f]"),
				AngularVelocity.X, AngularVelocity.Y, AngularVelocity.Z,
				LocalAngularVelocity.X, LocalAngularVelocity.Y, LocalAngularVelocity.Z);

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UAngularVelocityArrayObservation* UAngularVelocityArrayObservation::AddAngularVelocityArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 AngularVelocityNum, const float Scale)
{
	if (AngularVelocityNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddAngularVelocityArrayObservation: Number of elements in array must be at least 1, got %i."), AngularVelocityNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UAngularVelocityArrayObservation, UE::Learning::FAngularVelocityFeature>(InInteractor, Name, TEXT("AddAngularVelocityArrayObservation"), AngularVelocityNum, Scale);
}

void UAngularVelocityArrayObservation::SetAngularVelocityArrayObservation(const int32 AgentId, const TArray<FVector>& AngularVelocities, const FRotator RelativeRotation)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, FVector> AngularVelocityView = FeatureObject->InstanceData->View(FeatureObject->AngularVelocityHandle);
	const TLearningArrayView<1, FQuat> RelativeRotationView = FeatureObject->InstanceData->View(FeatureObject->RelativeRotationHandle);

	if (AngularVelocities.Num() != AngularVelocityView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), AngularVelocityView.Num<1>(), AngularVelocities.Num());
		return;
	}

	UE::Learning::Array::Copy<1, FVector>(AngularVelocityView[AgentId], AngularVelocities);
	RelativeRotationView[AgentId] = RelativeRotation.Quaternion();
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UAngularVelocityArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UAngularVelocityArrayObservation::VisualLog);

	const TLearningArrayView<2, const FVector> AngularVelocityView = FeatureObject->InstanceData->ConstView(FeatureObject->AngularVelocityHandle);
	const TLearningArrayView<1, const FQuat> RelativeRotationView = FeatureObject->InstanceData->ConstView(FeatureObject->RelativeRotationHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	const int32 AngularVelocityNum = AngularVelocityView.Num<1>();

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			const FQuat RelativeRotation = RelativeRotationView[Instance];

			for (int32 AngularVelocityIdx = 0; AngularVelocityIdx < AngularVelocityNum; AngularVelocityIdx++)
			{
				const FVector AngularVelocity = AngularVelocityView[Instance][0];
				const FVector LocalAngularVelocity = RelativeRotation.UnrotateVector(AngularVelocity);
				const FVector Offset = UE::Learning::Agents::Debug::GridOffsetForIndex(AngularVelocityIdx, AngularVelocityNum);

				UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset,
					Actor->GetActorLocation() + Offset + AngularVelocity,
					VisualLogColor.ToFColor(true),
					TEXT(""));

				UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
					Actor->GetActorLocation() + Offset + AngularVelocity,
					VisualLogColor.ToFColor(true),
					TEXT("Angular Velocity %i: [% 6.3f % 6.3f % 6.3f]\nLocal Angular Velocity: [% 6.3f % 6.3f % 6.3f]"),
					AngularVelocityIdx,
					AngularVelocity.X, AngularVelocity.Y, AngularVelocity.Z,
					LocalAngularVelocity.X, LocalAngularVelocity.Y, LocalAngularVelocity.Z);
			}

			UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
				Actor->GetActorLocation(),
				RelativeRotation,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif

UScalarAngularVelocityObservation* UScalarAngularVelocityObservation::AddScalarAngularVelocityObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const float Scale)
{
	return UE::Learning::Agents::Observations::Private::AddObservation<UScalarAngularVelocityObservation, UE::Learning::FScalarAngularVelocityFeature>(InInteractor, Name, TEXT("AddScalarAngularVelocityObservation"), 1, Scale);
}

void UScalarAngularVelocityObservation::SetScalarAngularVelocityObservation(const int32 AgentId, const float AngularVelocity)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	FeatureObject->InstanceData->View(FeatureObject->AngularVelocityHandle)[AgentId][0] = AngularVelocity;
	AgentIteration[AgentId]++;
}

void UScalarAngularVelocityObservation::SetScalarAngularVelocityObservationWithAxis(const int32 AgentId, const FVector AngularVelocity, const FVector Axis)
{
	SetScalarAngularVelocityObservation(AgentId, AngularVelocity.Dot(Axis));
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UScalarAngularVelocityObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UScalarAngularVelocityObservation::VisualLog);

	const TLearningArrayView<2, const float> AngularVelocityView = FeatureObject->InstanceData->ConstView(FeatureObject->AngularVelocityHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nAngular Velocity: [% 6.1f]\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				AngularVelocityView[Instance][0],
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif


UScalarAngularVelocityArrayObservation* UScalarAngularVelocityArrayObservation::AddScalarAngularVelocityArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name, const int32 AngularVelocityNum, const float Scale)
{
	if (AngularVelocityNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("AddScalarAngularVelocityArrayObservation: Number of elements in array must be at least 1, got %i."), AngularVelocityNum);
		return nullptr;
	}

	return UE::Learning::Agents::Observations::Private::AddObservation<UScalarAngularVelocityArrayObservation, UE::Learning::FScalarAngularVelocityFeature>(InInteractor, Name, TEXT("AddScalarAngularVelocityArrayObservation"), AngularVelocityNum, Scale);
}

void UScalarAngularVelocityArrayObservation::SetScalarAngularVelocityArrayObservation(const int32 AgentId, const TArray<float>& AngularVelocities)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> AngularVelocityView = FeatureObject->InstanceData->View(FeatureObject->AngularVelocityHandle);

	if (AngularVelocities.Num() != AngularVelocityView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), AngularVelocityView.Num<1>(), AngularVelocities.Num());
		return;
	}

	UE::Learning::Array::Copy<1, float>(AngularVelocityView[AgentId], AngularVelocities);
	AgentIteration[AgentId]++;
}

void UScalarAngularVelocityArrayObservation::SetScalarAngularVelocityArrayObservationWithAxis(const int32 AgentId, const TArray<FVector>& AngularVelocities, const FVector Axis)
{
	if (!Interactor->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	const TLearningArrayView<2, float> AngularVelocityView = FeatureObject->InstanceData->View(FeatureObject->AngularVelocityHandle);

	if (AngularVelocities.Num() != AngularVelocityView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Got wrong number of elements in array. Expected %i, got %i."), *GetName(), AngularVelocityView.Num<1>(), AngularVelocities.Num());
		return;
	}

	for (int32 AngularVelocityIdx = 0; AngularVelocityIdx < AngularVelocityView.Num<1>(); AngularVelocityIdx++)
	{
		AngularVelocityView[AgentId][AngularVelocityIdx] = AngularVelocities[AngularVelocityIdx].Dot(Axis);
	}
	AgentIteration[AgentId]++;
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void UScalarAngularVelocityArrayObservation::VisualLog(const UE::Learning::FIndexSet Instances) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(UScalarAngularVelocityArrayObservation::VisualLog);

	const TLearningArrayView<2, const float> AngularVelocityView = FeatureObject->InstanceData->ConstView(FeatureObject->AngularVelocityHandle);
	const TLearningArrayView<2, const float> FeatureView = FeatureObject->InstanceData->ConstView(FeatureObject->FeatureHandle);

	for (const int32 Instance : Instances)
	{
		if (const AActor* Actor = Cast<AActor>(Interactor->GetAgent(Instance)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nScale: [% 6.2f]\nAngular Velocities: %s\nEncoded: %s"),
				Instance,
				FeatureObject->Scale,
				*UE::Learning::Array::FormatFloat(AngularVelocityView[Instance]),
				*UE::Learning::Array::FormatFloat(FeatureView[Instance]));
		}
	}
}
#endif
