// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneEventSystems.h"
#include "Algo/RemoveIf.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneSpawnablesSystem.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"

#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "MovieSceneTracksComponentTypes.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"

#include "Templates/SubclassOf.h"
#include "Engine/World.h"
#include "Engine/LevelScriptActor.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneEventSystems)


#define LOCTEXT_NAMESPACE "MovieSceneEventSystem"

DECLARE_CYCLE_STAT(TEXT("Event Systems"),  MovieSceneEval_Events,        STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("Trigger Events"), MovieSceneEval_TriggerEvents, STATGROUP_MovieSceneECS);


UMovieSceneEventSystem::UMovieSceneEventSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
}

void UMovieSceneEventSystem::AddEvent(UE::MovieScene::FInstanceHandle RootInstance, const FMovieSceneEventTriggerData& TriggerData)
{
	check(TriggerData.Ptrs.Function != nullptr);
	EventsByRoot.FindOrAdd(RootInstance).Add(TriggerData);
}

bool UMovieSceneEventSystem::HasEvents() const
{
	return EventsByRoot.Num() != 0;
}

bool UMovieSceneEventSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	return HasEvents();
}

void UMovieSceneEventSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	if (EventsByRoot.Num() > 0)
	{
		TriggerAllEvents();
	}
}

void UMovieSceneEventSystem::OnUnlink()
{
	if (!ensure(EventsByRoot.Num() == 0))
	{
		EventsByRoot.Reset();
	}
}

void UMovieSceneEventSystem::TriggerAllEvents()
{
	using namespace UE::MovieScene;

	SCOPE_CYCLE_COUNTER(MovieSceneEval_Events);

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	struct FTriggerBatch
	{
		TArray<FMovieSceneEventTriggerData> Triggers;
		IMovieScenePlayer* Player;
	};
	TArray<FTriggerBatch> TriggerBatches;

	for (TPair<FInstanceHandle, TArray<FMovieSceneEventTriggerData>>& Pair : EventsByRoot)
	{
		const FSequenceInstance& RootInstance = InstanceRegistry->GetInstance(Pair.Key);
		IMovieScenePlayer* Player = RootInstance.GetPlayer();

		FFrameTime SkipUntil;
		const bool bSkipTrigger = Player->IsDisablingEventTriggers(SkipUntil);

		if (RootInstance.GetContext().GetDirection() == EPlayDirection::Forwards)
		{
			if (bSkipTrigger)
			{
				Algo::RemoveIf(Pair.Value, [SkipUntil](FMovieSceneEventTriggerData& Data) { return Data.RootTime <= SkipUntil; });
			}
			Algo::SortBy(Pair.Value, &FMovieSceneEventTriggerData::RootTime);
		}
		else
		{
			if (bSkipTrigger)
			{
				Algo::RemoveIf(Pair.Value, [SkipUntil](FMovieSceneEventTriggerData& Data) { return Data.RootTime >= SkipUntil; });
			}
			Algo::SortBy(Pair.Value, &FMovieSceneEventTriggerData::RootTime, TGreater<>());
		}

		FTriggerBatch& TriggerBatch = TriggerBatches.Emplace_GetRef();
		TriggerBatch.Triggers = Pair.Value;
		TriggerBatch.Player = Player;
	}

	// We need to clean our state before actually triggering the events because one of those events could
	// call back into an evaluation (for instance, by starting play on another sequence). If we don't clean
	// this before, would would re-enter and re-trigger past events, resulting in an infinite loop!
	EventsByRoot.Empty();

	for (const FTriggerBatch& TriggerBatch : TriggerBatches)
	{
		TriggerEvents(TriggerBatch.Triggers, TriggerBatch.Player);
	}
}

void UMovieSceneEventSystem::TriggerEvents(TArrayView<const FMovieSceneEventTriggerData> Events, IMovieScenePlayer* Player)
{
#if !NO_LOGGING
	UMovieSceneSequence* PlayerSequence = Player->GetEvaluationTemplate().GetRootSequence();
	FString SequenceName = PlayerSequence->GetName();
	UE_LOG(LogMovieScene, VeryVerbose, TEXT("%s: Triggering %d events"), *SequenceName, Events.Num());
#endif

	TArray<UObject*> GlobalContexts = Player->GetEventContexts();

	for (const FMovieSceneEventTriggerData& Event : Events)
	{
		SCOPE_CYCLE_COUNTER(MovieSceneEval_TriggerEvents);

		UObject* DirectorInstance = Player->GetEvaluationTemplate().GetOrCreateDirectorInstance(Event.SequenceID, *Player);
		if (!DirectorInstance)
		{
#if !NO_LOGGING
			UE_LOG(LogMovieScene, Warning, TEXT("Failed to trigger event '%s' because no director instance was available."), *Event.Ptrs.Function->GetName());
#endif
			continue;
		}

#if !NO_LOGGING
		UE_LOG(LogMovieScene, VeryVerbose, TEXT("%s: - Triggering event at frame %d, subframe %f"), *SequenceName, Event.RootTime.FrameNumber.Value, Event.RootTime.GetSubFrame());
#endif

#if WITH_EDITOR
		const static FName NAME_CallInEditor(TEXT("CallInEditor"));

		UWorld* World = DirectorInstance->GetWorld();
		const bool bIsGameWorld = World && World->IsGameWorld();
#endif // WITH_EDITOR



#if WITH_EDITOR
		if (!bIsGameWorld && !Event.Ptrs.Function->HasMetaData(NAME_CallInEditor))
		{
			UE_LOG(LogMovieScene, Verbose, TEXT("Refusing to trigger event '%s' in editor world when 'Call in Editor' is false."), *Event.Ptrs.Function->GetName());
			continue;
		}
#endif // WITH_EDITOR


		UE_LOG(LogMovieScene, VeryVerbose, TEXT("Triggering event '%s'."), *Event.Ptrs.Function->GetName());

		if (Event.Ptrs.Function->NumParms == 0 && !Event.ObjectBindingID.IsValid())
		{
			DirectorInstance->ProcessEvent(Event.Ptrs.Function, nullptr);
		}
		else
		{
			TriggerEventWithParameters(DirectorInstance, Event, GlobalContexts, Player);
		}
	}
}

void UMovieSceneEventSystem::TriggerEventWithParameters(UObject* DirectorInstance, const FMovieSceneEventTriggerData& Event, TArrayView<UObject* const> GlobalContexts, IMovieScenePlayer* Player)
{
	const FMovieSceneEventPtrs& EventPtrs = Event.Ptrs;
	if (!ensureMsgf(!EventPtrs.BoundObjectProperty.Get() || (EventPtrs.BoundObjectProperty->GetOwner<UObject>() == EventPtrs.Function && EventPtrs.BoundObjectProperty->GetOffset_ForUFunction() < EventPtrs.Function->ParmsSize), TEXT("Bound object property belongs to the wrong function or has an offset greater than the parameter size! This should never happen and indicates a BP compilation or nativization error.")))
	{
		return;
	}

	// Parse all function parameters.
	uint8* Parameters = (uint8*)FMemory_Alloca(EventPtrs.Function->ParmsSize + EventPtrs.Function->MinAlignment);
	Parameters = Align(Parameters, EventPtrs.Function->MinAlignment);

	// Mem zero the parameter list
	FMemory::Memzero(Parameters, EventPtrs.Function->ParmsSize);

	// Initialize all CPF_Param properties - these are aways at the head of the list
	for (TFieldIterator<FProperty> It(EventPtrs.Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* LocalProp = *It;
		checkSlow(LocalProp);
		if (!LocalProp->HasAnyPropertyFlags(CPF_ZeroConstructor))
		{
			LocalProp->InitializeValue_InContainer(Parameters);
		}
	}

	FProperty* BoundObjectProperty = EventPtrs.BoundObjectProperty.Get();

	// If the event exists on an object binding, only call the events for those bindings (never for the global contexts)
	if (Event.ObjectBindingID.IsValid())
	{
		for (TWeakObjectPtr<> WeakBoundObject : Player->FindBoundObjects(Event.ObjectBindingID, Event.SequenceID))
		{
			if (UObject* BoundObject = WeakBoundObject.Get())
			{
				// Attempt to bind the object to the function parameters
				if (PatchBoundObject(Parameters, BoundObject, EventPtrs.BoundObjectProperty.Get(), Player, Event.SequenceID))
				{
					DirectorInstance->ProcessEvent(EventPtrs.Function, Parameters);
				}
			}
		}
	}

	// At this point we know the event is a master track with parameters - either trigger for global contexts, or just on its own
	else if (GlobalContexts.Num() != 0)
	{
		for (UObject* Context : GlobalContexts)
		{
			if (PatchBoundObject(Parameters, Context, EventPtrs.BoundObjectProperty.Get(), Player, Event.SequenceID))
			{
				DirectorInstance->ProcessEvent(EventPtrs.Function, Parameters);
			}
		}
	}
	else
	{
		DirectorInstance->ProcessEvent(EventPtrs.Function, Parameters);
	}

	// Destroy all parameter properties one by one
	for (TFieldIterator<FProperty> It(EventPtrs.Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Parameters);
	}
}

bool UMovieSceneEventSystem::PatchBoundObject(uint8* Parameters, UObject* BoundObject, FProperty* BoundObjectProperty, IMovieScenePlayer* Player, FMovieSceneSequenceID SequenceID)
{
	if (!BoundObjectProperty)
	{
		return true;
	}

	if (FInterfaceProperty* InterfaceParameter = CastField<FInterfaceProperty>(BoundObjectProperty))
		{
		if (BoundObject->GetClass()->ImplementsInterface(InterfaceParameter->InterfaceClass))
		{
			FScriptInterface Interface;
			Interface.SetObject(BoundObject);
			Interface.SetInterface(BoundObject->GetInterfaceAddress(InterfaceParameter->InterfaceClass));
			InterfaceParameter->SetPropertyValue_InContainer(Parameters, Interface);
			return true;
		}

		FMessageLog("PIE").Warning()
			->AddToken(FUObjectToken::Create(BoundObjectProperty->GetOwnerUObject()))
			->AddToken(FUObjectToken::Create(Player->GetEvaluationTemplate().GetSequence(SequenceID)))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("LevelBP_InterfaceNotImplemented_Error", "Failed to trigger event because it does not implement the necessary interface. Function expects a '{0}'."), FText::FromName(InterfaceParameter->InterfaceClass->GetFName()))));
		return false;
	}

	if (FObjectProperty* ObjectParameter = CastField<FObjectProperty>(BoundObjectProperty))
	{
		if (BoundObject->IsA<ALevelScriptActor>())
		{
			FMessageLog("PIE").Warning()
				->AddToken(FUObjectToken::Create(BoundObjectProperty->GetOwnerUObject()))
				->AddToken(FUObjectToken::Create(Player->GetEvaluationTemplate().GetSequence(SequenceID)))
				->AddToken(FTextToken::Create(LOCTEXT("LevelBP_LevelScriptActor_Error", "Failed to trigger event: only Interface pins are supported for master tracks within Level Sequences. Please remove the pin, or change it to an interface that is implemented on the desired level blueprint.")));

			return false;
		}
		else if (!BoundObject->IsA(ObjectParameter->PropertyClass))
		{
			FMessageLog("PIE").Warning()
				->AddToken(FUObjectToken::Create(Player->GetEvaluationTemplate().GetSequence(SequenceID)))
				->AddToken(FUObjectToken::Create(BoundObjectProperty->GetOwnerUObject()))
				->AddToken(FUObjectToken::Create(BoundObject))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("LevelBP_InvalidCast_Error", "Failed to trigger event: Cast to {0} failed."), FText::FromName(ObjectParameter->PropertyClass->GetFName()))));

			return false;
		}

		ObjectParameter->SetObjectPropertyValue_InContainer(Parameters, BoundObject);
		return true;
	}

	FMessageLog("PIE").Warning()
		->AddToken(FUObjectToken::Create(Player->GetEvaluationTemplate().GetSequence(SequenceID)))
		->AddToken(FUObjectToken::Create(BoundObjectProperty->GetOwnerUObject()))
		->AddToken(FUObjectToken::Create(BoundObject))
		->AddToken(FTextToken::Create(FText::Format(LOCTEXT("LevelBP_UnsupportedProperty_Error", "Failed to trigger event: Unsupported property type for bound object: {0}."), FText::FromName(BoundObjectProperty->GetClass()->GetFName()))));
	return false;
}


UMovieScenePreSpawnEventSystem::UMovieScenePreSpawnEventSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	Phase = UE::MovieScene::ESystemPhase::Spawn;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(GetClass(), UMovieSceneSpawnablesSystem::StaticClass());
	}
}

UMovieScenePostSpawnEventSystem::UMovieScenePostSpawnEventSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	Phase = UE::MovieScene::ESystemPhase::Spawn;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(UMovieSceneSpawnablesSystem::StaticClass(), GetClass());
	}
}

UMovieScenePostEvalEventSystem::UMovieScenePostEvalEventSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	Phase = UE::MovieScene::ESystemPhase::Finalization;
}

void UMovieScenePostEvalEventSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	if (HasEvents())
	{
		FMovieSceneEntitySystemRunner* Runner = Linker->GetActiveRunner();
		if (ensure(Runner))
		{
			Runner->GetQueuedEventTriggers().AddUObject(this, &UMovieScenePostEvalEventSystem::TriggerAllEvents);
		}
	}
}

#undef LOCTEXT_NAMESPACE

