// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSequence.h"

#include "Evaluation/MovieSceneEvaluationCustomVersion.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "MovieScene.h"
#include "MovieSceneBindingReferences.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/ReleaseObjectVersion.h"
#include "UObject/ObjectSaveContext.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sections/MovieSceneSubSection.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Interfaces/ITargetPlatform.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntityManager.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "UniversalObjectLocator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneSequence)


UMovieSceneSequence::UMovieSceneSequence(const FObjectInitializer& Init)
	: Super(Init)
{
	bParentContextsAreSignificant = false;
	bPlayableDirectly = true;
	SequenceFlags = EMovieSceneSequenceFlags::None;
	CompiledData = nullptr;

	// Ensure that the precompiled data is set up when constructing the CDO. This guarantees that we do not try and create it for the first time when collecting garbage
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UMovieSceneCompiledDataManager::GetPrecompiledData();

#if WITH_EDITOR
		UMovieSceneCompiledDataManager::GetPrecompiledData(EMovieSceneServerClientMask::Client);
		UMovieSceneCompiledDataManager::GetPrecompiledData(EMovieSceneServerClientMask::Server);
#endif
	}
}

bool UMovieSceneSequence::MakeLocatorForObject(UObject* Object, UObject* Context, FUniversalObjectLocator& OutLocator) const
{
	if (CanPossessObject(*Object, Context))
	{
		OutLocator.Reset(Object, Context);
		return true;
	}

	return false;
}

const FMovieSceneBindingReferences* UMovieSceneSequence::GetBindingReferences() const
{
	return nullptr;
}

FMovieSceneBindingReferences* UMovieSceneSequence::GetBindingReferences()
{
	const FMovieSceneBindingReferences* Result = const_cast<const UMovieSceneSequence*>(this)->GetBindingReferences();
	return const_cast<FMovieSceneBindingReferences*>(Result);
}

void UMovieSceneSequence::UnloadBoundObject(const UE::UniversalObjectLocator::FResolveParams& ResolveParams, const FGuid& ObjectId, int32 BindingIndex)
{
	FMovieSceneBindingReferences* Refs = GetBindingReferences();
	if (Refs)
	{
		Refs->UnloadBoundObject(ResolveParams, ObjectId, BindingIndex);
	}
}

void UMovieSceneSequence::LocateBoundObjects(const FGuid& ObjectId, const UE::UniversalObjectLocator::FResolveParams& ResolveParams, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	const FMovieSceneBindingReferences* Refs = GetBindingReferences();
	if (Refs)
	{
		Refs->ResolveBinding(ObjectId, ResolveParams, OutObjects);
	}
	else
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		LocateBoundObjects(ObjectId, const_cast<UObject*>(ResolveParams.Context), OutObjects);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}

void UMovieSceneSequence::PostLoad()
{
	UMovieSceneCompiledDataManager* PrecompiledData = UMovieSceneCompiledDataManager::GetPrecompiledData();

#if WITH_EDITORONLY_DATA
	// Wipe compiled data on editor load to ensure we don't try and iteratively compile previously saved content. In a cooked game, this will contain our up-to-date compiled template.
	PrecompiledData->Reset(this);
#endif

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		PrecompiledData->LoadCompiledData(this);

#if !WITH_EDITOR
		// Don't need this any more - allow it to be GC'd so it doesn't take up memory
		CompiledData = nullptr;
#else
		// Wipe out in -game as well
		if (!GIsEditor)
		{
			CompiledData = nullptr;
		}
#endif
	}

#if DO_CHECK
	if (FPlatformProperties::RequiresCookedData() && !EnumHasAnyFlags(SequenceFlags, EMovieSceneSequenceFlags::Volatile) && !HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject))
	{
		ensureAlwaysMsgf(PrecompiledData->FindDataID(this).IsValid(), TEXT("No precompiled movie scene data is present for sequence '%s'. This should have been generated and saved during cook."), *GetName());
	}
#endif

	Super::PostLoad();
}

void UMovieSceneSequence::BeginDestroy()
{
	Super::BeginDestroy();

	if (!GExitPurge && !HasAnyFlags(RF_ClassDefaultObject))
	{
		UMovieSceneCompiledDataManager::GetPrecompiledData()->Reset(this);

#if WITH_EDITOR
		UMovieSceneCompiledDataManager::GetPrecompiledData(EMovieSceneServerClientMask::Client)->Reset(this);
		UMovieSceneCompiledDataManager::GetPrecompiledData(EMovieSceneServerClientMask::Server)->Reset(this);
#endif
	}
}

void UMovieSceneSequence::PostDuplicate(bool bDuplicateForPIE)
{
	if (bDuplicateForPIE)
	{
		UMovieSceneCompiledDataManager::GetPrecompiledData()->Compile(this);
	}

	Super::PostDuplicate(bDuplicateForPIE);
}

EMovieSceneServerClientMask UMovieSceneSequence::OverrideNetworkMask(EMovieSceneServerClientMask InDefaultMask) const
{
	return InDefaultMask;
}

void UMovieSceneSequence::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
#if WITH_EDITOR
	if (!HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject))
	{
		const ITargetPlatform* TargetPlatform = ObjectSaveContext.GetTargetPlatform();
		if (TargetPlatform && TargetPlatform->RequiresCookedData())
		{
			EMovieSceneServerClientMask NetworkMask = EMovieSceneServerClientMask::All;
			if (TargetPlatform->IsClientOnly())
			{
				NetworkMask = EMovieSceneServerClientMask::Client;
			}
			else if (!TargetPlatform->AllowAudioVisualData())
			{
				NetworkMask = EMovieSceneServerClientMask::Server;
			}
			NetworkMask = OverrideNetworkMask(NetworkMask);

			if (ObjectSaveContext.IsCooking())
			{
				OptimizeForCook();
			}
	
			UMovieSceneCompiledDataManager::GetPrecompiledData(NetworkMask)->CopyCompiledData(this);
		}
		else if (CompiledData)
		{
			// Don't save template data unless we're cooking
			CompiledData->Reset();
		}
	}
#endif
	Super::PreSave(ObjectSaveContext);
}

void UMovieSceneSequence::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FMovieSceneEvaluationCustomVersion::GUID);
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);

	Super::Serialize(Ar);
}

#if WITH_EDITOR

bool UMovieSceneSequence::OptimizeForCook()
{
	UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene)
	{
		return false;
	}

	bool bModified = false;

	for (int32 TrackIndex = 0; TrackIndex < MovieScene->GetTracks().Num(); )
	{
		UMovieSceneTrack* Track = MovieScene->GetTracks()[TrackIndex];
		if (Track && Track->GetCookOptimizationFlags() == ECookOptimizationFlags::RemoveTrack)
		{				
			Track->RemoveForCook();
			MovieScene->RemoveTrack(*Track);
			UE_LOG(LogMovieScene, Display, TEXT("Removing muted track: %s from: %s"), *Track->GetDisplayName().ToString(), *GetPathName());
			bModified = true;
			continue;
		}
		++TrackIndex;
	}

	// Go through the tracks again and look at sections
	for (int32 TrackIndex = 0; TrackIndex < MovieScene->GetTracks().Num(); ++TrackIndex)
	{
		UMovieSceneTrack* Track =  MovieScene->GetTracks()[TrackIndex];
		if (Track)
		{
			for (int32 SectionIndex = 0; SectionIndex < Track->GetAllSections().Num(); )
			{
				UMovieSceneSection* Section = Track->GetAllSections()[SectionIndex];
				if (Section && Section->GetCookOptimizationFlags() == ECookOptimizationFlags::RemoveSection)
				{
					Section->RemoveForCook();
					Track->RemoveSection(*Section);
					UE_LOG(LogMovieScene, Display, TEXT("Removing muted section: %s from: %s"), *Section->GetPathName(), *Track->GetDisplayName().ToString());
					bModified = true;
					continue;
				}
				++SectionIndex;
			}
		}
	}

	for (int32 ObjectBindingIndex = 0; ObjectBindingIndex < MovieScene->GetBindings().Num(); )
	{
		bool bRemoveObject = false;

		// First, look to remove the object
		for (int32 TrackIndex = 0; TrackIndex < MovieScene->GetBindings()[ObjectBindingIndex].GetTracks().Num(); ++TrackIndex)
		{
			UMovieSceneTrack* Track = MovieScene->GetBindings()[ObjectBindingIndex].GetTracks()[TrackIndex];
			if (Track && Track->GetCookOptimizationFlags() == ECookOptimizationFlags::RemoveObject)
			{
				bRemoveObject = true;
				break;
			}
		}

		// Look to remove tracks
		for (int32 TrackIndex = 0; TrackIndex < MovieScene->GetBindings()[ObjectBindingIndex].GetTracks().Num(); )
		{
			UMovieSceneTrack* Track = MovieScene->GetBindings()[ObjectBindingIndex].GetTracks()[TrackIndex];
			if (Track && (Track->GetCookOptimizationFlags() == ECookOptimizationFlags::RemoveTrack || bRemoveObject))
			{
				Track->RemoveForCook();
				MovieScene->RemoveTrack(*Track);
				UE_LOG(LogMovieScene, Display, TEXT("Removing muted track: %s from: %s"), *Track->GetDisplayName().ToString(), *GetPathName());
				bModified = true;
				continue;
			}
			++TrackIndex;
		}

		// Go through the tracks again and look at sections
		for (int32 TrackIndex = 0; TrackIndex < MovieScene->GetBindings()[ObjectBindingIndex].GetTracks().Num(); ++TrackIndex)
		{
			UMovieSceneTrack* Track = MovieScene->GetBindings()[ObjectBindingIndex].GetTracks()[TrackIndex];
			if (Track)
			{
				for (int32 SectionIndex = 0; SectionIndex < Track->GetAllSections().Num(); )
				{
					UMovieSceneSection* Section = Track->GetAllSections()[SectionIndex];
					if (Section && (Section->GetCookOptimizationFlags() == ECookOptimizationFlags::RemoveSection || bRemoveObject))
					{
						Section->RemoveForCook();
						Track->RemoveSection(*Section);
						UE_LOG(LogMovieScene, Display, TEXT("Removing muted section: %s from: %s"), *Section->GetPathName(), *Track->GetDisplayName().ToString());
						bModified = true;
						continue;
					}
					++SectionIndex;
				}
			}
		}
		
		if (bRemoveObject)
		{
			UE_LOG(LogMovieScene, Display, TEXT("Removing muted object: %s from: %s"), *MovieScene->GetBindings()[ObjectBindingIndex].GetName(), *GetPathName());
			FGuid GuidToRemove = MovieScene->GetBindings()[ObjectBindingIndex].GetObjectGuid();
			bModified |= MovieScene->RemoveSpawnable(GuidToRemove);
			bModified |= MovieScene->RemovePossessable(GuidToRemove);
		}
		else
		{
			++ObjectBindingIndex;
		}
	}

	if (bModified)
	{
		Modify();
		MovieScene->Modify();
	}

	return bModified;
}

#endif

UMovieSceneCompiledData* UMovieSceneSequence::GetCompiledData() const
{
	return CompiledData;
}

UMovieSceneCompiledData* UMovieSceneSequence::GetOrCreateCompiledData()
{
	if (!CompiledData)
	{
		CompiledData = FindObject<UMovieSceneCompiledData>(this, TEXT("CompiledData"));
		if (CompiledData)
		{
			CompiledData->Reset();
		}
		else
		{
			CompiledData = NewObject<UMovieSceneCompiledData>(this, "CompiledData");
		}
	}
	return CompiledData;
}

FGuid UMovieSceneSequence::FindPossessableObjectId(UObject& Object, UObject* Context) const
{
	using namespace UE::MovieScene;

	FSharedPlaybackStateCreateParams CreateParams;
	CreateParams.PlaybackContext = Context;
	UMovieSceneSequence* ThisSequence = const_cast<UMovieSceneSequence*>(this);
	TSharedRef<FSharedPlaybackState> TransientPlaybackState = MakeShared<FSharedPlaybackState>(*ThisSequence, CreateParams);

	FMovieSceneEvaluationState State;
	TransientPlaybackState->AddCapabilityRaw(&State);
	State.AssignSequence(MovieSceneSequenceID::Root, *ThisSequence, TransientPlaybackState);

	FGuid ExistingID = State.FindObjectId(Object, MovieSceneSequenceID::Root, TransientPlaybackState);
	return ExistingID;
}

FMovieSceneObjectBindingID UMovieSceneSequence::FindBindingByTag(FName InBindingName) const
{
	for (FMovieSceneObjectBindingID ID : FindBindingsByTag(InBindingName))
	{
		return ID;
	}

	FMessageLog("PIE")
		.Warning(NSLOCTEXT("UMovieSceneSequence", "FindNamedBinding_Warning", "Attempted to find a named binding that did not exist"))
		->AddToken(FUObjectToken::Create(this));

	return FMovieSceneObjectBindingID();
}

const TArray<FMovieSceneObjectBindingID>& UMovieSceneSequence::FindBindingsByTag(FName InBindingName) const
{
	const FMovieSceneObjectBindingIDs* BindingIDs = GetMovieScene()->AllTaggedBindings().Find(InBindingName);
	if (BindingIDs)
	{
		return BindingIDs->IDs;
	}

	static TArray<FMovieSceneObjectBindingID> EmptyBindings;
	return EmptyBindings;
}

FMovieSceneTimecodeSource UMovieSceneSequence::GetEarliestTimecodeSource() const
{
	const UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene)
	{
		return FMovieSceneTimecodeSource();
	}

	return MovieScene->GetEarliestTimecodeSource();
}

UObject* UMovieSceneSequence::CreateDirectorInstance(IMovieScenePlayer& Player, FMovieSceneSequenceID SequenceID)
{
	return CreateDirectorInstance(Player.GetSharedPlaybackState(), SequenceID);
}

