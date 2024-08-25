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
UCLASS(MinimalAPI)
class UWorldFolders : public UObject
{
public:
	GENERATED_BODY()

	UNREALED_API void Initialize(UWorld* InWorld);

	UNREALED_API void RebuildList();
	UNREALED_API bool AddFolder(const FFolder& InFolder);
	UNREALED_API bool RemoveFolder(const FFolder& InFolder, bool bShouldDeleteFolder = false);
	UNREALED_API bool RenameFolder(const FFolder& InOldFolder, const FFolder& InNewFolder);
	UNREALED_API bool IsFolderExpanded(const FFolder& InFolder) const;
	UNREALED_API bool SetIsFolderExpanded(const FFolder& InFolder, bool bIsExpanded);
	UNREALED_API FFolder GetActorEditorContextFolder(bool bMustMatchCurrentLevel) const;
	UNREALED_API bool SetActorEditorContextFolder(const FFolder& InFolder);
	UNREALED_API void PushActorEditorContext(bool bDuplicateContext = false);
	UNREALED_API void PopActorEditorContext();
	UNREALED_API bool ContainsFolder(const FFolder& InFolder) const;
	UNREALED_API void ForEachFolder(TFunctionRef<bool(const FFolder&)> Operation);
	UNREALED_API void ForEachFolderWithRootObject(const FFolder::FRootObject& InFolderRootObject, TFunctionRef<bool(const FFolder&)> Operation);
	UNREALED_API void SaveState();
	UNREALED_API UWorld* GetWorld() const;

	//~ Begin UObject
	UNREALED_API virtual void Serialize(FArchive& Ar) override;
	//~ End UObject

	//~ Begin Deprecated
	UNREALED_API FActorFolderProps* GetFolderProperties(const FFolder& InFolder);
	//~ End Deprecated

private:

	UNREALED_API void BroadcastOnActorFolderCreated(const FFolder& InFolder);
	UNREALED_API void BroadcastOnActorFolderDeleted(const FFolder& InFolder);
	UNREALED_API void BroadcastOnActorFolderMoved(const FFolder& InSrcFolder, const FFolder& InDstFolder);

	UNREALED_API FWorldFoldersImplementation& GetImpl(const FFolder& InFolder) const;
	UNREALED_API bool IsUsingPersistentFolders(const FFolder& InFolder) const;

	UNREALED_API FString GetWorldStateFilename() const;
	UNREALED_API void LoadState();

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
