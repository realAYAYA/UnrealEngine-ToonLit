// Copyright Epic Games, Inc. All Rights Reserved.

#include "TemplateSequence.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieSceneSpawnTrack.h"

#include "TemplateSequenceActor.h"
#include "TemplateSequencePlayer.h"

#include "Compilation/MovieSceneCompiledDataManager.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/MovieScenePlayback.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TemplateSequence)

IMPLEMENT_MODULE(FDefaultModuleImpl, TemplateSequence);

DEFINE_LOG_CATEGORY(LogTemplateSequence);

UTemplateSequence::UTemplateSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MovieScene(nullptr)
{
	bParentContextsAreSignificant = true;
}

void UTemplateSequence::Initialize()
{
	MovieScene = NewObject<UMovieScene>(this, NAME_None, RF_Transactional);

	MovieScene->SetEvaluationType(EMovieSceneEvaluationType::WithSubFrames);

	FFrameRate TickResolution(24000, 1);
	MovieScene->SetTickResolutionDirectly(TickResolution);

	FFrameRate DisplayRate(30, 1);
	MovieScene->SetDisplayRate(DisplayRate);
}

FGuid UTemplateSequence::GetRootObjectBindingID() const
{
	if (MovieScene != nullptr && MovieScene->GetSpawnableCount() > 0)
	{
		const FMovieSceneSpawnable& FirstSpawnable = MovieScene->GetSpawnable(0);
		return FirstSpawnable.GetGuid();
	}

	return FGuid();
}

const UObject* UTemplateSequence::GetRootObjectSpawnableTemplate() const
{
	if (MovieScene != nullptr && MovieScene->GetSpawnableCount() > 0)
	{
		const FMovieSceneSpawnable& FirstSpawnable = MovieScene->GetSpawnable(0);
		return FirstSpawnable.GetObjectTemplate();
	}
	return nullptr;
}

void UTemplateSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
	if (UActorComponent* Component = Cast<UActorComponent>(&PossessedObject))
	{
		const FName ComponentName = Component->GetFName();
		BoundActorComponents.Add(ObjectId, ComponentName);
	}
}

bool UTemplateSequence::CanPossessObject(UObject& Object, UObject* InPlaybackContext) const
{
	return Object.IsA<AActor>() || Object.IsA<UActorComponent>();
}

void UTemplateSequence::LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	AActor* Actor = Cast<AActor>(Context);
	if (Actor == nullptr)
	{
		return;
	}

	const FName* ComponentName = BoundActorComponents.Find(ObjectId);
	if (ComponentName == nullptr)
	{
		return;
	}

	if (UActorComponent* FoundComponent = FindObject<UActorComponent>(Actor, *ComponentName->ToString(), false))
	{
		OutObjects.Add(FoundComponent);
	}
}

UMovieScene* UTemplateSequence::GetMovieScene() const
{
	return MovieScene;
}

UObject* UTemplateSequence::GetParentObject(UObject* Object) const
{
	if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		return Component->GetOwner();
	}

	return nullptr;
}

void UTemplateSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
	BoundActorComponents.Remove(ObjectId);
}

void UTemplateSequence::UnbindObjects(const FGuid& ObjectId, const TArray<UObject*>& InObjects, UObject* Context)
{
	BoundActorComponents.Remove(ObjectId);
}

void UTemplateSequence::UnbindInvalidObjects(const FGuid& ObjectId, UObject* Context)
{
	BoundActorComponents.Remove(ObjectId);
}

FGuid UTemplateSequence::FindOrAddBinding(UObject* InObject)
{
	UObject* ParentObject = GetParentObject(InObject);
	FGuid    ParentGuid   = ParentObject ? FindOrAddBinding(ParentObject) : FGuid();

	if (ParentObject && !ParentGuid.IsValid())
	{
		UE_LOG(LogTemplateSequence, Error, TEXT("Unable to possess object '%s' because it's parent could not be bound."), *InObject->GetName());
		return FGuid();
	}

	// Perform a potentially slow lookup of every possessable binding in the sequence to see if we already have this
	UWorld* World = InObject->GetWorld();
	ensure(World);
	{
		ATemplateSequenceActor* OutActor = nullptr;
		UTemplateSequencePlayer* Player = UTemplateSequencePlayer::CreateTemplateSequencePlayer(World, this, FMovieSceneSequencePlaybackSettings(), OutActor);
		ensure(Player);
		
		// Don't override the default, the default assignment should remain bound so that objects will spawn
		OutActor->BindingOverride.bOverridesDefault = false;

		Player->Initialize(this, World, FMovieSceneSequencePlaybackSettings());
		Player->State.AssignSequence(MovieSceneSequenceID::Root, *this, *Player);
		Player->PlayTo(FMovieSceneSequencePlaybackParams(MovieScene->GetPlaybackRange().GetLowerBoundValue(), EUpdatePositionMethod::Play), FMovieSceneSequencePlayToParams());

		FGuid FoundBinding;
		for (int32 BindingIndex = 0; !FoundBinding.IsValid() && BindingIndex < GetMovieScene()->GetBindings().Num(); ++BindingIndex)
		{
			for (TWeakObjectPtr<UObject> BoundObject : Player->FindBoundObjects(GetMovieScene()->GetBindings()[BindingIndex].GetObjectGuid(), MovieSceneSequenceID::Root))
			{
				if (BoundObject.IsValid() && BoundObject.Get()->GetClass() == InObject->GetClass())
				{
					if (!ParentObject || BoundObject.Get()->GetName() == InObject->GetName())
					{
						FoundBinding = GetMovieScene()->GetBindings()[BindingIndex].GetObjectGuid();
						break;
					}
				}
			}
		}
		
		if (!FoundBinding.IsValid())
		{
			FGuid ExistingID = Player->FindObjectId(*InObject, MovieSceneSequenceID::Root);
			if (ExistingID.IsValid())
			{
				FoundBinding = ExistingID;
			}
		}

		Player->Stop();
		if (OutActor)
		{
			World->DestroyActor(OutActor);
		}

		if (FoundBinding.IsValid())
		{
			return FoundBinding;
		}
	}

	const FGuid NewGuid = MovieScene->AddPossessable(InObject->GetName(), InObject->GetClass());
	
	// Set up parent/child guids for possessables within spawnables
	if (ParentGuid.IsValid())
	{
		FMovieScenePossessable* ChildPossessable = MovieScene->FindPossessable(NewGuid);
		if (ensure(ChildPossessable))
		{
			ChildPossessable->SetParent(ParentGuid, MovieScene);
		}

		FMovieSceneSpawnable* ParentSpawnable = MovieScene->FindSpawnable(ParentGuid);
		if (ParentSpawnable)
		{
			ParentSpawnable->AddChildPossessable(NewGuid);
		}
	}

	BindPossessableObject(NewGuid, *InObject, ParentObject);

	return NewGuid;
}

FGuid UTemplateSequence::CreatePossessable(UObject* ObjectToPossess)
{
	return FindOrAddBinding(ObjectToPossess);
}

bool UTemplateSequence::AllowsSpawnableObjects() const
{
	return true;
}

UObject* UTemplateSequence::MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName)
{
	return MovieSceneHelpers::MakeSpawnableTemplateFromInstance(InSourceObject, MovieScene, ObjectName);
}

void UTemplateSequence::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	if (BoundActorClass.IsValid())
	{
		FAssetRegistryTag Tag("BoundActorClass", BoundActorClass->GetPathName(), FAssetRegistryTag::TT_Alphabetical);
		OutTags.Add(Tag);
	}
	else
	{
		OutTags.Emplace("BoundActorClass", "(None)", FAssetRegistryTag::TT_Alphabetical);
	}
}

#if WITH_EDITOR

void UTemplateSequence::PostLoadAssetRegistryTags(const FAssetData& InAssetData, TArray<FAssetRegistryTag>& OutTagsAndValuesToUpdate) const
{
	Super::PostLoadAssetRegistryTags(InAssetData, OutTagsAndValuesToUpdate);

	static const FName BoundActorClassTagName(TEXT("BoundActorClass"));
	FString BoundActorClassTagValue = InAssetData.GetTagValueRef<FString>(BoundActorClassTagName);
	if (!BoundActorClassTagValue.IsEmpty() && FPackageName::IsShortPackageName(BoundActorClassTagValue))
	{
		FTopLevelAssetPath BoundActorClassPathName = UClass::TryConvertShortTypeNameToPathName<UStruct>(BoundActorClassTagValue, ELogVerbosity::Warning, TEXT("UTemplateSequence::PostLoadAssetRegistryTags"));
		if (!BoundActorClassPathName.IsNull())
		{
			OutTagsAndValuesToUpdate.Add(FAssetRegistryTag(BoundActorClassTagName, BoundActorClassPathName.ToString(), FAssetRegistryTag::TT_Alphabetical));
		}
	}
}

FText UTemplateSequence::GetDisplayName() const
{
	return UMovieSceneSequence::GetDisplayName();
}

ETrackSupport UTemplateSequence::IsTrackSupported(TSubclassOf<class UMovieSceneTrack> InTrackClass) const
{
	if (InTrackClass == UMovieSceneSpawnTrack::StaticClass())
	{
		return ETrackSupport::Supported;
	}

	return Super::IsTrackSupported(InTrackClass);
}

void UTemplateSequence::GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const
{
	Super::GetAssetRegistryTagMetadata(OutMetadata);

	OutMetadata.Add(
		"BoundActorClass",
		FAssetRegistryTagMetadata()
			.SetDisplayName(NSLOCTEXT("TemplateSequence", "BoundActorClass_Label", "Bound Actor Class"))
			.SetTooltip(NSLOCTEXT("TemplateSequence", "BoundActorClass_Tooltip", "The type of actor bound to this template sequence"))
		);
}

#endif

