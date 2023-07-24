// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/GCObject.h"
#include "Folder.h"
#include "WorldFolders.h"
#include "IActorEditorContextClient.h"

class FObjectPostSaveContext;
class AActor;
class UActorFolder;
class FWorldPartitionActorDesc;

/** Multicast delegates for broadcasting various folder events */

//~ Begin Deprecated
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnActorFolderCreate, UWorld&, FName);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnActorFolderDelete, UWorld&, FName);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnActorFolderMove, UWorld&, FName /* src */, FName /* dst */);
//~ End Deprecated

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnActorFolderCreated, UWorld&, const FFolder&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnActorFolderDeleted, UWorld&, const FFolder&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnActorFolderMoved, UWorld&, const FFolder& /* src */, const FFolder& /* dst */);


/** Class responsible for managing an in-memory representation of actor folders in the editor */
struct UNREALED_API FActorFolders : public FGCObject, public IActorEditorContextClient
{
	FActorFolders();
	~FActorFolders();

	// FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return "FActorFolders";
	}
	// End FGCObject Interface

	/** Check whether the singleton is valid */
	static bool IsAvailable() { return Singleton != nullptr; }

	/** Singleton access - only valid if IsAvailable() */
	static FActorFolders& Get();

	/** Initialize the singleton instance - called on Editor Startup */
	static void Init();

	/** Clean up the singleton instance - called on Editor Exit */
	static void Cleanup();

	/** Folder creation and deletion events. Called whenever a folder is created or deleted in a world. */
	static FOnActorFolderCreated OnFolderCreated;
	static FOnActorFolderMoved 	OnFolderMoved;
	static FOnActorFolderDeleted OnFolderDeleted;

	//~ Begin Deprecated

	UE_DEPRECATED(5.0, "OnFolderCreate has been deprecated. Please use OnFolderCreated.")
	static FOnActorFolderCreate OnFolderCreate;

	UE_DEPRECATED(5.0, "OnFolderMove has been deprecated. Please use OnFolderMoved.")
	static FOnActorFolderMove 	OnFolderMove;

	UE_DEPRECATED(5.0, "OnFolderDelete has been deprecated. Please use OnFolderDeleted.")
	static FOnActorFolderDelete OnFolderDelete;

	UE_DEPRECATED(5.0, "GetFolderProperties using FName has been deprecated. Please use new interface using FFolder.")
	FActorFolderProps* GetFolderProperties(UWorld& InWorld, FName InPath);

	UE_DEPRECATED(5.0, "GetDefaultFolderName using FName  has been deprecated. Please use new interface using FFolder.")
	FName GetDefaultFolderName(UWorld& InWorld, FName ParentPath = FName());
	
	UE_DEPRECATED(5.0, "GetDefaultFolderNameForSelection using FName  has been deprecated. Please use new interface using FFolder.")
	FName GetDefaultFolderNameForSelection(UWorld& InWorld);

	UE_DEPRECATED(5.0, "GetFolderName using FName  has been deprecated. Please use new interface using FFolder.")
	FName GetFolderName(UWorld& InWorld, FName ParentPath, FName FolderName);

	UE_DEPRECATED(5.0, "CreateFolder using FName  has been deprecated. Please use new interface using FFolder.")
	void CreateFolder(UWorld& InWorld, FName Path);

	UE_DEPRECATED(5.0, "CreateFolderContainingSelection using FName  has been deprecated. Please use new interface using FFolder.")
	void CreateFolderContainingSelection(UWorld& InWorld, FName Path);

	UE_DEPRECATED(5.0, "SetSelectedFolderPath using FName  has been deprecated. Please use new interface using FFolder.")
	void SetSelectedFolderPath(FName Path) const;

	UE_DEPRECATED(5.0, "DeleteFolder using FName  has been deprecated. Please use new interface using FFolder.")
	void DeleteFolder(UWorld& InWorld, FName FolderToDelete);

	UE_DEPRECATED(5.0, "RenameFolderInWorld using FName  has been deprecated. Please use new interface using FFolder.")
	bool RenameFolderInWorld(UWorld& World, FName OldPath, FName NewPath);

	//~ End Deprecated
	static FFolder GetActorDescFolder(UWorld& InWorld, const FWorldPartitionActorDesc* InActorDesc);

	/** Apply an operation to each actor desc in the given list of folders. */
	static void ForEachActorDescInFolders(UWorld& InWorld, const TSet<FName>& InPaths, TFunctionRef<bool(const FWorldPartitionActorDesc*)> Operation, const FFolder::FRootObject& InFolderRootObject = FFolder::GetInvalidRootObject());
		
	/** Apply an operation to each actor in the given list of folders. Will stop when operation returns false. */
	static void ForEachActorInFolders(UWorld& InWorld, const TArray<FName>& InPaths, TFunctionRef<bool(AActor*)> Operation, const FFolder::FRootObject& InFolderRootObject = FFolder::GetInvalidRootObject());
	static void ForEachActorInFolders(UWorld& InWorld, const TSet<FName>& InPaths, TFunctionRef<bool(AActor*)> Operation, const FFolder::FRootObject& InFolderRootObject = FFolder::GetInvalidRootObject());

	/** Get an array of actors from a list of folders */
	static void GetActorsFromFolders(UWorld& InWorld, const TArray<FName>& InPaths, TArray<AActor*>& OutActors, const FFolder::FRootObject& InFolderRootObject = FFolder::GetInvalidRootObject());
	static void GetActorsFromFolders(UWorld& InWorld, const TSet<FName>& InPaths, TArray<AActor*>& OutActors, const FFolder::FRootObject& InFolderRootObject = FFolder::GetInvalidRootObject());

	/** Get an array of weak actor pointers from a list of folders */
	static void GetWeakActorsFromFolders(UWorld& InWorld, const TArray<FName>& InPaths, TArray<TWeakObjectPtr<AActor>>& OutActors, const FFolder::FRootObject& InFolderRootObject = FFolder::GetInvalidRootObject());
	static void GetWeakActorsFromFolders(UWorld& InWorld, const TSet<FName>& InPaths, TArray<TWeakObjectPtr<AActor>>& OutActors, const FFolder::FRootObject& InFolderRootObject = FFolder::GetInvalidRootObject());

	/** Tests whether a folder container exists for the specified world */
	bool IsInitializedForWorld(UWorld& InWorld) const;

	/** Get a default folder name under the specified parent path */
	FFolder GetDefaultFolderName(UWorld& InWorld, const FFolder& InParentFolder);
	
	/** Get a new default folder name that would apply to the current selection */
	FFolder GetDefaultFolderForSelection(UWorld& InWorld, TArray<FFolder>* InSelectedFolders = nullptr);

	/** Get folder name that is unique under specified parent path */
	FFolder GetFolderName(UWorld& InWorld, const FFolder& InParentFolder, const FName& InLeafName);

	/** Create a new folder in the specified world, of the specified path */
	bool CreateFolder(UWorld& InWorld, const FFolder& InFolder);

	/** Same as CreateFolder, but moves the current actor selection into the new folder as well */
	void CreateFolderContainingSelection(UWorld& InWorld, const FFolder& InFolder);

	/** Sets the folder path for all the selected actors */
	void SetSelectedFolderPath(const FFolder& InFolder) const;

	/** Delete the specified folder in the world */
	void DeleteFolder(UWorld& InWorld, const FFolder& InFolderToDelete);

	/** Rename the specified path to a new name */
	bool RenameFolderInWorld(UWorld& InWorld, const FFolder& OldPath, const FFolder& NewPath);

	/** Notify that a root object has been removed. Cleanup of existing folders with with root object. */
	void OnFolderRootObjectRemoved(UWorld& InWorld, const FFolder::FRootObject& InFolderRootObject);

	/** Return if folder exists */
	bool ContainsFolder(UWorld& InWorld, const FFolder& InFolder);

	/** Return the folder expansion state */
	bool IsFolderExpanded(UWorld& InWorld, const FFolder& InFolder);

	/** Set the folder expansion state */
	void SetIsFolderExpanded(UWorld& InWorld, const FFolder& InFolder, bool bIsExpanded);

	/** Iterate on all folders of a world and pass it to the provided operation. */
	void ForEachFolder(UWorld& InWorld, TFunctionRef<bool(const FFolder&)> Operation);

	/** Iterate on all world's folders with the given root object and pass it to the provided operation. */
	void ForEachFolderWithRootObject(UWorld& InWorld, const FFolder::FRootObject& InFolderRootObject, TFunctionRef<bool(const FFolder&)> Operation);

	/** Get the folder properties for the specified path. Returns nullptr if no properties exist */
	FActorFolderProps* GetFolderProperties(UWorld& InWorld, const FFolder& InFolder);

	//~ Begin IActorEditorContextClient interface
	virtual void OnExecuteActorEditorContextAction(UWorld* InWorld, const EActorEditorContextAction& InType, class AActor* InActor = nullptr) override;
	virtual bool GetActorEditorContextDisplayInfo(UWorld* InWorld, FActorEditorContextClientDisplayInfo& OutDiplayInfo) const override;
	virtual bool CanResetContext(UWorld* InWorld) const override { return true; };
	virtual TSharedRef<SWidget> GetActorEditorContextWidget(UWorld* InWorld) const override;
	virtual FOnActorEditorContextClientChanged& GetOnActorEditorContextClientChanged() override { return ActorEditorContextClientChanged; }
	//~ End IActorEditorContextClient interface
	FFolder GetActorEditorContextFolder(UWorld& InWorld, bool bMustMatchCurrentLevel = true) const;
	void SetActorEditorContextFolder(UWorld& InWorld, const FFolder& InFolder);

private:
	
	static FFolder::FRootObject GetWorldFolderRootObject(UWorld& InWorld);

	/** Broadcast when actor folder is created. */
	void BroadcastOnActorFolderCreated(UWorld& InWorld, const FFolder& InFolder);

	/** Broadcast when actor folder is deleted. */
	void BroadcastOnActorFolderDeleted(UWorld& InWorld, const FFolder& InFolder);

	/** Broadcast when actor folder has moved. */
	void BroadcastOnActorFolderMoved(UWorld& InWorld, const FFolder& InSrcFolder, const FFolder& InDstFolder);

	/** Get or create a folder container for the specified world */
	UWorldFolders& GetOrCreateWorldFolders(UWorld& InWorld);

	/** Create and update a folder container for the specified world */
	UWorldFolders& CreateWorldFolders(UWorld& InWorld);

	/** Rebuild the folder list for the specified world. This can be very slow as it
		iterates all actors in memory to rebuild the array of actors for this world */
	void RebuildFolderListForWorld(UWorld& InWorld);

	/** Called when an actor's folder has changed */
	void OnActorFolderChanged(const AActor* InActor, FName OldPath);

	/** Called when the actor list of the current world has changed */
	void OnLevelActorListChanged();

	/** Called when an actor folder is added */
	void OnActorFolderAdded(UActorFolder* InActorFolder);

	/** Called when the global map in the editor has changed */
	void OnMapChange(uint32 MapChangeFlags);

	/** Called after a world has been saved */
	void OnWorldSaved(UWorld* World, FObjectPostSaveContext ObjectSaveContext);

	/** Attempt to save the folders state */
	void SaveWorldFoldersState(UWorld* World);

	/** Remove any references to folder arrays for dead worlds */
	void Housekeeping();

	/** Add a folder to the folder map for the specified world. Does not trigger any events. */
	bool AddFolderToWorld(UWorld& InWorld, const FFolder& InFolder);

	/** Removed folders from specified world. Can optionally trigger delete events. */
	void RemoveFoldersFromWorld(UWorld& InWorld, const TArray<FFolder>& InFolders, bool bBroadcastDelete);

	/** Transient map of folders, keyed on world pointer */
	TMap<TWeakObjectPtr<UWorld>, UWorldFolders*> WorldFolders;

	/** Called when ActorEditorContextClient changed. */
	void BroadcastOnActorEditorContextClientChanged();

	/** Delegate used to notify changes to ActorEditorContextSubsystem */
	FOnActorEditorContextClientChanged ActorEditorContextClientChanged;

	/** Singleton instance maintained by the editor */
	static FActorFolders* Singleton;

	friend UWorldFolders;
};
