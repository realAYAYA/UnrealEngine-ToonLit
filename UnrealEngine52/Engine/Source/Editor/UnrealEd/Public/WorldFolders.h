// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "Misc/Guid.h"
#include "WorldFoldersImplementation.h"
#include "WorldPersistentFolders.h"
#include "WorldTransientFolders.h"
#include "WorldFolders.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldFolders, Log, All)

USTRUCT()
struct FActorFolderProps
{
	GENERATED_USTRUCT_BODY()

	FActorFolderProps() : bIsExpanded(true) {}

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FActorFolderProps& Folder)
	{
		return Ar << Folder.bIsExpanded;
	}

	bool bIsExpanded;
};

USTRUCT()
struct FActorPlacementFolder
{
	GENERATED_BODY()

	UPROPERTY()
	FName Path;

	UPROPERTY()
	TWeakObjectPtr<UObject> RootObjectPtr;
	
	UPROPERTY()
	FGuid ActorFolderGuid;

	void Reset()
	{
		Path = NAME_None;
		RootObjectPtr.Reset();
		ActorFolderGuid.Invalidate();
	}

	FActorPlacementFolder& operator = (const FFolder& InOtherFolder)
	{
		Path = InOtherFolder.GetPath();
		RootObjectPtr = InOtherFolder.GetRootObjectPtr();
		ActorFolderGuid = InOtherFolder.GetActorFolderGuid();
		return *this;
	}

	FFolder GetFolder() const
	{
		FFolder::FRootObject RootObject = FFolder::FRootObject(RootObjectPtr.Get());
		return ActorFolderGuid.IsValid() ? FFolder(RootObject, ActorFolderGuid) : FFolder(RootObject, Path);
	}
};

/** Per-World Actor Folders UObject (used to support undo/redo reliably) */
UCLASS()
class UNREALED_API UWorldFolders : public UObject
{
public:
	GENERATED_BODY()

	void Initialize(UWorld* InWorld);

	void RebuildList();
	bool AddFolder(const FFolder& InFolder);
	bool RemoveFolder(const FFolder& InFolder, bool bShouldDeleteFolder = false);
	bool RenameFolder(const FFolder& InOldFolder, const FFolder& InNewFolder);
	bool IsFolderExpanded(const FFolder& InFolder) const;
	bool SetIsFolderExpanded(const FFolder& InFolder, bool bIsExpanded);
	FFolder GetActorEditorContextFolder(bool bMustMatchCurrentLevel) const;
	bool SetActorEditorContextFolder(const FFolder& InFolder);
	void PushActorEditorContext();
	void PopActorEditorContext();
	bool ContainsFolder(const FFolder& InFolder) const;
	void ForEachFolder(TFunctionRef<bool(const FFolder&)> Operation);
	void ForEachFolderWithRootObject(const FFolder::FRootObject& InFolderRootObject, TFunctionRef<bool(const FFolder&)> Operation);
	void SaveState();
	UWorld* GetWorld() const;

	//~ Begin UObject
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject

	//~ Begin Deprecated
	FActorFolderProps* GetFolderProperties(const FFolder& InFolder);
	//~ End Deprecated

private:

	void BroadcastOnActorFolderCreated(const FFolder& InFolder);
	void BroadcastOnActorFolderDeleted(const FFolder& InFolder);
	void BroadcastOnActorFolderMoved(const FFolder& InSrcFolder, const FFolder& InDstFolder);

	FWorldFoldersImplementation& GetImpl(const FFolder& InFolder) const;
	bool IsUsingPersistentFolders(const FFolder& InFolder) const;

	FString GetWorldStateFilename() const;
	void LoadState();

	TUniquePtr<FWorldPersistentFolders> PersistentFolders;
	TUniquePtr<FWorldTransientFolders> TransientFolders;

	TWeakObjectPtr<UWorld> World;
	TMap<FFolder, FActorFolderProps> FoldersProperties;
	TMap<FFolder, FActorFolderProps> LoadedStateFoldersProperties;

	UPROPERTY()
	FActorPlacementFolder CurrentFolder;

	TArray<FActorPlacementFolder> CurrentFolderStack;

	friend class FWorldFoldersImplementation;
	friend class FWorldPersistentFolders;
	friend class FWorldTransientFolders;
};