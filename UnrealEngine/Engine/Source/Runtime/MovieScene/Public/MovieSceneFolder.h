// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "EventHandlers/IFolderEventHandler.h"
#include "EventHandlers/MovieSceneDataEventContainer.h"
#include "Math/Color.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneFolder.generated.h"

class FArchive;
class ITransactionObjectAnnotation;
class UMovieSceneTrack;
namespace UE { namespace MovieScene { class IFolderEventHandler; } }

/** Represents a folder used for organizing objects in tracks in a movie scene. */
UCLASS(DefaultToInstanced)
class MOVIESCENE_API UMovieSceneFolder : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Gets the name of this folder. */
	FName GetFolderName() const;

	/** Sets the name of this folder. Automatically calls Modify() on the folder object. */
	void SetFolderName( FName InFolderName );

	/** Gets the folders contained by this folder. */
	TArrayView<UMovieSceneFolder* const> GetChildFolders() const;

	/** Adds a child folder to this folder. Automatically calls Modify() on the folder object. */
	void AddChildFolder( UMovieSceneFolder* InChildFolder );

	/** Removes a child folder from this folder. Automatically calls Modify() on the folder object. */
	void RemoveChildFolder( UMovieSceneFolder* InChildFolder );

	/** Gets the master tracks contained by this folder. */
	const TArray<UMovieSceneTrack*>& GetChildMasterTracks() const;

	/** Adds a master track to this folder. Automatically calls Modify() on the folder object. */
	void AddChildMasterTrack( UMovieSceneTrack* InMasterTrack );

	/** Removes a master track from this folder. Automatically calls Modify() on the folder object. */
	void RemoveChildMasterTrack( UMovieSceneTrack* InMasterTrack );

	/** Clear all child master tracks from this folder. */
	void ClearChildMasterTracks();

	/** Gets the guids for the object bindings contained by this folder. */
	const TArray<FGuid>& GetChildObjectBindings() const;

	/** Adds a guid for an object binding to this folder. Automatically calls Modify() on the folder object. */
	void AddChildObjectBinding(const FGuid& InObjectBinding );

	/** Removes a guid for an object binding from this folder. Automatically calls Modify() on the folder object. */
	void RemoveChildObjectBinding( const FGuid& InObjectBinding );

	/** Clear all child object bindings from this folder. */
	void ClearChildObjectBindings();

	/** Called after this object has been deserialized */
	virtual void PostLoad() override;

	/** Searches for a guid in this folder and its child folders, if found returns the folder containing the guid. */
	UMovieSceneFolder* FindFolderContaining(const FGuid& InObjectBinding);

	/** Searches for a track in this folder and its child folders, if found returns the folder containing the track. */
	UMovieSceneFolder* FindFolderContaining(const UMovieSceneTrack* InTrack);

	/** Get the folder path for this folder, stopping at the given root folders */
	static void CalculateFolderPath(UMovieSceneFolder* InFolder, TArrayView<UMovieSceneFolder* const> RootFolders, TArray<FName>& FolderPath);
	
	/** For the given set of folders, return the folder that has the matching folder path */
	static UMovieSceneFolder* GetFolderWithPath(const TArray<FName>& InFolderPath, const TArray<UMovieSceneFolder*>& InFolders, TArrayView<UMovieSceneFolder* const> RootFolders);

	virtual void Serialize( FArchive& Archive );

	FName MakeUniqueChildFolderName(FName InName) const;

	static FName MakeUniqueChildFolderName(FName InName, TArrayView<UMovieSceneFolder* const> InFolders);

#if WITH_EDITORONLY_DATA
	/**
	 * Get this folder's color.
	 *
	 * @return The folder color.
	 */
	const FColor& GetFolderColor() const
	{
		return FolderColor;
	}

	/**
	 * Set this folder's color. Does not call Modify() on the folder object for legacy reasons.
	 *
	 * @param InFolderColor The folder color to set.
	 */
	void SetFolderColor(const FColor& InFolderColor)
	{
		FolderColor = InFolderColor;
	}

	/**
	 * Get this folder's desired sorting order 
	 */
	int32 GetSortingOrder() const
	{
		return SortingOrder;
	}

	/**
	 * Set this folder's desired sorting order. Does not call Modify() internally for legacy reasons.
	 *
	 * @param InSortingOrder The higher the value the further down the list the folder will be.
	 */
	void SetSortingOrder(const int32 InSortingOrder)
	{
		SortingOrder = InSortingOrder;
	}
#endif

#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation) override;
#endif

	/** Event handlers for handling changes to this object */
	UE::MovieScene::TDataEventContainer<UE::MovieScene::IFolderEventHandler> EventHandlers;

private:
	/** The name of this folder. */
	UPROPERTY()
	FName FolderName;

	/** The folders contained by this folder. */
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneFolder>> ChildFolders;

	/** The master tracks contained by this folder. */
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneTrack>> ChildMasterTracks;

	/** The guid strings used to serialize the guids for the object bindings contained by this folder. */
	UPROPERTY()
	TArray<FString> ChildObjectBindingStrings;

#if WITH_EDITORONLY_DATA
	/** This folder's color */
	UPROPERTY(EditAnywhere, Category=General, DisplayName=Color)
	FColor FolderColor;

	/** This folder's desired sorting order */
	UPROPERTY()
	int32 SortingOrder;
#endif

	/** The guids for the object bindings contained by this folder. */
	TArray<FGuid> ChildObjectBindings;
};

MOVIESCENE_API void GetMovieSceneFoldersRecursive(TArrayView<UMovieSceneFolder* const> InFoldersToRecurse, TArray<UMovieSceneFolder*>& OutFolders);


