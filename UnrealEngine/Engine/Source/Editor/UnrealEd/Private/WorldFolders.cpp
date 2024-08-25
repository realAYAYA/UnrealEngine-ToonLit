// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldFolders.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"
#include "EditorActorFolders.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "UnrealEd.WorldFolders"

DEFINE_LOG_CATEGORY(LogWorldFolders);

void UWorldFolders::Initialize(UWorld* InWorld)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldFolders::Initialize);

	check(!World.IsValid());
	check(IsValidChecked(InWorld));
	
	World = InWorld;
	SetFlags(RF_Transactional);
	
	PersistentFolders = MakeUnique<FWorldPersistentFolders>(*this);
	TransientFolders = MakeUnique<FWorldTransientFolders>(*this);

	RebuildList();

	LoadState();
}

void UWorldFolders::RebuildList()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldFolders::RebuildList);

	if (GetWorld()->IsGameWorld())
	{
		return;
	}

	Modify();
	
	// Clear folders with a Root Object.
	TArray<FFolder> FoldersToRemove;
	ForEachFolder([&FoldersToRemove](const FFolder& Folder)
	{
		if (!Folder.IsRootObjectPersistentLevel())
		{
			FoldersToRemove.Add(Folder);
		}
		return true;
	});

	for (const FFolder& Folder : FoldersToRemove)
	{
		RemoveFolder(Folder);
	}

	// Iterate over every actor in memory. WARNING: This is potentially very expensive!
	for (FActorIterator ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AddFolder(ActorIt->GetFolder());
	}

	// Add levels ActorFolders as they are still needed for unloaded actors (only in editor)
	if (!GetWorld()->IsGameWorld())
	{
		for (ULevel* Level : GetWorld()->GetLevels())
		{
			const bool bIsLevelVisibleOrAssociating = (Level->bIsVisible && !Level->bIsBeingRemoved) || Level->bIsAssociatingLevel || Level->bIsDisassociatingLevel;
			if (bIsLevelVisibleOrAssociating)
			{
				Level->ForEachActorFolder([this](UActorFolder* ActorFolder)
				{
					AddFolder(ActorFolder->GetFolder());
					return true;
				}, /*bSkipDeleted*/ true);
			}
		}
	}
}

UWorld* UWorldFolders::GetWorld() const
{
	return World.Get();
}

bool UWorldFolders::AddFolder(const FFolder& InFolder)
{
	if (!InFolder.IsNone())
	{
		if (!FoldersProperties.Contains(InFolder))
		{
			// Add the parent as well
			const FFolder ParentFolder = InFolder.GetParent();
			if (!ParentFolder.IsNone())
			{
				AddFolder(ParentFolder);
			}

			Modify();
			FActorFolderProps* LoadedFolderProps = LoadedStateFoldersProperties.Find(InFolder);
			FoldersProperties.Add(InFolder, LoadedFolderProps ? *LoadedFolderProps : FActorFolderProps());

			return GetImpl(InFolder).AddFolder(InFolder);
		}
	}

	return false;
}

bool UWorldFolders::RemoveFolder(const FFolder& InFolder, bool bShouldDeleteFolder)
{
	if (FoldersProperties.Contains(InFolder))
	{
		Modify();
		FoldersProperties.Remove(InFolder);

		return GetImpl(InFolder).RemoveFolder(InFolder, bShouldDeleteFolder);
	}
	return false;
}

bool UWorldFolders::RenameFolder(const FFolder& InOldFolder, const FFolder& InNewFolder)
{
	Modify();

	check(IsValid(World.Get()));
	check(InOldFolder.GetRootObject() == InNewFolder.GetRootObject());

	bool bSuccess = GetImpl(InOldFolder).RenameFolder(InOldFolder, InNewFolder);
	if (bSuccess)
	{
		bool bChanged = false;
		for (int StackIndex=0; StackIndex < CurrentFolderStack.Num(); ++StackIndex)
		{
			FActorPlacementFolder& StackElement = CurrentFolderStack[StackIndex];
			if (StackElement.GetFolder() == InOldFolder)
			{
				StackElement.Path = InNewFolder.GetPath();
				bChanged = true;
			}
		}
		if (CurrentFolder.GetFolder() == InOldFolder)
		{
			CurrentFolder.Path = InNewFolder.GetPath();
			bChanged = true;
		}
		if (bChanged)
		{
			FActorFolders::Get().BroadcastOnActorEditorContextClientChanged(*World.Get());
		}
	}
	return bSuccess;
}

void UWorldFolders::BroadcastOnActorFolderCreated(const FFolder& InFolder)
{
	check(World.IsValid());
	FActorFolders::Get().BroadcastOnActorFolderCreated(*World, InFolder);
}

void UWorldFolders::BroadcastOnActorFolderDeleted(const FFolder& InFolder)
{
	check(World.IsValid());
	FActorFolders::Get().BroadcastOnActorFolderDeleted(*World, InFolder);
}

void UWorldFolders::BroadcastOnActorFolderMoved(const FFolder& InSrcFolder, const FFolder& InDstFolder)
{
	check(World.IsValid());
	FActorFolders::Get().BroadcastOnActorFolderMoved(*World, InSrcFolder, InDstFolder);
}

bool UWorldFolders::IsFolderExpanded(const FFolder& InFolder) const
{
	const FActorFolderProps* FolderProps = FoldersProperties.Find(InFolder);
	return FolderProps ? FolderProps->bIsExpanded : false;
}

bool UWorldFolders::SetIsFolderExpanded(const FFolder& InFolder, bool bIsExpanded)
{
	if (FActorFolderProps* FolderProps = FoldersProperties.Find(InFolder))
	{
		FolderProps->bIsExpanded = bIsExpanded;
		return true;
	}
	return false;
}

FFolder UWorldFolders::GetActorEditorContextFolder(bool bMustMatchCurrentLevel) const
{
	FFolder Folder = CurrentFolder.GetFolder();
	if (ContainsFolder(Folder))
	{
		ULevelInstanceSubsystem* LevelInstanceSubsystem = GetWorld()->GetSubsystem<ULevelInstanceSubsystem>();
		ILevelInstanceInterface* EditingLevelInstance = LevelInstanceSubsystem ? LevelInstanceSubsystem->GetEditingLevelInstance() : nullptr;
		if (UObject* EditingLevelInstanceObject = EditingLevelInstance ? CastChecked<UObject>(EditingLevelInstance) : nullptr)
		{
			FFolder::FRootObject RootObject = FFolder::FRootObject(EditingLevelInstanceObject);
			if (Folder.GetRootObject() == RootObject)
			{
				return Folder;
			}
		}
		else if (!bMustMatchCurrentLevel || (Folder.GetRootObject() == FFolder::FRootObject(GetWorld()->GetCurrentLevel())))
		{
			return Folder;
		}
	}
	return FFolder::GetWorldRootFolder(GetWorld());
}

bool UWorldFolders::SetActorEditorContextFolder(const FFolder& InFolder)
{
	if (InFolder != CurrentFolder.GetFolder())
	{
		Modify();
		CurrentFolder = InFolder;
		return true;
	}
	return false;
}

void UWorldFolders::PushActorEditorContext(bool bDuplicateContext)
{
	Modify();
	CurrentFolderStack.Push(CurrentFolder);
	if (!bDuplicateContext)
	{
		CurrentFolder.Reset();
	}
}

void UWorldFolders::PopActorEditorContext()
{
	check(!CurrentFolderStack.IsEmpty());

	Modify();
	CurrentFolder = CurrentFolderStack.Pop();
}

bool UWorldFolders::ContainsFolder(const FFolder& InFolder) const
{
	return InFolder.IsValid() && !InFolder.IsNone() && GetImpl(InFolder).ContainsFolder(InFolder);
}

void UWorldFolders::ForEachFolder(TFunctionRef<bool(const FFolder&)> Operation)
{
	for (const auto& Pair : FoldersProperties)
	{
		if (!Operation(Pair.Key))
		{
			break;
		}
	}
}

void UWorldFolders::ForEachFolderWithRootObject(const FFolder::FRootObject& InFolderRootObject, TFunctionRef<bool(const FFolder&)> Operation)
{
	for (const auto& Pair : FoldersProperties)
	{
		const FFolder& Folder = Pair.Key;
		if (Folder.GetRootObject() == InFolderRootObject)
		{
			if (!Operation(Folder))
			{
				break;
			}
		}
	}
}

void UWorldFolders::Serialize(FArchive& Ar)
{
	if (IsTemplate())
	{
		return;
	}

	check(PersistentFolders.IsValid());
	Ar << FoldersProperties;

	Super::Serialize(Ar);
}

FString UWorldFolders::GetWorldStateFilename() const
{
	if (World->IsGameWorld() || World->IsInstanced() || FPackageName::IsTempPackage(World->GetPackage()->GetName()))
	{
		return FString();
	}
	UPackage* Package = World->GetOutermost();
	const FString PathName = Package->GetPathName();
	const uint32 PathNameCrc = FCrc::MemCrc32(*PathName, sizeof(TCHAR) * PathName.Len());
	return FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("Config"), TEXT("WorldState"), *FString::Printf(TEXT("%u.json"), PathNameCrc));
}

void UWorldFolders::LoadState()
{
	const FString Filename = GetWorldStateFilename();
	if (Filename.IsEmpty())
	{
		return;
	}

	// Attempt to load the folder properties from user's saved world state directory and apply them.
	TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(*Filename));
	if (Ar)
	{
		TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
		auto Reader = TJsonReaderFactory<TCHAR>::Create(Ar.Get());
		if (FJsonSerializer::Deserialize(Reader, RootObject))
		{
			FFolder WorldDefaultFolder = FFolder::GetWorldRootFolder(World.Get());
			check(WorldDefaultFolder.IsRootObjectValid());
			const FFolder::FRootObject WorldRootObject = WorldDefaultFolder.GetRootObject();

			const TSharedPtr<FJsonObject>& JsonFolders = RootObject->GetObjectField(TEXT("Folders"));
			for (const auto& KeyValue : JsonFolders->Values)
			{
				// Only pull in the folder's properties if this folder still exists in the world.
				// This means that old stale folders won't re-appear in the world (they'll won't get serialized when the world is saved anyway)
				auto FolderProperties = KeyValue.Value->AsObject();
				const FFolder Folder(WorldRootObject, *KeyValue.Key);
				const bool bIsExpanded = FolderProperties->GetBoolField(TEXT("bIsExpanded"));
				if (!SetIsFolderExpanded(Folder, bIsExpanded))
				{
					FActorFolderProps& LoadedFolderProps = LoadedStateFoldersProperties.FindOrAdd(Folder);
					LoadedFolderProps.bIsExpanded = bIsExpanded;
				}
			}
		}
		Ar->Close();
	}
}

void UWorldFolders::SaveState()
{
	const FString Filename = GetWorldStateFilename();
	if (Filename.IsEmpty())
	{
		return;
	}

	TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*Filename));
	if (Ar)
	{
		TSharedRef<FJsonObject> RootObject = MakeShareable(new FJsonObject);
		TSharedRef<FJsonObject> JsonFolders = MakeShareable(new FJsonObject);

		ForEachFolder([this, &JsonFolders](const FFolder& Folder)
		{
			// Only write for World root
			if (Folder.IsRootObjectPersistentLevel())
			{
				TSharedRef<FJsonObject> JsonFolder = MakeShareable(new FJsonObject);
				JsonFolder->SetBoolField(TEXT("bIsExpanded"), IsFolderExpanded(Folder));
				JsonFolders->SetObjectField(Folder.ToString(), JsonFolder);
			}
			return true;
		});

		RootObject->SetObjectField(TEXT("Folders"), JsonFolders);
		{
			auto Writer = TJsonWriterFactory<TCHAR>::Create(Ar.Get());
			FJsonSerializer::Serialize(RootObject, Writer);
			Ar->Close();
		}
	}
}

bool UWorldFolders::IsUsingPersistentFolders(const FFolder& InFolder) const
{
	ULevel* Level = FWorldPersistentFolders::GetRootObjectContainer(InFolder, GetWorld());
	return Level ? Level->IsUsingActorFolders() : false;
}

FWorldFoldersImplementation& UWorldFolders::GetImpl(const FFolder& InFolder) const
{
	if (IsUsingPersistentFolders(InFolder))
	{
		return *PersistentFolders;
	}
	else
	{
		return *TransientFolders;
	}
}

////////////////////////////////////////////
//~ Begin Deprecated
FActorFolderProps* UWorldFolders::GetFolderProperties(const FFolder& InFolder)
{
	return FoldersProperties.Find(InFolder);
}
//~ End Deprecated
////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE 