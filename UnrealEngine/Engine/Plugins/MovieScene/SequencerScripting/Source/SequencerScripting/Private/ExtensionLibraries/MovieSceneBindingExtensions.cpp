// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExtensionLibraries/MovieSceneBindingExtensions.h"
#include "ExtensionLibraries/MovieSceneSequenceExtensions.h"
#include "MovieSceneBindingProxy.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "Algo/Find.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneBindingExtensions)

bool UMovieSceneBindingExtensions::IsValid(const FMovieSceneBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.Sequence ? InBinding.Sequence->GetMovieScene() : nullptr;
	if (MovieScene && InBinding.BindingID.IsValid())
	{
		return Algo::FindBy(MovieScene->GetBindings(), InBinding.BindingID, &FMovieSceneBinding::GetObjectGuid) != nullptr;
	}

	return false;
}

FGuid UMovieSceneBindingExtensions::GetId(const FMovieSceneBindingProxy& InBinding)
{
	return InBinding.BindingID;
}

FText UMovieSceneBindingExtensions::GetDisplayName(const FMovieSceneBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.Sequence ? InBinding.Sequence->GetMovieScene() : nullptr;
	if (MovieScene && InBinding.BindingID.IsValid())
	{
		return MovieScene->GetObjectDisplayName(InBinding.BindingID);
	}

	return FText();
}

void UMovieSceneBindingExtensions::SetDisplayName(const FMovieSceneBindingProxy& InBinding, const FText& InDisplayName)
{
	UMovieScene* MovieScene = InBinding.Sequence ? InBinding.Sequence->GetMovieScene() : nullptr;
	if (MovieScene && InBinding.BindingID.IsValid())
	{
#if WITH_EDITORONLY_DATA
		MovieScene->Modify();
		MovieScene->SetObjectDisplayName(InBinding.BindingID, InDisplayName);
#endif
	}
}

FString UMovieSceneBindingExtensions::GetName(const FMovieSceneBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.Sequence ? InBinding.Sequence->GetMovieScene() : nullptr;
	if (MovieScene && InBinding.BindingID.IsValid())
	{
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(InBinding.BindingID);
		if (Spawnable)
		{
			return Spawnable->GetName();
		}

		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(InBinding.BindingID);
		if (Possessable)
		{
			return Possessable->GetName();
		}
	}

	return FString();
}

void UMovieSceneBindingExtensions::SetName(const FMovieSceneBindingProxy& InBinding, const FString& InName)
{
	UMovieScene* MovieScene = InBinding.Sequence ? InBinding.Sequence->GetMovieScene() : nullptr;
	if (MovieScene && InBinding.BindingID.IsValid())
	{
		MovieScene->Modify();

		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(InBinding.BindingID);
		if (Spawnable)
		{
			Spawnable->SetName(InName);
		}

		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(InBinding.BindingID);
		if (Possessable)
		{
			Possessable->SetName(InName);
		}
	}
}

TArray<UMovieSceneTrack*> UMovieSceneBindingExtensions::GetTracks(const FMovieSceneBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		const FMovieSceneBinding* Binding = Algo::FindBy(MovieScene->GetBindings(), InBinding.BindingID, &FMovieSceneBinding::GetObjectGuid);
		if (Binding)
		{
			return Binding->GetTracks();
		}
	}
	return TArray<UMovieSceneTrack*>();
}

void UMovieSceneBindingExtensions::RemoveTrack(const FMovieSceneBindingProxy& InBinding, UMovieSceneTrack* TrackToRemove)
{
	if (!TrackToRemove)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot call RemoveTrack on a null track"), ELogVerbosity::Error);
		return;
	}

	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (TrackToRemove && MovieScene)
	{
		MovieScene->RemoveTrack(*TrackToRemove);
	}
}

void UMovieSceneBindingExtensions::Remove(const FMovieSceneBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		if (!MovieScene->RemovePossessable(InBinding.BindingID))
		{
			MovieScene->RemoveSpawnable(InBinding.BindingID);
		}
	}
}

TArray<UMovieSceneTrack*> UMovieSceneBindingExtensions::FindTracksByType(const FMovieSceneBindingProxy& InBinding, TSubclassOf<UMovieSceneTrack> TrackType)
{
	UMovieScene* MovieScene   = InBinding.GetMovieScene();
	UClass*      DesiredClass = TrackType.Get();

	if (MovieScene)
	{
		const FMovieSceneBinding* Binding = Algo::FindBy(MovieScene->GetBindings(), InBinding.BindingID, &FMovieSceneBinding::GetObjectGuid);
		if (Binding)
		{
			bool bExactMatch = false;
			return UMovieSceneSequenceExtensions::FilterTracks(Binding->GetTracks(), DesiredClass, bExactMatch);
		}
	}
	return TArray<UMovieSceneTrack*>();
}

TArray<UMovieSceneTrack*> UMovieSceneBindingExtensions::FindTracksByExactType(const FMovieSceneBindingProxy& InBinding, TSubclassOf<UMovieSceneTrack> TrackType)
{
	UMovieScene* MovieScene   = InBinding.GetMovieScene();
	UClass*      DesiredClass = TrackType.Get();

	if (MovieScene)
	{
		const FMovieSceneBinding* Binding = Algo::FindBy(MovieScene->GetBindings(), InBinding.BindingID, &FMovieSceneBinding::GetObjectGuid);
		if (Binding)
		{
			bool bExactMatch = true;
			return UMovieSceneSequenceExtensions::FilterTracks(Binding->GetTracks(), DesiredClass, bExactMatch);
		}
	}
	return TArray<UMovieSceneTrack*>();
}

UMovieSceneTrack* UMovieSceneBindingExtensions::AddTrack(const FMovieSceneBindingProxy& InBinding, TSubclassOf<UMovieSceneTrack> TrackType)
{
	UMovieScene* MovieScene   = InBinding.GetMovieScene();
	UClass*      DesiredClass = TrackType.Get();

	if (MovieScene)
	{
		const bool bBindingExists = Algo::FindBy(MovieScene->GetBindings(), InBinding.BindingID, &FMovieSceneBinding::GetObjectGuid) != nullptr;
		if (bBindingExists)
		{
			UMovieSceneTrack* NewTrack = NewObject<UMovieSceneTrack>(MovieScene, DesiredClass, NAME_None, RF_Transactional);
			if (NewTrack)
			{
				MovieScene->AddGivenTrack(NewTrack, InBinding.BindingID);
				return NewTrack;
			}
		}
	}
	return nullptr;
}

TArray<FMovieSceneBindingProxy> UMovieSceneBindingExtensions::GetChildPossessables(const FMovieSceneBindingProxy& InBinding)
{
	TArray<FMovieSceneBindingProxy> Result;

	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(InBinding.BindingID);
		if (Spawnable)
		{
			for (const FGuid& ChildGuid : Spawnable->GetChildPossessables())
			{
				Result.Emplace(ChildGuid, InBinding.Sequence);
			}
			return Result;
		}

		const int32 Count = MovieScene->GetPossessableCount();
		for (int32 i = 0; i < Count; ++i)
		{
			FMovieScenePossessable& PossessableChild = MovieScene->GetPossessable(i);
			if (PossessableChild.GetParent() == InBinding.BindingID)
			{
				Result.Emplace(PossessableChild.GetGuid(), InBinding.Sequence);
			}
		}
	}
	return Result;
}

UObject* UMovieSceneBindingExtensions::GetObjectTemplate(const FMovieSceneBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(InBinding.BindingID);
		if (Spawnable)
		{
			return Spawnable->GetObjectTemplate();
		}
	}
	return nullptr;
}

UClass* UMovieSceneBindingExtensions::GetPossessedObjectClass(const FMovieSceneBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(InBinding.BindingID);
		if (Possessable)
		{
#if WITH_EDITORONLY_DATA
			return const_cast<UClass*>(Possessable->GetPossessedObjectClass());
#endif
		}
	}
	return nullptr;
}

FMovieSceneBindingProxy UMovieSceneBindingExtensions::GetParent(const FMovieSceneBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(InBinding.BindingID);
		if (Possessable)
		{
			return FMovieSceneBindingProxy(Possessable->GetParent(), InBinding.Sequence);
		}
	}
	return FMovieSceneBindingProxy();
}


void UMovieSceneBindingExtensions::SetParent(const FMovieSceneBindingProxy& InBinding, const FMovieSceneBindingProxy& InParentBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(InBinding.BindingID);
		if (Possessable)
		{
			MovieScene->Modify();
			Possessable->SetParent(InParentBinding.BindingID, MovieScene);
		}
	}
}

void UMovieSceneBindingExtensions::MoveBindingContents(const FMovieSceneBindingProxy& SourceBindingId, const FMovieSceneBindingProxy& DestinationBindingId)
{
	UMovieScene* MovieScene = SourceBindingId.GetMovieScene();
	if (MovieScene)
	{
		MovieScene->Modify();

		FMovieScenePossessable* SourcePossessable = MovieScene->FindPossessable(SourceBindingId.BindingID);
		FMovieSceneSpawnable* SourceSpawnable = MovieScene->FindSpawnable(SourceBindingId.BindingID);

		FMovieScenePossessable* DestinationPossessable = MovieScene->FindPossessable(DestinationBindingId.BindingID);
		FMovieSceneSpawnable* DestinationSpawnable = MovieScene->FindSpawnable(DestinationBindingId.BindingID);

		if (SourcePossessable && DestinationPossessable)
		{
			MovieScene->MoveBindingContents(SourcePossessable->GetGuid(), DestinationPossessable->GetGuid());
		}
		else if (SourcePossessable && DestinationSpawnable)
		{
			MovieScene->MoveBindingContents(SourcePossessable->GetGuid(), DestinationSpawnable->GetGuid());
		}
		else if (SourceSpawnable && DestinationPossessable)
		{
			MovieScene->MoveBindingContents(SourceSpawnable->GetGuid(), DestinationPossessable->GetGuid());
		}
		else if (SourceSpawnable && DestinationSpawnable)
		{
			MovieScene->MoveBindingContents(SourceSpawnable->GetGuid(), DestinationSpawnable->GetGuid());
		}
	}
}

