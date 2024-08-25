// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Framework/SlateDelegates.h"
#include "Misc/FrameTime.h"
#include "MovieSceneBindingProxy.h"
#include "SequencerUtilities.generated.h"

class ACineCameraActor;
template< typename ObjectType > class TAttribute;
class UActorFactory;
class UMovieSceneFolder;
class UMovieSceneTrack;
class UMovieSceneSection;
class UMovieSceneSequence;
class ISequencer;
class FMenuBuilder;
struct FMovieSceneBinding;
struct FMovieScenePossessable;
struct FMovieSceneSpawnable;
struct FNotificationInfo;
class ULevelSequence;
enum class EMovieSceneBlendType : uint8;

namespace UE::Sequencer
{

struct FCreateBindingParams
{
	UE_DEPRECATED(5.4, "Please use FSequencerUtilitiesCreateBindingParams directly.")
	FCreateBindingParams(const FString& InBindingNameOverride)
		: BindingNameOverride(InBindingNameOverride)
	{}

	FCreateBindingParams()
	{}
	
	FCreateBindingParams& Name(FString&& InName)
	{
		BindingNameOverride = MoveTemp(InName);
		return *this;
	}
	FCreateBindingParams& Folder(const FName& InFolder)
	{
		DesiredFolder = InFolder;
		return *this;
	}

	FString BindingNameOverride;
	FName DesiredFolder;
};

} // namespace UE::Sequencer

/* Paste folders params */
USTRUCT(BlueprintType)
struct FMovieScenePasteFoldersParams
{
	GENERATED_BODY()

	FMovieScenePasteFoldersParams() {}
	FMovieScenePasteFoldersParams(UMovieSceneSequence* InSequence, UMovieSceneFolder* InParentFolder = nullptr)
		: Sequence(InSequence)
		, ParentFolder(InParentFolder) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	TObjectPtr<UMovieSceneSequence> Sequence;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	TObjectPtr<UMovieSceneFolder> ParentFolder;
};

/* Paste sections params */
USTRUCT(BlueprintType)
struct FMovieScenePasteSectionsParams
{
	GENERATED_BODY()

	FMovieScenePasteSectionsParams() {}
	FMovieScenePasteSectionsParams(const TArray<UMovieSceneTrack*>& InTracks, const TArray<int32>& InTrackRowIndices, FFrameTime InTime)
		: Tracks(InTracks)
		, TrackRowIndices(InTrackRowIndices)
		, Time(InTime) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	TArray<TObjectPtr<UMovieSceneTrack>> Tracks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	TArray<int32> TrackRowIndices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	FFrameTime Time;
};

/* Paste tracks params */
USTRUCT(BlueprintType)
struct FMovieScenePasteTracksParams
{
	GENERATED_BODY()

	FMovieScenePasteTracksParams() {}
	FMovieScenePasteTracksParams(UMovieSceneSequence* InSequence, const TArray<FMovieSceneBindingProxy>& InBindings = TArray<FMovieSceneBindingProxy>(), UMovieSceneFolder* InParentFolder = nullptr, const TArray<UMovieSceneFolder*>& InFolders = TArray<UMovieSceneFolder*>())
		: Sequence(InSequence)
		, Bindings(InBindings)
		, ParentFolder(InParentFolder)
		, Folders(InFolders) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	TObjectPtr<UMovieSceneSequence> Sequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	TArray<FMovieSceneBindingProxy> Bindings;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	TObjectPtr<UMovieSceneFolder> ParentFolder;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	TArray<TObjectPtr<UMovieSceneFolder>> Folders;
};

/* Paste bindings params */
USTRUCT(BlueprintType)
struct FMovieScenePasteBindingsParams
{
	GENERATED_BODY()

	FMovieScenePasteBindingsParams(const TArray<FMovieSceneBindingProxy>& InBindings = TArray<FMovieSceneBindingProxy>(), UMovieSceneFolder* InParentFolder = nullptr, const TArray<UMovieSceneFolder*>& InFolders = TArray<UMovieSceneFolder*>(), bool bInDuplicateExistingActors = false)
		: Bindings(InBindings)
		, ParentFolder(InParentFolder)
		, Folders(InFolders)
		, bDuplicateExistingActors(bInDuplicateExistingActors) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	TArray<FMovieSceneBindingProxy> Bindings;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	TObjectPtr<UMovieSceneFolder> ParentFolder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	TArray<TObjectPtr<UMovieSceneFolder>> Folders;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Scene")
	bool bDuplicateExistingActors;
};

struct SEQUENCER_API FSequencerUtilities
{
	/* Creates a button (used for +Section) that opens a ComboButton with a user-defined sub-menu content. */
	static TSharedRef<SWidget> MakeAddButton(FText HoverText, FOnGetContent MenuContent, const TAttribute<bool>& HoverState, TWeakPtr<ISequencer> InSequencer);
	
	/* Creates a button (used for +Section) that fires a user-defined OnClick response with no sub-menu. */
	static TSharedRef<SWidget> MakeAddButton(FText HoverText, FOnClicked OnClicked, const TAttribute<bool>& HoverState, TWeakPtr<ISequencer> InSequencer);

	static void CreateNewSection(UMovieSceneTrack* InTrack, TWeakPtr<ISequencer> InSequencer, int32 InRowIndex, EMovieSceneBlendType InBlendType);

	static void PopulateMenu_CreateNewSection(FMenuBuilder& MenuBuilder, int32 RowIndex, UMovieSceneTrack* Track, TWeakPtr<ISequencer> InSequencer);

	static void PopulateMenu_BlenderSubMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track, TWeakPtr<ISequencer> InSequencer);

	static void PopulateMenu_SetBlendType(FMenuBuilder& MenuBuilder, UMovieSceneSection* Section, TWeakPtr<ISequencer> InSequencer);

	static void PopulateMenu_SetBlendType(FMenuBuilder& MenuBuilder, const TArray<TWeakObjectPtr<UMovieSceneSection>>& InSections, TWeakPtr<ISequencer> InSequencer);

	static TArray<FString> GetAssociatedLevelSequenceMapPackages(const ULevelSequence* InSequence);
	static TArray<FString> GetAssociatedLevelSequenceMapPackages(FName LevelSequencePackageName);

	/** 
	 * Generates a unique FName from a candidate name given a set of already existing names.  
	 * The name is made unique by appending a number to the end.
	 */
	static FName GetUniqueName(FName CandidateName, const TArray<FName>& ExistingNames);

	/** Add existing actors to Sequencer */
	static TArray<FGuid> AddActors(TSharedRef<ISequencer> Sequencer, const TArray<TWeakObjectPtr<AActor> >& InActors);

	/** Create a new camera actor and add it to Sequencer */
	static FGuid CreateCamera(TSharedRef<ISequencer> Sequencer, const bool bSpawnable, ACineCameraActor*& OutActor);

	/** Create a new camera from a rig and add it to Sequencer */
	static FGuid CreateCameraWithRig(TSharedRef<ISequencer> Sequencer, AActor* Actor, const bool bSpawnable, ACineCameraActor*& OutActor);

	static FGuid MakeNewSpawnable(TSharedRef<ISequencer> Sequencer, UObject& SourceObject, UActorFactory* ActorFactory = nullptr, bool bSetupDefaults = true, FName SpawnableName = NAME_None);

	/** Convert the requested possessable to spawnable. If there are multiple objects assigned to the possessable, multiple spawnables will be created */
	static TArray<FMovieSceneSpawnable*> ConvertToSpawnable(TSharedRef<ISequencer> Sequencer, FGuid PossessableGuid);
	
	/** Convert the requested spawnable to possessable */
	static FMovieScenePossessable* ConvertToPossessable(TSharedRef<ISequencer> Sequencer, FGuid SpawnableGuid);

	/** Copy/paste folders */
	static void CopyFolders(const TArray<UMovieSceneFolder*>& Folders, FString& ExportedText);
	static bool PasteFolders(const FString& TextToImport, FMovieScenePasteFoldersParams PasteFoldersParams, TArray<UMovieSceneFolder*>& OutFolders, TArray<FNotificationInfo>& OutErrors);
	static bool CanPasteFolders(const FString& TextToImport);

	/** Copy/paste tracks */
	static void CopyTracks(const TArray<UMovieSceneTrack*>& Tracks, const TArray<UMovieSceneFolder*>& InFolders, FString& ExportedText);
	static bool PasteTracks(const FString& TextToImport, FMovieScenePasteTracksParams PasteTracksParams, TArray<UMovieSceneTrack*>& OutTracks, TArray<FNotificationInfo>& OutErrors);
	static bool CanPasteTracks(const FString& TextToImport);

	/** Copy/paste sections */
	static void CopySections(const TArray<UMovieSceneSection*>& Sections, FString& ExportedText);
	static bool PasteSections(const FString& TextToImport, FMovieScenePasteSectionsParams PasteSectionsParams, TArray<UMovieSceneSection*>& OutSections, TArray<FNotificationInfo>& OutErrors);
	static bool CanPasteSections(const FString& TextToImport);

	/** Copy/paste object bindings */
	static void CopyBindings(TSharedRef<ISequencer> Sequencer, const TArray<FMovieSceneBindingProxy>& Bindings, const TArray<UMovieSceneFolder*>& InFolders, FString& ExportedText);
	static bool PasteBindings(const FString& TextToImport, TSharedRef<ISequencer> Sequencer, FMovieScenePasteBindingsParams PasteBindingsParams, TArray<FMovieSceneBindingProxy>& OutBindings, TArray<FNotificationInfo>& OutErrors);
	static bool CanPasteBindings(TSharedRef<ISequencer> Sequencer, const FString& TextToImport);
	static TArray<FString> GetPasteBindingsObjectNames(TSharedRef<ISequencer> Sequencer, const FString& TextToImport);

	/** Utility functions for managing bindings */
	static FGuid CreateBinding(TSharedRef<ISequencer> Sequencer, UObject& InObject, const UE::Sequencer::FCreateBindingParams& Params = UE::Sequencer::FCreateBindingParams());
	static void UpdateBindingIDs(TSharedRef<ISequencer> Sequencer, FGuid OldGuid, FGuid NewGuid);
	static FGuid AssignActor(TSharedRef<ISequencer> Sequencer, AActor* Actor, FGuid InObjectBinding);
	static void AddActorsToBinding(TSharedRef<ISequencer> Sequencer, const TArray<AActor*>& Actors, const FMovieSceneBindingProxy& ObjectBinding);
	static void ReplaceBindingWithActors(TSharedRef<ISequencer> Sequencer, const TArray<AActor*>& Actors, const FMovieSceneBindingProxy& ObjectBinding);
	static void RemoveActorsFromBinding(TSharedRef<ISequencer> Sequencer, const TArray<AActor*>& Actors, const FMovieSceneBindingProxy& ObjectBinding);

	/** Show a read only error if the movie scene is locked */
	static void ShowReadOnlyError();
	
	/** Show an error if spawnable is not allowed in a movie scene*/
	static void ShowSpawnableNotAllowedError();

	// Methods exposing FSequencer functionality outside of the Sequencer module.
	// This is needed while things are moving into view-models here and there, step by step.
	static void SaveCurrentMovieSceneAs(TSharedRef<ISequencer> Sequencer);
	static void SynchronizeExternalSelectionWithSequencerSelection (TSharedRef<ISequencer> Sequencer);
	static TRange<FFrameNumber> GetTimeBounds(TSharedRef<ISequencer> Sequencer);
};
