// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceLevelStreaming.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "LevelInstancePrivate.h"
#include "LevelUtils.h"
#include "Hash/CityHash.h"

#if WITH_EDITOR
#include "Settings/LevelEditorMiscSettings.h"
#include "LevelInstance/LevelInstanceEditorLevelStreaming.h"
#include "LevelInstance/ILevelInstanceEditorModule.h"
#include "LevelInstance/LevelInstanceEditorInstanceActor.h"
#include "LevelInstance/LevelInstanceEditorObject.h"
#include "LevelInstance/LevelInstanceEditorPivotActor.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/WorldPartitionMiniMap.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ITransaction.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "EditorLevelUtils.h"
#include "HAL/PlatformTime.h"
#include "Engine/Selection.h"
#include "Engine/LevelBounds.h"
#include "Modules/ModuleManager.h"
#include "Engine/Blueprint.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "PackedLevelActor/PackedLevelActorBuilder.h"
#include "Engine/LevelScriptBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/GCObject.h"
#include "EditorActorFolders.h"
#include "Misc/MessageDialog.h"
#include "Subsystems/ActorEditorContextSubsystem.h"
#endif

#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelInstanceSubsystem)

#define LOCTEXT_NAMESPACE "LevelInstanceSubsystem"

DEFINE_LOG_CATEGORY(LogLevelInstance);

ULevelInstanceSubsystem::ULevelInstanceSubsystem()
	: UWorldSubsystem()
#if WITH_EDITOR
	, bIsCreatingLevelInstance(false)
	, bIsCommittingLevelInstance(false)
#endif
{}

ULevelInstanceSubsystem::~ULevelInstanceSubsystem()
{}

void ULevelInstanceSubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	ULevelInstanceSubsystem* This = CastChecked<ULevelInstanceSubsystem>(InThis);

#if WITH_EDITORONLY_DATA
	if (This->LevelInstanceEdit)
	{
		This->LevelInstanceEdit->AddReferencedObjects(Collector);
	}

	This->ActorDescContainerInstanceManager.AddReferencedObjects(Collector);
#endif
}

void ULevelInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR
	if (GEditor)
	{
		ILevelInstanceEditorModule& EditorModule = FModuleManager::LoadModuleChecked<ILevelInstanceEditorModule>("LevelInstanceEditor");
		FEditorDelegates::OnAssetsPreDelete.AddUObject(this, &ULevelInstanceSubsystem::OnAssetsPreDelete);
	}
#endif
}

void ULevelInstanceSubsystem::Deinitialize()
{
#if WITH_EDITOR
	FEditorDelegates::OnAssetsPreDelete.RemoveAll(this);
#endif
}

bool ULevelInstanceSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return Super::DoesSupportWorldType(WorldType) || WorldType == EWorldType::EditorPreview || WorldType == EWorldType::Inactive;
}

ILevelInstanceInterface* ULevelInstanceSubsystem::GetLevelInstance(const FLevelInstanceID& LevelInstanceID) const
{
	if (ILevelInstanceInterface*const* LevelInstance = RegisteredLevelInstances.Find(LevelInstanceID))
	{
		return *LevelInstance;
	}

	return nullptr;
}

FLevelInstanceID::FLevelInstanceID(ULevelInstanceSubsystem* LevelInstanceSubsystem, ILevelInstanceInterface* LevelInstance)
{
	AActor* LevelInstanceActor = CastChecked<AActor>(LevelInstance);
	LevelInstanceSubsystem->ForEachLevelInstanceAncestorsAndSelf(LevelInstanceActor, [this](const ILevelInstanceInterface* AncestorOrSelf)
	{
		Guids.Add(AncestorOrSelf->GetLevelInstanceGuid());
		return true;
	});
	check(!Guids.IsEmpty());
	
	uint64 NameHash = 0;
	ActorName = LevelInstanceActor->GetFName();
	// Add Actor Name to hash because with World Partition Embedding top level of Level Instance hierarchy can get stripped leaving us with clashing ids.
	// When embedding actors we make sure their names are unique (they get suffixed with their parent container id)
	// Only do it for actor with a stable name. Actor with an unstable name are dynamically spawned and should have an unique replicated GUID.
	if (LevelInstanceActor->IsNameStableForNetworking())
	{
		FString NameStr = ActorName.ToString();
		NameHash = CityHash64((const char*)*NameStr, NameStr.Len() * sizeof(TCHAR));
	}
	
	Hash = CityHash64WithSeed((const char*)Guids.GetData(), Guids.Num() * sizeof(FGuid), NameHash);
}

FLevelInstanceID ULevelInstanceSubsystem::RegisterLevelInstance(ILevelInstanceInterface* LevelInstance)
{
	FLevelInstanceID LevelInstanceID(this, LevelInstance);
	check(LevelInstanceID.IsValid());
	ILevelInstanceInterface*& Value = RegisteredLevelInstances.FindOrAdd(LevelInstanceID);
	check(GIsReinstancing || Value == nullptr || Value == LevelInstance);
	Value = LevelInstance;

	return LevelInstanceID;
}

void ULevelInstanceSubsystem::UnregisterLevelInstance(ILevelInstanceInterface* LevelInstance)
{
	RegisteredLevelInstances.Remove(LevelInstance->GetLevelInstanceID());
}

void ULevelInstanceSubsystem::RequestLoadLevelInstance(ILevelInstanceInterface* LevelInstance, bool bForce /* = false */)
{
	check(LevelInstance && IsValidChecked(CastChecked<AActor>(LevelInstance)) && !CastChecked<AActor>(LevelInstance)->IsUnreachable());
	if (LevelInstance->IsWorldAssetValid())
	{
#if WITH_EDITOR
		if (!IsEditingLevelInstance(LevelInstance))
#endif
		{
			LevelInstancesToUnload.Remove(LevelInstance->GetLevelInstanceID());

			bool* bForcePtr = LevelInstancesToLoadOrUpdate.Find(LevelInstance);

			// Avoid loading if already loaded. Can happen if actor requests unload/load in same frame. Without the force it means its not necessary.
			if (IsLoaded(LevelInstance) && !bForce && (bForcePtr == nullptr || !(*bForcePtr)))
			{
				return;
			}

			if (bForcePtr != nullptr)
			{
				*bForcePtr |= bForce;
			}
			else
			{
				LevelInstancesToLoadOrUpdate.Add(LevelInstance, bForce);
			}
		}
	}
}

void ULevelInstanceSubsystem::RequestUnloadLevelInstance(ILevelInstanceInterface* LevelInstance)
{
	const FLevelInstanceID& LevelInstanceID = LevelInstance->GetLevelInstanceID();
	// Test whether level instance is loaded or is still loading
	if (LoadedLevelInstances.Contains(LevelInstanceID) || LoadingLevelInstances.Contains(LevelInstanceID))
	{
		// LevelInstancesToUnload uses FLevelInstanceID because LevelInstance* can be destroyed in later Tick and we don't need it.
		LevelInstancesToUnload.Add(LevelInstanceID);
	}
	LevelInstancesToLoadOrUpdate.Remove(LevelInstance);
}

bool ULevelInstanceSubsystem::IsLoaded(const ILevelInstanceInterface* LevelInstance) const
{
	return LevelInstance->HasValidLevelInstanceID() && LoadedLevelInstances.Contains(LevelInstance->GetLevelInstanceID());
}

void ULevelInstanceSubsystem::UpdateStreamingState()
{
	if (!LevelInstancesToUnload.Num() && !LevelInstancesToLoadOrUpdate.Num())
	{
		return;
	}

#if WITH_EDITOR
	// Do not update during transaction
	if (GUndo)
	{
		return;
	}

	FScopedSlowTask SlowTask(LevelInstancesToUnload.Num() + LevelInstancesToLoadOrUpdate.Num() * 2, LOCTEXT("UpdatingLevelInstances", "Updating Level Instances..."), !GetWorld()->IsGameWorld());
	SlowTask.MakeDialogDelayed(1.0f);

	check(!LevelsToRemoveScope);
	LevelsToRemoveScope.Reset(new FLevelsToRemoveScope(this));
#endif

	if (LevelInstancesToUnload.Num())
	{
		TSet<FLevelInstanceID> LevelInstancesToUnloadCopy(MoveTemp(LevelInstancesToUnload));
		for (const FLevelInstanceID& LevelInstanceID : LevelInstancesToUnloadCopy)
		{
#if WITH_EDITOR
			SlowTask.EnterProgressFrame(1.f, LOCTEXT("UnloadingLevelInstance", "Unloading Level Instance"));
#endif
			if (LoadingLevelInstances.Contains(LevelInstanceID))
			{
				LevelInstancesToUnload.Add(LevelInstanceID);
			}
			else
			{
				UnloadLevelInstance(LevelInstanceID);
			}
		}
	}

	if (LevelInstancesToLoadOrUpdate.Num())
	{
		// Unload levels before doing any loading
		TMap<ILevelInstanceInterface*, bool> LevelInstancesToLoadOrUpdateCopy(MoveTemp(LevelInstancesToLoadOrUpdate));
		for (const TPair<ILevelInstanceInterface*, bool>& Pair : LevelInstancesToLoadOrUpdateCopy)
		{
#if WITH_EDITOR
			SlowTask.EnterProgressFrame(1.f, LOCTEXT("UnloadingLevelInstance", "Unloading Level Instance"));
#endif
			ILevelInstanceInterface* LevelInstance = Pair.Key;
			if (Pair.Value)
			{
				UnloadLevelInstance(LevelInstance->GetLevelInstanceID());
			}
		}

#if WITH_EDITOR
		LevelsToRemoveScope.Reset();
		double StartTime = FPlatformTime::Seconds();
#endif
		for (const TPair<ILevelInstanceInterface*, bool>& Pair : LevelInstancesToLoadOrUpdateCopy)
		{
#if WITH_EDITOR
			SlowTask.EnterProgressFrame(1.f, FText::Format(LOCTEXT("LoadingLevelInstance", "Loading Level Instance {0}"), FText::FromString(Pair.Key->GetWorldAsset().ToString())));
#endif
			LoadLevelInstance(Pair.Key);
		}
#if WITH_EDITOR
		double ElapsedTime = FPlatformTime::Seconds() - StartTime;
		UE_LOG(LogLevelInstance, Log, TEXT("Loaded %s levels in %s seconds"), *FText::AsNumber(LevelInstancesToLoadOrUpdateCopy.Num()).ToString(), *FText::AsNumber(ElapsedTime).ToString());
#endif
	}

#if WITH_EDITOR
	LevelsToRemoveScope.Reset();
#endif
}

void ULevelInstanceSubsystem::RegisterLoadedLevelStreamingLevelInstance(ULevelStreamingLevelInstance* LevelStreaming)
{
	const FLevelInstanceID LevelInstanceID = LevelStreaming->GetLevelInstanceID();
	check(LoadingLevelInstances.Contains(LevelInstanceID));
	LoadingLevelInstances.Remove(LevelInstanceID);
	check(!LoadedLevelInstances.Contains(LevelInstanceID));
	FLevelInstance& LevelInstanceEntry = LoadedLevelInstances.Add(LevelInstanceID);
	LevelInstanceEntry.LevelStreaming = LevelStreaming;

	// LevelInstanceID might not be registered anymore in the case where the level instance 
	// was unloaded while still being load.
	if (ILevelInstanceInterface* LevelInstance = LevelStreaming->GetLevelInstance())
	{
		check(LevelInstance->GetLevelInstanceID() == LevelInstanceID);
		LevelInstance->OnLevelInstanceLoaded();
	}
	else
	{
		// Validate that the LevelInstanceID is requested to be unloaded
		check(LevelInstancesToUnload.Contains(LevelInstanceID));
	}
}

#if WITH_EDITOR

void ULevelInstanceSubsystem::OnAssetsPreDelete(const TArray<UObject*>& Objects)
{
	for (UObject* Object : Objects)
	{
		if (IsValid(Object))
		{
			if (UPackage* Package = Object->GetPackage())
			{
				TArray<ILevelInstanceInterface*> LevelInstances = GetLevelInstances(Package->GetLoadedPath().GetPackageName());
				for (ILevelInstanceInterface* LevelInstancePtr : LevelInstances)
				{
					UnloadLevelInstance(LevelInstancePtr->GetLevelInstanceID());
				}
			}
		}
	}
}

void ULevelInstanceSubsystem::RegisterLoadedLevelStreamingLevelInstanceEditor(ULevelStreamingLevelInstanceEditor* LevelStreaming)
{
	if (!bIsCreatingLevelInstance)
	{
		check(!LevelInstanceEdit.IsValid());
		ILevelInstanceInterface* LevelInstance = LevelStreaming->GetLevelInstance();
		LevelInstanceEdit = MakeUnique<FLevelInstanceEdit>(LevelStreaming, LevelInstance->GetLevelInstanceID());

		if (ILevelInstanceEditorModule* EditorModule = FModuleManager::GetModulePtr<ILevelInstanceEditorModule>("LevelInstanceEditor"))
		{
			EditorModule->OnExitEditorMode().AddUObject(this, &ULevelInstanceSubsystem::OnExitEditorMode);
			EditorModule->OnTryExitEditorMode().AddUObject(this, &ULevelInstanceSubsystem::OnTryExitEditorMode);
		}
	}
}

void ULevelInstanceSubsystem::ResetEdit(TUniquePtr<FLevelInstanceEdit>& InLevelInstanceEdit)
{
	if (InLevelInstanceEdit)
	{
		if (InLevelInstanceEdit == LevelInstanceEdit)
		{
			if (ILevelInstanceEditorModule* EditorModule = FModuleManager::GetModulePtr<ILevelInstanceEditorModule>("LevelInstanceEditor"))
			{
				EditorModule->OnExitEditorMode().RemoveAll(this);
				EditorModule->OnTryExitEditorMode().RemoveAll(this);
			}
		}
		else
		{
			// Only case supported where we are using a tmp FLevelInstanceEdit
			check(bIsCreatingLevelInstance);
		}

		InLevelInstanceEdit.Reset();
	}
}
#endif

void ULevelInstanceSubsystem::LoadLevelInstance(ILevelInstanceInterface* LevelInstance)
{
	check(LevelInstance);
	AActor* LevelInstanceActor = CastChecked<AActor>(LevelInstance);
	if (IsLoaded(LevelInstance) || !IsValidChecked(LevelInstanceActor) || LevelInstanceActor->IsUnreachable() || !LevelInstance->IsWorldAssetValid())
	{
		return;
	}

	const FLevelInstanceID& LevelInstanceID = LevelInstance->GetLevelInstanceID();
	check(!LoadedLevelInstances.Contains(LevelInstanceID));
	check(!LoadingLevelInstances.Contains(LevelInstanceID));
	LoadingLevelInstances.Add(LevelInstanceID);

	if (ULevelStreamingLevelInstance* LevelStreaming = ULevelStreamingLevelInstance::LoadInstance(LevelInstance))
	{
#if WITH_EDITOR
		check(LevelInstanceActor->GetWorld()->IsGameWorld() || LoadedLevelInstances.Contains(LevelInstanceID));
#endif
	}
	else
	{
		LoadingLevelInstances.Remove(LevelInstanceID);
	}
}

void ULevelInstanceSubsystem::UnloadLevelInstance(const FLevelInstanceID& LevelInstanceID)
{
	if (GetWorld()->IsGameWorld())
	{
		FLevelInstance LevelInstance;
		if (LoadedLevelInstances.RemoveAndCopyValue(LevelInstanceID, LevelInstance))
		{
			ULevelStreamingLevelInstance::UnloadInstance(LevelInstance.LevelStreaming);
		}
	}
#if WITH_EDITOR
	else
	{
		// Create scope if it doesn't exist
		bool bReleaseScope = false;
		if (!LevelsToRemoveScope)
		{
			bReleaseScope = true;
			LevelsToRemoveScope.Reset(new FLevelsToRemoveScope(this));
		}

		FLevelInstance LevelInstance;
		if (LoadedLevelInstances.RemoveAndCopyValue(LevelInstanceID, LevelInstance))
		{
			if (ULevel* LoadedLevel = LevelInstance.LevelStreaming->GetLoadedLevel())
			{
				ForEachActorInLevel(LoadedLevel, [this](AActor* LevelActor)
				{
					if (ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(LevelActor))
					{
						// Make sure to remove from pending loads if we are unloading child can't be loaded
						LevelInstancesToLoadOrUpdate.Remove(LevelInstance);

						UnloadLevelInstance(LevelInstance->GetLevelInstanceID());
					}
					return true;
				});
			}

			ULevelStreamingLevelInstance::UnloadInstance(LevelInstance.LevelStreaming);
		}

		if (bReleaseScope)
		{
			LevelsToRemoveScope.Reset();
		}
	}
#endif
}

void ULevelInstanceSubsystem::ForEachActorInLevel(ULevel* Level, TFunctionRef<bool(AActor * LevelActor)> Operation) const
{
	for (AActor* LevelActor : Level->Actors)
	{
		if (IsValid(LevelActor))
		{
			if (!Operation(LevelActor))
			{
				return;
			}
		}
	}
}

void ULevelInstanceSubsystem::ForEachLevelInstanceAncestorsAndSelf(AActor* Actor, TFunctionRef<bool(ILevelInstanceInterface*)> Operation) const
{
	if (ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(Actor))
	{
		if (!Operation(LevelInstance))
		{
			return;
		}
	}

	ForEachLevelInstanceAncestors(Actor, Operation);
}

ULevelStreamingLevelInstance* ULevelInstanceSubsystem::GetLevelInstanceLevelStreaming(const ILevelInstanceInterface* LevelInstance) const
{
	if (LevelInstance->HasValidLevelInstanceID())
	{
		if (const FLevelInstance* LevelInstanceEntry = LoadedLevelInstances.Find(LevelInstance->GetLevelInstanceID()))
		{
			return LevelInstanceEntry->LevelStreaming;
		}
	}

	return nullptr;
}

void ULevelInstanceSubsystem::ForEachLevelInstanceAncestors(AActor* Actor, TFunctionRef<bool(ILevelInstanceInterface*)> Operation) const
{
	ILevelInstanceInterface* ParentLevelInstance = nullptr;
	do
	{
		ParentLevelInstance = GetOwningLevelInstance(Actor->GetLevel());
		Actor = Cast<AActor>(ParentLevelInstance);

	} while (ParentLevelInstance != nullptr && Operation(ParentLevelInstance));
}

ILevelInstanceInterface* ULevelInstanceSubsystem::GetOwningLevelInstance(const ULevel* Level) const
{
	if (ULevelStreaming* BaseLevelStreaming = FLevelUtils::FindStreamingLevel(Level))
	{
#if WITH_EDITOR
		if (ULevelStreamingLevelInstanceEditor* LevelStreamingEditor = Cast<ULevelStreamingLevelInstanceEditor>(BaseLevelStreaming))
		{
			return LevelStreamingEditor->GetLevelInstance();
		}
		else 
#endif
		if (ULevelStreamingLevelInstance* LevelStreaming = Cast<ULevelStreamingLevelInstance>(BaseLevelStreaming))
		{
			return LevelStreaming->GetLevelInstance();
		}
		else if (UWorldPartitionLevelStreamingDynamic* WorldPartitionLevelStreaming = Cast<UWorldPartitionLevelStreamingDynamic>(BaseLevelStreaming))
		{
			return GetOwningLevelInstance(WorldPartitionLevelStreaming->GetOuterWorld()->PersistentLevel);
		}
	}

	return nullptr;
}

#if WITH_EDITOR

void ULevelInstanceSubsystem::Tick()
{
	if (GetWorld()->WorldType == EWorldType::Inactive)
	{
		return;
	}

	// For non-game world, Tick is responsible of processing LevelInstances to update/load/unload
	if (!GetWorld()->IsGameWorld())
	{
		UpdateStreamingState();
	}
}

void ULevelInstanceSubsystem::OnExitEditorMode()
{
	OnExitEditorModeInternal(/*bForceExit=*/true);
}

void ULevelInstanceSubsystem::OnTryExitEditorMode()
{	
	if (OnExitEditorModeInternal(/*bForceExit=*/false))
	{
		ILevelInstanceEditorModule& EditorModule = FModuleManager::GetModuleChecked<ILevelInstanceEditorModule>("LevelInstanceEditor");
		EditorModule.DeactivateEditorMode();
	}
}

bool ULevelInstanceSubsystem::OnExitEditorModeInternal(bool bForceExit)
{
	if (bIsCommittingLevelInstance || bIsCreatingLevelInstance)
	{
		return false;
	}

	if (LevelInstanceEdit)
	{
		TGuardValue<bool> CommitScope(bIsCommittingLevelInstance, true);
		ILevelInstanceInterface* LevelInstance = GetEditingLevelInstance();

		bool bDiscard = false;
		bool bIsDirty = IsLevelInstanceEditDirty(LevelInstanceEdit.Get());
		if (bIsDirty && CanCommitLevelInstance(LevelInstance, /*bDiscardEdits=*/true))
		{
			FText Title = LOCTEXT("CommitOrDiscardChangesTitle", "Save changes?");
			// if bForceExit we can't cancel the exiting of the mode so the user needs to decide between saving or discarding
			EAppReturnType::Type Ret = FMessageDialog::Open(bForceExit ? EAppMsgType::YesNo : EAppMsgType::YesNoCancel, LOCTEXT("CommitOrDiscardChangesMsg", "Unsaved Level changes will get discarded. Do you want to save them now?"), &Title);
			if (Ret == EAppReturnType::Cancel && !bForceExit)
			{
				return false;
			}

			bDiscard = (Ret != EAppReturnType::Yes);
		}

		return CommitLevelInstanceInternal(LevelInstanceEdit, bDiscard, /*bDiscardOnFailure=*/bForceExit);
	}

	return false;
}

bool ULevelInstanceSubsystem::CanPackAllLoadedActors() const
{
	return !LevelInstanceEdit;
}

void ULevelInstanceSubsystem::PackAllLoadedActors()
{
	if (!CanPackAllLoadedActors())
	{
		return;
	}

	// Add Dependencies first so that we pack in the proper order (depth first)
	TFunction<void(APackedLevelActor*, TArray<UBlueprint*>&, TArray<APackedLevelActor*>&)> GatherDepencenciesRecursive = [&GatherDepencenciesRecursive](APackedLevelActor* PackedLevelActor, TArray<UBlueprint*>& BPsToPack, TArray<APackedLevelActor*>& ToPack)
	{
		// Early out on already processed BPs or non BP Packed LIs.
		UBlueprint* Blueprint = Cast<UBlueprint>(PackedLevelActor->GetClass()->ClassGeneratedBy);
		if ((Blueprint && BPsToPack.Contains(Blueprint)) || ToPack.Contains(PackedLevelActor))
		{
			return;
		}
		
		// Recursive deps
		for (const TSoftObjectPtr<UBlueprint>& Dependency : PackedLevelActor->PackedBPDependencies)
		{
			if (UBlueprint* LoadedDependency = Dependency.LoadSynchronous())
			{
				if (APackedLevelActor* CDO = Cast<APackedLevelActor>(LoadedDependency->GeneratedClass ? LoadedDependency->GeneratedClass->GetDefaultObject() : nullptr))
				{
					GatherDepencenciesRecursive(CDO, BPsToPack, ToPack);
				}
			}
		}

		// Add after dependencies
		if (Blueprint)
		{
			BPsToPack.Add(Blueprint);
		}
		else
		{
			ToPack.Add(PackedLevelActor);
		}
	};

	TArray<APackedLevelActor*> PackedLevelActorsToUpdate;
	TArray<UBlueprint*> BlueprintsToUpdate;
	for (TObjectIterator<UWorld> It(RF_ClassDefaultObject | RF_ArchetypeObject, true); It; ++It)
	{
		UWorld* CurrentWorld = *It;
		if (IsValid(CurrentWorld) && CurrentWorld->GetSubsystem<ULevelInstanceSubsystem>() != nullptr)
		{
			for (TActorIterator<APackedLevelActor> PackedLevelActorIt(CurrentWorld); PackedLevelActorIt; ++PackedLevelActorIt)
			{
				GatherDepencenciesRecursive(*PackedLevelActorIt, BlueprintsToUpdate, PackedLevelActorsToUpdate);
			}
		}
	}

	int32 Count = BlueprintsToUpdate.Num() + PackedLevelActorsToUpdate.Num();
	if (!Count)
	{
		return;
	}
	
	GEditor->SelectNone(true, true);

	FScopedSlowTask SlowTask(Count, (LOCTEXT("TaskPackLevels", "Packing Levels")));
	SlowTask.MakeDialog();
		
	auto UpdateProgress = [&SlowTask]()
	{
		if (SlowTask.CompletedWork < SlowTask.TotalAmountOfWork)
		{
			SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("TaskPackLevelProgress", "Packing Level {0} of {1}"), FText::AsNumber(SlowTask.CompletedWork), FText::AsNumber(SlowTask.TotalAmountOfWork)));
		}
	};

	TSharedPtr<FPackedLevelActorBuilder> Builder = FPackedLevelActorBuilder::CreateDefaultBuilder();
	const bool bCheckoutAndSave = false;
	for (UBlueprint* Blueprint : BlueprintsToUpdate)
	{
		Builder->UpdateBlueprint(Blueprint, bCheckoutAndSave);
		UpdateProgress();
	}

	for (APackedLevelActor* PackedLevelActor : PackedLevelActorsToUpdate)
	{
		PackedLevelActor->UpdateLevelInstanceFromWorldAsset();
		UpdateProgress();
	}
}

bool ULevelInstanceSubsystem::GetLevelInstanceBounds(const ILevelInstanceInterface* LevelInstance, FBox& OutBounds) const
{
	if (IsLoaded(LevelInstance))
	{
		const FLevelInstance& LevelInstanceEntry = LoadedLevelInstances.FindChecked(LevelInstance->GetLevelInstanceID());
		OutBounds = LevelInstanceEntry.LevelStreaming->GetBounds();
		return true;
	}
	
	if (LevelInstance->HasValidLevelInstanceID()) // Check ID to make sure GetLevelInstancesBounds is called on a registered LevelInstance that can retrieve its Edit.
	{
		if (const FLevelInstanceEdit* CurrentEdit = GetLevelInstanceEdit(LevelInstance))
		{
			OutBounds = CurrentEdit->LevelStreaming->GetBounds();
			return true;
		}
	}
	
	if(LevelInstance->IsWorldAssetValid())
	{
		// @todo_ow: remove this temp fix once it is again safe to call the asset registry while saving.
		// Currently it can lead to a FindObject which is illegal while saving.
		if (UE::IsSavingPackage(nullptr))
		{
			OutBounds = FBox(ForceInit);
			return true;
		}

		FString LevelPackage = LevelInstance->GetWorldAssetPackage();

		if (FBox ContainerBounds = ActorDescContainerInstanceManager.GetContainerBounds(*LevelPackage); ContainerBounds.IsValid)
		{
			FTransform LevelInstancePivotOffsetTransform = FTransform(ULevel::GetLevelInstancePivotOffsetFromPackage(*LevelPackage));
			FTransform LevelTransform = LevelInstancePivotOffsetTransform * CastChecked<AActor>(LevelInstance)->GetActorTransform();
			OutBounds = ContainerBounds.TransformBy(LevelTransform);
			return true;
		}

		return GetLevelInstanceBoundsFromPackage(CastChecked<AActor>(LevelInstance)->GetActorTransform(), *LevelInstance->GetWorldAssetPackage(), OutBounds);
	}

	return false;
}

bool ULevelInstanceSubsystem::GetLevelInstanceBoundsFromPackage(const FTransform& InstanceTransform, FName LevelPackage, FBox& OutBounds)
{
	FBox LevelBounds;
	if (ULevel::GetLevelBoundsFromPackage(LevelPackage, LevelBounds))
	{
		FVector LevelBoundsLocation;
		FVector BoundsLocation;
		FVector BoundsExtent;
		LevelBounds.GetCenterAndExtents(BoundsLocation, BoundsExtent);

		//@todo_ow: This will result in a new BoundsExtent that is larger than it should. To fix this, we would need the Object Oriented BoundingBox of the actor (the BV of the actor without rotation)
		const FVector BoundsMin = BoundsLocation - BoundsExtent;
		const FVector BoundsMax = BoundsLocation + BoundsExtent;
		OutBounds = FBox(BoundsMin, BoundsMax).TransformBy(InstanceTransform);
		return true;
	}

	return false;
}

void ULevelInstanceSubsystem::ForEachActorInLevelInstance(const ILevelInstanceInterface* LevelInstance, TFunctionRef<bool(AActor * LevelActor)> Operation) const
{
	if (ULevel* LevelInstanceLevel = GetLevelInstanceLevel(LevelInstance))
	{
		ForEachActorInLevel(LevelInstanceLevel, Operation);
	}
}

void ULevelInstanceSubsystem::ForEachLevelInstanceAncestorsAndSelf(const AActor* Actor, TFunctionRef<bool(const ILevelInstanceInterface*)> Operation) const
{
	if (const ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(Actor))
	{
		if (!Operation(LevelInstance))
		{
			return;
		}
	}

	ForEachLevelInstanceAncestors(Actor, Operation);
}

void ULevelInstanceSubsystem::ForEachLevelInstanceAncestors(const AActor* Actor, TFunctionRef<bool(const ILevelInstanceInterface*)> Operation) const
{
	const ILevelInstanceInterface* ParentLevelInstance = nullptr;
	do 
	{
		ParentLevelInstance = GetOwningLevelInstance(Actor->GetLevel());
		Actor = Cast<AActor>(ParentLevelInstance);
	} 
	while (ParentLevelInstance != nullptr && Operation(ParentLevelInstance));
}

void ULevelInstanceSubsystem::ForEachLevelInstanceChild(const ILevelInstanceInterface* LevelInstance, bool bRecursive, TFunctionRef<bool(const ILevelInstanceInterface*)> Operation) const
{
	ForEachLevelInstanceChildImpl(LevelInstance, bRecursive, Operation);
}

bool ULevelInstanceSubsystem::ForEachLevelInstanceChildImpl(const ILevelInstanceInterface* LevelInstance, bool bRecursive, TFunctionRef<bool(const ILevelInstanceInterface*)> Operation) const
{
	bool bContinue = true;
	if (ULevel* LevelInstanceLevel = GetLevelInstanceLevel(LevelInstance))
	{
		ForEachActorInLevel(LevelInstanceLevel, [&bContinue, this, Operation,bRecursive](AActor* LevelActor)
		{
			if (const ILevelInstanceInterface* ChildLevelInstance = Cast<ILevelInstanceInterface>(LevelActor))
			{
				bContinue = Operation(ChildLevelInstance);
				
				if (bContinue && bRecursive)
				{
					bContinue = ForEachLevelInstanceChildImpl(ChildLevelInstance, bRecursive, Operation);
				}
			}
			return bContinue;
		});
	}

	return bContinue;
}

void ULevelInstanceSubsystem::ForEachLevelInstanceChild(ILevelInstanceInterface* LevelInstance, bool bRecursive, TFunctionRef<bool(ILevelInstanceInterface*)> Operation) const
{
	ForEachLevelInstanceChildImpl(LevelInstance, bRecursive, Operation);
}

bool ULevelInstanceSubsystem::ForEachLevelInstanceChildImpl(ILevelInstanceInterface* LevelInstance, bool bRecursive, TFunctionRef<bool(ILevelInstanceInterface*)> Operation) const
{
	bool bContinue = true;
	if (ULevel* LevelInstanceLevel = GetLevelInstanceLevel(LevelInstance))
	{
		ForEachActorInLevel(LevelInstanceLevel, [&bContinue, this, Operation, bRecursive](AActor* LevelActor)
		{
			if (ILevelInstanceInterface* ChildLevelInstance = Cast<ILevelInstanceInterface>(LevelActor))
			{
				bContinue = Operation(ChildLevelInstance);

				if (bContinue && bRecursive)
				{
					bContinue = ForEachLevelInstanceChildImpl(ChildLevelInstance, bRecursive, Operation);
				}
			}
			return bContinue;
		});
	}

	return bContinue;
}

bool ULevelInstanceSubsystem::HasDirtyChildrenLevelInstances(const ILevelInstanceInterface* LevelInstance) const
{
	bool bDirtyChildren = false;
	ForEachLevelInstanceChild(LevelInstance, /*bRecursive=*/true, [this, &bDirtyChildren](const ILevelInstanceInterface* ChildLevelInstance)
	{
		if (IsEditingLevelInstanceDirty(ChildLevelInstance))
		{
			bDirtyChildren = true;
			return false;
		}
		return true;
	});
	return bDirtyChildren;
}

void ULevelInstanceSubsystem::SetIsHiddenEdLayer(ILevelInstanceInterface* LevelInstance, bool bIsHiddenEdLayer)
{
	if (ULevel* LevelInstanceLevel = GetLevelInstanceLevel(LevelInstance))
	{
		ForEachActorInLevel(LevelInstanceLevel, [bIsHiddenEdLayer](AActor* LevelActor)
		{
			LevelActor->SetIsHiddenEdLayer(bIsHiddenEdLayer);
			return true;
		});
	}
}

void ULevelInstanceSubsystem::SetIsTemporarilyHiddenInEditor(ILevelInstanceInterface* LevelInstance, bool bIsHidden)
{
	if (ULevel* LevelInstanceLevel = GetLevelInstanceLevel(LevelInstance))
	{
		ForEachActorInLevel(LevelInstanceLevel, [bIsHidden](AActor* LevelActor)
		{
			LevelActor->SetIsTemporarilyHiddenInEditor(bIsHidden);
			return true;
		});
	}
}

bool ULevelInstanceSubsystem::SetCurrent(ILevelInstanceInterface* LevelInstance) const
{
	if (IsEditingLevelInstance(LevelInstance))
	{
		return GetWorld()->SetCurrentLevel(GetLevelInstanceLevel(LevelInstance));
	}

	return false;
}

bool ULevelInstanceSubsystem::IsCurrent(const ILevelInstanceInterface* LevelInstance) const
{
	if (IsEditingLevelInstance(LevelInstance))
	{
		return GetLevelInstanceLevel(LevelInstance) == GetWorld()->GetCurrentLevel();
	}

	return false;
}

bool ULevelInstanceSubsystem::MoveActorsToLevel(const TArray<AActor*>& ActorsToRemove, ULevel* DestinationLevel, TArray<AActor*>* OutActors /*= nullptr*/) const
{
	check(DestinationLevel);

	const bool bWarnAboutReferences = true;
	const bool bWarnAboutRenaming = true;
	const bool bMoveAllOrFail = true;
	if (!EditorLevelUtils::MoveActorsToLevel(ActorsToRemove, DestinationLevel, bWarnAboutReferences, bWarnAboutRenaming, bMoveAllOrFail, OutActors))
	{
		UE_LOG(LogLevelInstance, Warning, TEXT("Failed to move actors out of Level Instance because not all actors could be moved"));
		return false;
	}

	ILevelInstanceInterface* OwningInstance = GetOwningLevelInstance(DestinationLevel);
	if (!OwningInstance || !OwningInstance->IsEditing())
	{
		for (const auto& Actor : ActorsToRemove)
		{
			const bool bEditing = false;
			Actor->PushLevelInstanceEditingStateToProxies(bEditing);
		}
	}

	return true;
}

bool ULevelInstanceSubsystem::MoveActorsTo(ILevelInstanceInterface* LevelInstance, const TArray<AActor*>& ActorsToMove, TArray<AActor*>* OutActors /*= nullptr*/)
{
	check(IsEditingLevelInstance(LevelInstance));
	ULevel* LevelInstanceLevel = GetLevelInstanceLevel(LevelInstance);
	check(LevelInstanceLevel);

	return MoveActorsToLevel(ActorsToMove, LevelInstanceLevel, OutActors);
}

ILevelInstanceInterface* ULevelInstanceSubsystem::CreateLevelInstanceFrom(const TArray<AActor*>& ActorsToMove, const FNewLevelInstanceParams& CreationParams)
{
	check(!bIsCreatingLevelInstance);
	TGuardValue<bool> CreateLevelInstanceGuard(bIsCreatingLevelInstance, true);
	ULevel* CurrentLevel = GetWorld()->GetCurrentLevel();
		
	if (ActorsToMove.Num() == 0)
	{
		UE_LOG(LogLevelInstance, Warning, TEXT("Failed to create Level Instance from empty actor array"));
		return nullptr;
	}
		
	FBox ActorLocationBox(ForceInit);
	for (const AActor* ActorToMove : ActorsToMove)
	{
		const bool bNonColliding = true;
		const bool bIncludeChildren = true;
		FBox LocalActorLocationBox = ActorToMove->GetComponentsBoundingBox(bNonColliding, bIncludeChildren);
		// In the case where we receive an invalid bounding box, use actor's location if it has a root component
		if (!LocalActorLocationBox.IsValid && ActorToMove->GetRootComponent())
		{
			LocalActorLocationBox = FBox({ ActorToMove->GetActorLocation() });
		}
		ActorLocationBox += LocalActorLocationBox;

		FText Reason;
		if (!CanMoveActorToLevel(ActorToMove, &Reason))
		{
			UE_LOG(LogLevelInstance, Warning, TEXT("%s"), *Reason.ToString());
			return nullptr;
		}
	}

	FVector LevelInstanceLocation;
	if (CreationParams.PivotType == ELevelInstancePivotType::Actor)
	{
		check(CreationParams.PivotActor);
		LevelInstanceLocation = CreationParams.PivotActor->GetActorLocation();
	}
	else if (CreationParams.PivotType == ELevelInstancePivotType::WorldOrigin)
	{
		LevelInstanceLocation = FVector(0.f, 0.f, 0.f);
	}
	else
	{
		LevelInstanceLocation = ActorLocationBox.GetCenter();
		if (CreationParams.PivotType == ELevelInstancePivotType::CenterMinZ)
		{
			LevelInstanceLocation.Z = ActorLocationBox.Min.Z;
		}
	}
		
	FString LevelFilename;
	if (!CreationParams.LevelPackageName.IsEmpty())
	{
		LevelFilename = FPackageName::LongPackageNameToFilename(CreationParams.LevelPackageName, FPackageName::GetMapPackageExtension());
	}

	// Tell current level edit to stop listening because management of packages to save is done here (operation is atomic and can't be undone)
	if (LevelInstanceEdit)
	{
		LevelInstanceEdit->EditorObject->bCreatingChildLevelInstance = true;
	}
	ON_SCOPE_EXIT
	{
		if (LevelInstanceEdit)
		{
			LevelInstanceEdit->EditorObject->bCreatingChildLevelInstance = false;
		}
	};
	
	TSet<FName> DirtyPackages;

	// Capture Packages before Moving actors as they can get GCed in the process
	for (AActor* ActorToMove : ActorsToMove)
	{
		// Don't force saving of unsaved/temp packages onto the user.
		if (!FPackageName::IsTempPackage(ActorToMove->GetPackage()->GetName()))
		{
			DirtyPackages.Add(ActorToMove->GetPackage()->GetFName());
		}
	}

	ULevelStreamingLevelInstanceEditor* LevelStreaming = nullptr;
	{
		const bool bIsPartitioned = GetWorld()->IsPartitionedWorld();
		LevelStreaming = StaticCast<ULevelStreamingLevelInstanceEditor*>(EditorLevelUtils::CreateNewStreamingLevelForWorld(
		*GetWorld(), ULevelStreamingLevelInstanceEditor::StaticClass(), CreationParams.UseExternalActors(), LevelFilename, &ActorsToMove, CreationParams.TemplateWorld, /*bUseSaveAs*/true, bIsPartitioned, [this, bIsPartitioned, &ActorsToMove](ULevel* InLevel)
		{
			if (InLevel->IsUsingExternalActors())
			{
				// UWorldFactory::FactoryCreateNew will modify the default brush to be in global space (see GEditor->InitBuilderBrush(NewWorld)).
				// The level is about to be saved, it doesn't have a transform and it is not yet added to the world levels.
				// Since, no logic will remove the transform on the actor, force its transform to identity here.
				if (ABrush* Brush = InLevel->GetDefaultBrush())
				{
					Brush->GetRootComponent()->SetRelativeTransform(FTransform::Identity);
				}
			}
			if (bIsPartitioned)
			{
				UWorldPartition* WorldPartition = InLevel->GetWorldPartition();
				check(WorldPartition);
				
				// Flag the world partition that it can be used by a Level Instance
				WorldPartition->SetCanBeUsedByLevelInstance(true);

				// Make sure new level's AWorldDataLayers contains all the necessary Data Layer Instances before moving actors
				if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
				{
					TSet<TObjectPtr<const UDataLayerAsset>> SourceDataLayerAssets;
					for (AActor* ActorToMove : ActorsToMove)
					{
						// Use the raw asset list as we don't want parent DataLayers
						SourceDataLayerAssets.Append(ActorToMove->GetDataLayerAssets());
					}
					AWorldDataLayers* WorldDataLayers = InLevel->GetWorldDataLayers();
					check(WorldDataLayers);

					for (const UDataLayerAsset* SourceDataLayerAsset : SourceDataLayerAssets)
					{
						if (UDataLayerInstanceWithAsset* SourceDataLayerInstance = Cast<UDataLayerInstanceWithAsset>(DataLayerSubsystem->GetDataLayerInstanceFromAsset(SourceDataLayerAsset)))
						{
							UDataLayerInstanceWithAsset* DataLayerInstanceWithAsset = WorldDataLayers->CreateDataLayer<UDataLayerInstanceWithAsset>(SourceDataLayerAsset);
						}
					}
				}

				// Validation
				check(InLevel->IsUsingActorFolders());
				check(!WorldPartition->IsStreamingEnabled());
			}
		}));
	}

	if (!LevelStreaming)
	{
		UE_LOG(LogLevelInstance, Warning, TEXT("Failed to create new Level"));
		return nullptr;
	}
		
	ULevel* LoadedLevel = LevelStreaming->GetLoadedLevel();
	check(LoadedLevel);
		
	// @todo_ow : Decide if we want to re-create the same hierarchy as the source level.
	for (AActor* Actor : LoadedLevel->Actors)
	{
		if (Actor)
		{
			Actor->SetFolderPath_Recursively(NAME_None);
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = CurrentLevel;
	AActor* NewLevelInstanceActor = nullptr;
	TSoftObjectPtr<UWorld> WorldPtr(LoadedLevel->GetTypedOuter<UWorld>());
			
	// Make sure newly created level asset gets scanned
	ULevel::ScanLevelAssets(LoadedLevel->GetPackage()->GetName());

	if (CreationParams.Type == ELevelInstanceCreationType::LevelInstance)
	{
		TSubclassOf<AActor> ActorClass = ALevelInstance::StaticClass();
		if (CreationParams.LevelInstanceClass)
		{
			ActorClass = CreationParams.LevelInstanceClass;
		}

		check(ActorClass->ImplementsInterface(ULevelInstanceInterface::StaticClass()));

		NewLevelInstanceActor = GetWorld()->SpawnActor<AActor>(ActorClass, SpawnParams);
	}
	else
	{
		check(CreationParams.Type == ELevelInstanceCreationType::PackedLevelActor);
		FString PackageDir = FPaths::GetPath(WorldPtr.GetLongPackageName());
		FString AssetName = FPackedLevelActorBuilder::GetPackedBPPrefix() + WorldPtr.GetAssetName();
		FString BPAssetPath = FString::Format(TEXT("{0}/{1}.{1}"), { PackageDir , AssetName });
		const bool bCompile = true;

		UBlueprint* NewBP = nullptr;
		if (CreationParams.LevelPackageName.IsEmpty())
		{
			NewBP = FPackedLevelActorBuilder::CreatePackedLevelActorBlueprintWithDialog(TSoftObjectPtr<UBlueprint>(BPAssetPath), WorldPtr, bCompile);
		}
		else
		{
			NewBP = FPackedLevelActorBuilder::CreatePackedLevelActorBlueprint(TSoftObjectPtr<UBlueprint>(BPAssetPath), WorldPtr, bCompile);
		}
				
		if (NewBP)
		{
			NewLevelInstanceActor = GetWorld()->SpawnActor<APackedLevelActor>(NewBP->GeneratedClass, SpawnParams);
		}

		if (!NewLevelInstanceActor)
		{
			UE_LOG(LogLevelInstance, Warning, TEXT("Failed to create packed level blueprint. Creating non blueprint packed level instance instead."));
			NewLevelInstanceActor = GetWorld()->SpawnActor<APackedLevelActor>(APackedLevelActor::StaticClass(), SpawnParams);
		}
	}
	
	check(NewLevelInstanceActor);

	ILevelInstanceInterface* NewLevelInstance = CastChecked<ILevelInstanceInterface>(NewLevelInstanceActor);
	NewLevelInstance->SetWorldAsset(WorldPtr);
	NewLevelInstanceActor->SetActorLocation(LevelInstanceLocation);
	NewLevelInstanceActor->SetActorLabel(WorldPtr.GetAssetName());
	
	// Actors were moved and kept their World positions so when saving we want their positions to actually be relative to the LevelInstance Actor
	// so we set the LevelTransform and we mark the level as having moved its actors. 
	// On Level save FLevelUtils::RemoveEditorTransform will fixup actor transforms to make them relative to the LevelTransform.
	LevelStreaming->LevelTransform = NewLevelInstanceActor->GetActorTransform();
	LoadedLevel->bAlreadyMovedActors = true;

	GEditor->SelectNone(false, true);
	GEditor->SelectActor(NewLevelInstanceActor, true, true);

	NewLevelInstance->OnEdit();

	// Notify parents of edit
	TArray<FLevelInstanceID> AncestorIDs;
	ForEachLevelInstanceAncestors(NewLevelInstanceActor, [&AncestorIDs](ILevelInstanceInterface* InAncestor)
	{
		AncestorIDs.Add(InAncestor->GetLevelInstanceID());
		return true;
	});

	for (const FLevelInstanceID& AncestorID : AncestorIDs)
	{
		OnEditChild(AncestorID);
	}
	
	// New level instance
	const FLevelInstanceID NewLevelInstanceID = NewLevelInstance->GetLevelInstanceID();

	struct FStackLevelInstanceEdit : public FGCObject
	{
		TUniquePtr<FLevelInstanceEdit> LevelInstanceEdit;

		virtual ~FStackLevelInstanceEdit() {}

		//~ FGCObject
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			if (LevelInstanceEdit)
			{
				LevelInstanceEdit->AddReferencedObjects(Collector);
			}
		}
		virtual FString GetReferencerName() const override
		{
			return TEXT("FStackLevelInstanceEdit");
		}
		//~ FGCObject
	};

	FStackLevelInstanceEdit StackLevelInstanceEdit;
	StackLevelInstanceEdit.LevelInstanceEdit = MakeUnique<FLevelInstanceEdit>(LevelStreaming, NewLevelInstance->GetLevelInstanceID());
	// Force mark it as changed
	StackLevelInstanceEdit.LevelInstanceEdit->MarkCommittedChanges();

	GetWorld()->SetCurrentLevel(LoadedLevel);

	// Commit will always pop the actor editor context, make sure to push one here
	UActorEditorContextSubsystem::Get()->PushContext();
	bool bCommitted = CommitLevelInstanceInternal(StackLevelInstanceEdit.LevelInstanceEdit, /*bDiscardEdits=*/false, /*bDiscardOnFailure=*/true, &DirtyPackages);
	check(bCommitted);
	check(!StackLevelInstanceEdit.LevelInstanceEdit);

	// In case Commit caused actor to be GCed (Commit can cause BP reinstancing)
	NewLevelInstanceActor = Cast<AActor>(GetLevelInstance(NewLevelInstanceID));

	// Don't force saving of unsaved/temp packages onto the user. 
	if (!FPackageName::IsTempPackage(NewLevelInstanceActor->GetPackage()->GetName()))
	{
		FEditorFileUtils::PromptForCheckoutAndSave({ NewLevelInstanceActor->GetPackage() }, /*bCheckDirty*/false, /*bPromptToSave*/false);
	}

	// EditorLevelUtils::CreateNewStreamingLevelForWorld deactivates all modes. Re-activate if needed
	if (LevelInstanceEdit)
	{
		ILevelInstanceEditorModule& EditorModule = FModuleManager::GetModuleChecked<ILevelInstanceEditorModule>("LevelInstanceEditor");
		EditorModule.ActivateEditorMode();
	}

	// After commit, CurrentLevel goes back to world's PersistentLevel. Set it back to the current editing level instance (if any).
	if (ILevelInstanceInterface* EditingLevelInstance = GetEditingLevelInstance())
	{
		SetCurrent(EditingLevelInstance);
	}

	return GetLevelInstance(NewLevelInstanceID);
}

bool ULevelInstanceSubsystem::BreakLevelInstance(ILevelInstanceInterface* LevelInstance, uint32 Levels /* = 1 */, TArray<AActor*>* OutMovedActors /* = nullptr */)
{
	const double StartTime = FPlatformTime::Seconds();

	const uint32 bAvoidRelabelOnPasteSelected = GetMutableDefault<ULevelEditorMiscSettings>()->bAvoidRelabelOnPasteSelected;
	ON_SCOPE_EXIT { GetMutableDefault<ULevelEditorMiscSettings>()->bAvoidRelabelOnPasteSelected = bAvoidRelabelOnPasteSelected; };
	GetMutableDefault<ULevelEditorMiscSettings>()->bAvoidRelabelOnPasteSelected = 1;

	TArray<AActor*> MovedActors;
	BreakLevelInstance_Impl(LevelInstance, Levels, MovedActors);

	USelection* ActorSelection = GEditor->GetSelectedActors();
	ActorSelection->BeginBatchSelectOperation();
	for (AActor* MovedActor : MovedActors)
	{
		GEditor->SelectActor(MovedActor, true, false);
	}
	ActorSelection->EndBatchSelectOperation(false);

	bool bStatus = MovedActors.Num() > 0;

	const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogLevelInstance, Log, TEXT("Break took %s seconds (%s actors)"), *FText::AsNumber(ElapsedTime).ToString(), *FText::AsNumber(MovedActors.Num()).ToString());
	
	if (OutMovedActors)
	{
		*OutMovedActors = MoveTemp(MovedActors);
	}

	return bStatus;
}

void ULevelInstanceSubsystem::BreakLevelInstance_Impl(ILevelInstanceInterface* LevelInstance, uint32 Levels, TArray<AActor*>& OutMovedActors)
{
	if (Levels > 0)
	{
		AActor* LevelInstanceActor = CastChecked<AActor>(LevelInstance);
		// Can only break the top level LevelInstance
		check(LevelInstanceActor->GetLevel() == GetWorld()->GetCurrentLevel());

		// Actors in a packed level actor will not be streamed in unless they are editing. Must force this before moving.
		if (LevelInstanceActor->IsA<APackedLevelActor>())
		{
			BlockLoadLevelInstance(LevelInstance);
		}

		// need to ensure that LevelInstance has been streamed in fully
		GEngine->BlockTillLevelStreamingCompleted(LevelInstanceActor->GetWorld());

		// Cannot break a level instance which has a level script
		if (LevelInstanceHasLevelScriptBlueprint(LevelInstance))
		{
			UE_LOG(LogLevelInstance, Warning, TEXT("Failed to completely break Level Instance because some children have Level Scripts."));

			if (LevelInstanceActor->IsA<APackedLevelActor>())
			{
				BlockUnloadLevelInstance(LevelInstance);
			}
			return;
		}

		TArray<const UDataLayerInstance*> LevelInstanceDataLayerInstances = LevelInstanceActor->GetDataLayerInstances();

		TSet<AActor*> ActorsToMove;
		TFunction<bool(AActor*)> AddActorToMove = [this, &ActorsToMove, &AddActorToMove, &LevelInstanceDataLayerInstances](AActor* Actor)
		{
			if (ActorsToMove.Contains(Actor))
			{
				return true;
			}

			// Skip some actor types
			// @todo_ow: Move this logic in a new virtual function
			if ((Actor != Actor->GetLevel()->GetDefaultBrush()) &&
				!Actor->IsA<ALevelBounds>() &&
				!Actor->IsA<AWorldSettings>() &&
				!Actor->IsA<ALevelInstanceEditorInstanceActor>() &&
				!Actor->IsA<AWorldDataLayers>() &&
				!Actor->IsA<AWorldPartitionMiniMap>())
			{
				if (CanMoveActorToLevel(Actor))
				{
					FSetActorHiddenInSceneOutliner Show(Actor, false);

					// Detach if Parent Actor can't be moved
					if (AActor* ParentActor = Actor->GetAttachParentActor())
					{
						if (!AddActorToMove(ParentActor))
						{
							Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
						}
					}

					// Apply the same data layer settings to the actors to move out
					if (Actor->SupportsDataLayer())
					{
						for (const UDataLayerInstance* DataLayerInstance : LevelInstanceDataLayerInstances)
						{
							Actor->AddDataLayer(DataLayerInstance);
						}
					}

					ActorsToMove.Add(Actor);
					return true;
				}
			}

			return false;
		};

		ForEachActorInLevelInstance(LevelInstance, [this, &ActorsToMove, &AddActorToMove](AActor* Actor)
		{
			AddActorToMove(Actor);
			return true;
		});

		ULevel* DestinationLevel = GetWorld()->GetCurrentLevel();
		check(DestinationLevel);

		const bool bWarnAboutReferences = true;
		const bool bWarnAboutRenaming = false;
		const bool bMoveAllOrFail = true;
		if (!EditorLevelUtils::CopyActorsToLevel(ActorsToMove.Array(), DestinationLevel, bWarnAboutReferences, bWarnAboutRenaming, bMoveAllOrFail))
		{
			UE_LOG(LogLevelInstance, Warning, TEXT("Failed to break Level Instance because not all actors could be moved"));
			return;
		}

		// Clear undo buffer here because operation of Breaking a level instance is not undoable.
		// - Do it before Unload and Destroy calls because those calls will try and unload the level.
		//	 The unloading of the level might cause a call to UEngine::FindAndPrintStaleReferencesToObject
		//	 which will slow down the unloading because the Trans buffer has some references to the moved actors.
		if (GEditor->Trans)
		{
			GEditor->Trans->Reset(LOCTEXT("BreakLevelInstance", "Break Level Instance"));
		}

		if (LevelInstanceActor->IsA<APackedLevelActor>())
		{
			BlockUnloadLevelInstance(LevelInstance);
		}

		// Destroy the old LevelInstance instance actor
		GetWorld()->DestroyActor(LevelInstanceActor);
	
		const bool bContinueBreak = Levels > 1;
		TArray<ILevelInstanceInterface*> Children;

		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			AActor* Actor = Cast<AActor>(*It);

			if (Actor)
			{
				OutMovedActors.Add(Actor);
			}

			// Break up any sub LevelInstances if more levels are requested
			if (bContinueBreak)
			{
				if (ILevelInstanceInterface* ChildLevelInstance = Cast<ILevelInstanceInterface>(Actor))
				{
					OutMovedActors.Remove(Actor);

					Children.Add(ChildLevelInstance);
				}
			}
		}

		for (auto& Child : Children)
		{
			BreakLevelInstance_Impl(Child, Levels - 1, OutMovedActors);
		}
	}

	return;
}

ULevel* ULevelInstanceSubsystem::GetLevelInstanceLevel(const ILevelInstanceInterface* LevelInstance) const
{
	if (LevelInstance->HasValidLevelInstanceID())
	{
		if (const FLevelInstanceEdit* CurrentEdit = GetLevelInstanceEdit(LevelInstance))
		{
			return LevelInstanceEdit->LevelStreaming->GetLoadedLevel();
		}
		else if (const FLevelInstance* LevelInstanceEntry = LoadedLevelInstances.Find(LevelInstance->GetLevelInstanceID()))
		{
			return LevelInstanceEntry->LevelStreaming->GetLoadedLevel();
		}
	}

	return nullptr;
}

bool ULevelInstanceSubsystem::LevelInstanceHasLevelScriptBlueprint(const ILevelInstanceInterface* LevelInstance) const
{
	if (LevelInstance)
	{
		if (ULevel* LevelInstanceLevel = GetLevelInstanceLevel(LevelInstance))
		{
			if (ULevelScriptBlueprint* LevelScriptBP = LevelInstanceLevel->GetLevelScriptBlueprint(true))
			{
				TArray<UEdGraph*> AllGraphs;
				LevelScriptBP->GetAllGraphs(AllGraphs);
				for (UEdGraph* CurrentGraph : AllGraphs)
				{
					for (UEdGraphNode* Node : CurrentGraph->Nodes)
					{
						if (!Node->IsAutomaticallyPlacedGhostNode())
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

void ULevelInstanceSubsystem::RemoveLevelsFromWorld(const TArray<ULevel*>& InLevels, bool bResetTrans)
{
	if (LevelsToRemoveScope && LevelsToRemoveScope->IsValid())
	{
		for (ULevel* Level : InLevels)
		{
			LevelsToRemoveScope->Levels.AddUnique(Level);
		}
		LevelsToRemoveScope->bResetTrans |= bResetTrans;
	}
	else
	{
		// No need to clear the whole editor selection since actor of this level will be removed from the selection by: UEditorEngine::OnLevelRemovedFromWorld
		EditorLevelUtils::RemoveLevelsFromWorld(InLevels, /*bClearSelection*/false, bResetTrans);
	}
}

void ULevelInstanceSubsystem::FActorDescContainerInstanceManager::FActorDescContainerInstance::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Container);
}

void ULevelInstanceSubsystem::FActorDescContainerInstanceManager::FActorDescContainerInstance::UpdateBounds()
{
	Bounds.Init();
	for (FActorDescList::TIterator<> ActorDescIt(Container); ActorDescIt; ++ActorDescIt)
	{
		if (ActorDescIt->GetActorNativeClass()->IsChildOf<ALevelBounds>())
		{
			continue;
		}
		Bounds += ActorDescIt->GetBounds();
	}
}

void ULevelInstanceSubsystem::FActorDescContainerInstanceManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& [Name, ContainerInstance] : ActorDescContainers)
	{
		ContainerInstance.AddReferencedObjects(Collector);
	}
}

UActorDescContainer* ULevelInstanceSubsystem::FActorDescContainerInstanceManager::RegisterContainer(FName PackageName, UWorld* InWorld)
{
	FActorDescContainerInstance* ExistingContainerInstance = &ActorDescContainers.FindOrAdd(PackageName);
	UActorDescContainer* ActorDescContainer = ExistingContainerInstance->Container;
	
	if (ExistingContainerInstance->RefCount++ == 0)
	{
		ActorDescContainer = NewObject<UActorDescContainer>(GetTransientPackage());
		ExistingContainerInstance->Container = ActorDescContainer;
		
		// This will potentially invalidate ExistingContainerInstance due to ActorDescContainers reallocation
		ActorDescContainer->Initialize({ InWorld, PackageName });
			
		ExistingContainerInstance = &ActorDescContainers.FindChecked(PackageName);
		ExistingContainerInstance->UpdateBounds();
	}

	check(ActorDescContainer->GetWorld() == InWorld);
	return ActorDescContainer;
}

void ULevelInstanceSubsystem::FActorDescContainerInstanceManager::UnregisterContainer(UActorDescContainer* Container)
{
	FName PackageName = Container->GetContainerPackage();
	FActorDescContainerInstance& ExistingContainerInstance = ActorDescContainers.FindChecked(PackageName);

	if (--ExistingContainerInstance.RefCount == 0)
	{
		ExistingContainerInstance.Container->Uninitialize();
		ActorDescContainers.FindAndRemoveChecked(PackageName);
	}
}

FBox ULevelInstanceSubsystem::FActorDescContainerInstanceManager::GetContainerBounds(FName PackageName) const
{
	if (const FActorDescContainerInstance* ActorDescContainerInstance = ActorDescContainers.Find(PackageName))
	{
		return ActorDescContainerInstance->Bounds;
	}
	return FBox(ForceInit);
}

void ULevelInstanceSubsystem::FActorDescContainerInstanceManager::OnLevelInstanceActorCommitted(ILevelInstanceInterface* LevelInstance)
{
	const FName PackageName = *LevelInstance->GetWorldAssetPackage();
	if (FActorDescContainerInstance* ActorDescContainerInstance = ActorDescContainers.Find(PackageName))
	{
		ActorDescContainerInstance->UpdateBounds();
	}
}

ULevelInstanceSubsystem::FLevelsToRemoveScope::FLevelsToRemoveScope(ULevelInstanceSubsystem* InOwner)
	: Owner(InOwner)
	, bIsBeingDestroyed(false)
{}

ULevelInstanceSubsystem::FLevelsToRemoveScope::~FLevelsToRemoveScope()
{
	if (Levels.Num() > 0)
	{
		bIsBeingDestroyed = true;
		double StartTime = FPlatformTime::Seconds();
		ULevelInstanceSubsystem* LevelInstanceSubsystem = Owner.Get();
		check(LevelInstanceSubsystem);
		LevelInstanceSubsystem->RemoveLevelsFromWorld(Levels, bResetTrans);
		double ElapsedTime = FPlatformTime::Seconds() - StartTime;
		UE_LOG(LogLevelInstance, Log, TEXT("Unloaded %s levels in %s seconds"), *FText::AsNumber(Levels.Num()).ToString(), *FText::AsNumber(ElapsedTime).ToString());
	}
}

bool ULevelInstanceSubsystem::CanMoveActorToLevel(const AActor* Actor, FText* OutReason) const
{
	if (Actor->IsA<ALevelInstancePivot>())
	{
		return false;
	}

	if (Actor->GetWorld() == GetWorld())
	{
		if (const ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(Actor))
		{
			if (IsEditingLevelInstance(LevelInstance))
			{
				if (OutReason != nullptr)
				{
					*OutReason = LOCTEXT("CanMoveActorLevelEditing", "Can't move Level Instance actor while it is being edited");
				}
				return false;
			}

			bool bEditingChildren = false;
			ForEachLevelInstanceChild(LevelInstance, true, [this, &bEditingChildren](const ILevelInstanceInterface* ChildLevelInstance)
			{
				if (IsEditingLevelInstance(ChildLevelInstance))
				{
					bEditingChildren = true;
					return false;
				}
				return true;
			});

			if (bEditingChildren)
			{
				if (OutReason != nullptr)
				{
					*OutReason = LOCTEXT("CanMoveActorToLevelChildEditing", "Can't move Level Instance actor while one of its child Level Instance is being edited");
				}
				return false;
			}
		}
	}

	return true;
}

void ULevelInstanceSubsystem::OnActorDeleted(AActor* Actor)
{
	if (ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(Actor))
	{
		if (Actor->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			// We are receiving this event when destroying the old actor after BP reinstantiation. In this case,
			// the newly created actor was already added to the list, so we can safely ignore this case.
			check(GIsReinstancing);
			return;
		}

		// Unregistered Level Instance Actor nothing to do.
		if (!LevelInstance->HasValidLevelInstanceID())
		{
			return;
		}

		const bool bIsEditingLevelInstance = IsEditingLevelInstance(LevelInstance);
		if (!bIsEditingLevelInstance && Actor->IsA<APackedLevelActor>())
		{
			return;
		}

		const bool bAlreadyRooted = Actor->IsRooted();
		// Unloading LevelInstances leads to GC and Actor can be collected. Add to root temp. It will get collected after the OnActorDeleted callbacks
		if (!bAlreadyRooted)
		{
			Actor->AddToRoot();
		}

		FScopedSlowTask SlowTask(0, LOCTEXT("UnloadingLevelInstances", "Unloading Level Instances..."), !GetWorld()->IsGameWorld());
		SlowTask.MakeDialog();
		check(!IsEditingLevelInstanceDirty(LevelInstance) && !HasDirtyChildrenLevelInstances(LevelInstance));
		if (bIsEditingLevelInstance)
		{
			CommitLevelInstance(LevelInstance);
		}
		else
		{
			// We are ending editing. Discard Non dirty child edits
			ForEachLevelInstanceChild(LevelInstance, /*bRecursive=*/true, [this](const ILevelInstanceInterface* ChildLevelInstance)
			{
				if (const FLevelInstanceEdit* ChildLevelInstanceEdit = GetLevelInstanceEdit(ChildLevelInstance))
				{
					check(!IsLevelInstanceEditDirty(ChildLevelInstanceEdit));
					ResetEdit(LevelInstanceEdit);
					return false;
				}
				return true;
			});
		}

		LevelInstancesToLoadOrUpdate.Remove(LevelInstance);
				
		UnloadLevelInstance(LevelInstance->GetLevelInstanceID());
		
		// Remove from root so it gets collected on the next GC if it can be.
		if (!bAlreadyRooted)
		{
			Actor->RemoveFromRoot();
		}
	}
}

bool ULevelInstanceSubsystem::ShouldIgnoreDirtyPackage(UPackage* DirtyPackage, const UWorld* EditingWorld)
{
	if (DirtyPackage == EditingWorld->GetOutermost())
	{
		return false;
	}

	bool bIgnore = true;
	ForEachObjectWithPackage(DirtyPackage, [&bIgnore, EditingWorld](UObject* Object)
	{
		if (Object->GetOutermostObject() == EditingWorld)
		{
			bIgnore = false;
		}

		return bIgnore;
	});

	return bIgnore;
}

ULevelInstanceSubsystem::FLevelInstanceEdit::FLevelInstanceEdit(ULevelStreamingLevelInstanceEditor* InLevelStreaming, FLevelInstanceID InLevelInstanceID)
	: LevelStreaming(InLevelStreaming)
{
	LevelStreaming->LevelInstanceID = InLevelInstanceID;
	EditorObject = NewObject<ULevelInstanceEditorObject>(GetTransientPackage(), NAME_None, RF_Transactional);
	EditorObject->EnterEdit(GetEditWorld());
}

ULevelInstanceSubsystem::FLevelInstanceEdit::~FLevelInstanceEdit()
{
	EditorObject->ExitEdit();
	ULevelStreamingLevelInstanceEditor::Unload(LevelStreaming);
}

UWorld* ULevelInstanceSubsystem::FLevelInstanceEdit::GetEditWorld() const
{
	if (LevelStreaming && LevelStreaming->GetLoadedLevel())
	{
		return LevelStreaming->GetLoadedLevel()->GetTypedOuter<UWorld>();
	}

	return nullptr;
}

void ULevelInstanceSubsystem::FLevelInstanceEdit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(EditorObject);
	Collector.AddReferencedObject(LevelStreaming);
}

bool ULevelInstanceSubsystem::FLevelInstanceEdit::CanDiscard(FText* OutReason) const
{
	return EditorObject->CanDiscard(OutReason);
}

bool ULevelInstanceSubsystem::FLevelInstanceEdit::HasCommittedChanges() const
{
	return EditorObject->bCommittedChanges;
}

void ULevelInstanceSubsystem::FLevelInstanceEdit::MarkCommittedChanges()
{
	EditorObject->bCommittedChanges = true;
}

void ULevelInstanceSubsystem::FLevelInstanceEdit::GetPackagesToSave(TArray<UPackage*>& OutPackagesToSave) const
{
	const UWorld* EditingWorld = GetEditWorld();
	check(EditingWorld);

	FEditorFileUtils::GetDirtyPackages(OutPackagesToSave, [EditingWorld](UPackage* DirtyPackage)
	{
		return ULevelInstanceSubsystem::ShouldIgnoreDirtyPackage(DirtyPackage, EditingWorld);
	});

	for (const TWeakObjectPtr<UPackage>& WeakPackageToSave : EditorObject->OtherPackagesToSave)
	{
		if (UPackage* PackageToSave = WeakPackageToSave.Get())
		{
			OutPackagesToSave.Add(PackageToSave);
		}
	}
}

FLevelInstanceID ULevelInstanceSubsystem::FLevelInstanceEdit::GetLevelInstanceID() const
{
	return LevelStreaming ? LevelStreaming->GetLevelInstanceID() : FLevelInstanceID();
}

const ULevelInstanceSubsystem::FLevelInstanceEdit* ULevelInstanceSubsystem::GetLevelInstanceEdit(const ILevelInstanceInterface* LevelInstance) const
{
	if (LevelInstance && LevelInstanceEdit && LevelInstanceEdit->GetLevelInstanceID() == LevelInstance->GetLevelInstanceID())
	{
		return LevelInstanceEdit.Get();
	}

	return nullptr;
}

bool ULevelInstanceSubsystem::IsEditingLevelInstanceDirty(const ILevelInstanceInterface* LevelInstance) const
{
	const FLevelInstanceEdit* CurrentEdit = GetLevelInstanceEdit(LevelInstance);
	if (!CurrentEdit)
	{
		return false;
	}

	return IsLevelInstanceEditDirty(CurrentEdit);
}

bool ULevelInstanceSubsystem::IsLevelInstanceEditDirty(const FLevelInstanceEdit* InLevelInstanceEdit) const
{
	TArray<UPackage*> OutPackagesToSave;
	InLevelInstanceEdit->GetPackagesToSave(OutPackagesToSave);
	return OutPackagesToSave.Num() > 0;
}

ILevelInstanceInterface* ULevelInstanceSubsystem::GetEditingLevelInstance() const
{
	if (LevelInstanceEdit)
	{
		return GetLevelInstance(LevelInstanceEdit->GetLevelInstanceID());
	}

	return nullptr;
}

bool ULevelInstanceSubsystem::CanEditLevelInstance(const ILevelInstanceInterface* LevelInstance, FText* OutReason) const
{
	// Only allow Editing in Editor World
	if (GetWorld()->WorldType != EWorldType::Editor)
	{
		return false;
	}

	if (ULevel* LevelInstanceLevel = GetLevelInstanceLevel(LevelInstance))
	{
		if (UWorldPartition* WorldPartition = LevelInstanceLevel->GetWorldPartition())
		{
			if (!WorldPartition->CanBeUsedByLevelInstance())
			{
				if (OutReason)
				{
					*OutReason = FText::Format(LOCTEXT("CanEditPartitionedLevelInstance", "LevelInstance doesn't support partitioned world {0}, make sure to flag world partition's 'Can be Used by Level Instance'."), FText::FromString(LevelInstance->GetWorldAssetPackage()));
				}
				return false;
			}
		}
	}
	
	if (LevelInstanceEdit)
	{
		if (LevelInstanceEdit->GetLevelInstanceID() == LevelInstance->GetLevelInstanceID())
		{
			if (OutReason)
			{
				*OutReason = FText::Format(LOCTEXT("CanEditLevelInstanceAlreadyBeingEdited", "Level Instance already being edited ({0})."), FText::FromString(LevelInstance->GetWorldAssetPackage()));
			}
			return false;
		}

		if (IsLevelInstanceEditDirty(LevelInstanceEdit.Get()))
		{
			if (OutReason)
			{
				*OutReason = FText::Format(LOCTEXT("CanEditLevelInstanceDirtyEdit", "Current Level Instance has unsaved changes and needs to be committed first ({0})."), FText::FromString(GetEditingLevelInstance()->GetWorldAssetPackage()));
			}
			return false;
		}
	}
	
	if (LevelInstance->IsWorldAssetValid())
	{
		if (GetWorld()->PersistentLevel->GetPackage()->GetName() == LevelInstance->GetWorldAssetPackage())
		{
			if (OutReason)
			{
				*OutReason = FText::Format(LOCTEXT("CanEditLevelInstancePersistentLevel", "The Persistent level and the Level Instance are the same ({0})."), FText::FromString(LevelInstance->GetWorldAssetPackage()));
			}
			return false;
		}

		if (FLevelUtils::FindStreamingLevel(GetWorld(), *LevelInstance->GetWorldAssetPackage()))
		{
			if (OutReason)
			{
				*OutReason = FText::Format(LOCTEXT("CanEditLevelInstanceAlreadyExists", "The same level was added to world outside of Level Instances ({0})."), FText::FromString(LevelInstance->GetWorldAssetPackage()));
			}
			return false;
		}

		FPackagePath WorldAssetPath;
		if (!FPackagePath::TryFromPackageName(LevelInstance->GetWorldAssetPackage(), WorldAssetPath) || !FPackageName::DoesPackageExist(WorldAssetPath))
		{
			if (OutReason)
			{
				*OutReason = FText::Format(LOCTEXT("CanEditLevelInstanceInvalidAsset", "Level Instance asset is invalid ({0})."), FText::FromString(LevelInstance->GetWorldAssetPackage()));
			}
			return false;
		}
	}

	return true;
}

bool ULevelInstanceSubsystem::CanCommitLevelInstance(const ILevelInstanceInterface* LevelInstance, bool bDiscardEdits, FText* OutReason) const
{
	if (const FLevelInstanceEdit* CurrentEdit = GetLevelInstanceEdit(LevelInstance))
	{
		return !bDiscardEdits || LevelInstanceEdit->CanDiscard(OutReason);
	}

	if (OutReason)
	{
		*OutReason = LOCTEXT("CanCommitLevelInstanceNotEditing", "Level Instance is not currently being edited");
	}
	return false;
}

void ULevelInstanceSubsystem::EditLevelInstance(ILevelInstanceInterface* LevelInstance, TWeakObjectPtr<AActor> ContextActorPtr)
{
	if (EditLevelInstanceInternal(LevelInstance, ContextActorPtr, FString(), false))
	{
		ILevelInstanceEditorModule& EditorModule = FModuleManager::GetModuleChecked<ILevelInstanceEditorModule>("LevelInstanceEditor");
		EditorModule.ActivateEditorMode();
	}
}

bool ULevelInstanceSubsystem::EditLevelInstanceInternal(ILevelInstanceInterface* LevelInstance, TWeakObjectPtr<AActor> ContextActorPtr, const FString& InActorNameToSelect, bool bRecursive)
{
	check(CanEditLevelInstance(LevelInstance));
		
	FScopedSlowTask SlowTask(0, LOCTEXT("BeginEditLevelInstance", "Loading Level Instance for edit..."), !GetWorld()->IsGameWorld());
	SlowTask.MakeDialog();

	// Gather information from the context actor to try and select something meaningful after the loading
	FString ActorNameToSelect = InActorNameToSelect;
	if (AActor* ContextActor = ContextActorPtr.Get())
	{
		ActorNameToSelect = ContextActor->GetName();
		ForEachLevelInstanceAncestorsAndSelf(ContextActor, [&ActorNameToSelect,LevelInstance](const ILevelInstanceInterface* AncestorLevelInstance)
		{
			// stop when we hit the LevelInstance we are about to edit
			if (AncestorLevelInstance == LevelInstance)
			{
				return false;
			}
			
			ActorNameToSelect = CastChecked<AActor>(AncestorLevelInstance)->GetName();
			return true;
		});
	}

	GEditor->SelectNone(false, true);
	
	// Avoid calling OnEditChild twice  on ancestors when EditLevelInstance calls itself
	if (!bRecursive)
	{
		TArray<FLevelInstanceID> AncestorIDs;
		ForEachLevelInstanceAncestors(CastChecked<AActor>(LevelInstance), [&AncestorIDs](ILevelInstanceInterface* InAncestor)
		{
			AncestorIDs.Add(InAncestor->GetLevelInstanceID());
			return true;
		});

		for (const FLevelInstanceID& AncestorID : AncestorIDs)
		{
			OnEditChild(AncestorID);
		}
	}

	// Check if there is an open (but clean) ancestor unload it before opening the LevelInstance for editing
	if (LevelInstanceEdit)
	{	
		// Only support one level of recursion to commit current edit
		check(!bRecursive);
		FLevelInstanceID PendingEditId = LevelInstance->GetLevelInstanceID();
		
		check(!IsLevelInstanceEditDirty(LevelInstanceEdit.Get()));
		CommitLevelInstanceInternal(LevelInstanceEdit);

		ILevelInstanceInterface* LevelInstanceToEdit = GetLevelInstance(PendingEditId);
		check(LevelInstanceToEdit);

		return EditLevelInstanceInternal(LevelInstanceToEdit, nullptr, ActorNameToSelect, /*bRecursive=*/true);
	}

	// Cleanup async requests in case
	LevelInstancesToUnload.Remove(LevelInstance->GetLevelInstanceID());
	LevelInstancesToLoadOrUpdate.Remove(LevelInstance);
	// Unload right away
	UnloadLevelInstance(LevelInstance->GetLevelInstanceID());
		
	// Load Edit LevelInstance level
	ULevelStreamingLevelInstanceEditor* LevelStreaming = ULevelStreamingLevelInstanceEditor::Load(LevelInstance);
	if (!LevelStreaming)
	{
		LevelInstance->LoadLevelInstance();
		return false;
	}

	check(LevelInstanceEdit.IsValid());
	check(LevelInstanceEdit->GetLevelInstanceID() == LevelInstance->GetLevelInstanceID());
	check(LevelInstanceEdit->LevelStreaming == LevelStreaming);
		
	// Try and select something meaningful
	AActor* ActorToSelect = nullptr;
	if (!ActorNameToSelect.IsEmpty())
	{		
		ActorToSelect = FindObject<AActor>(LevelStreaming->GetLoadedLevel(), *ActorNameToSelect);
	}

	// default to LevelInstance
	AActor* LevelInstanceActor = CastChecked<AActor>(LevelInstance);
	if (!ActorToSelect)
	{
		ActorToSelect = LevelInstanceActor;
	}
	LevelInstanceActor->SetIsTemporarilyHiddenInEditor(false);

	// Notify
	LevelInstance->OnEdit();

	GEditor->SelectActor(ActorToSelect, true, true);

	for (const auto& Actor : LevelStreaming->LoadedLevel->Actors)
	{
		const bool bEditing = true;
		if (Actor)
		{
			Actor->PushLevelInstanceEditingStateToProxies(bEditing);
		}
	}
	
	// Edit can't be undone
	GEditor->ResetTransaction(LOCTEXT("LevelInstanceEditResetTrans", "Edit Level Instance"));

	// When editing a Level Instance, push a new empty actor editor context
	UActorEditorContextSubsystem::Get()->PushContext();

	ResetLoadersForWorldAsset(LevelInstance->GetWorldAsset().GetLongPackageName());

	return true;
}

void ULevelInstanceSubsystem::ResetLoadersForWorldAsset(const FString& WorldAsset)
{
	for (TObjectIterator<UWorld> It(RF_ClassDefaultObject | RF_ArchetypeObject, true); It; ++It)
	{
		UWorld* CurrentWorld = *It;
		if (IsValid(CurrentWorld))
		{
			if (ULevelInstanceSubsystem* LevelInstanceSubsystem = CurrentWorld->GetSubsystem<ULevelInstanceSubsystem>())
			{
				LevelInstanceSubsystem->ResetLoadersForWorldAssetInternal(WorldAsset);
			}
		}
	}
}

void ULevelInstanceSubsystem::ResetLoadersForWorldAssetInternal(const FString& WorldAsset)
{
	for (const auto& [LevelInstanceID, LoadedLevelInstance] : LoadedLevelInstances)
	{
		if (LoadedLevelInstance.LevelStreaming && LoadedLevelInstance.LevelStreaming->PackageNameToLoad.ToString() == WorldAsset)
		{
			LoadedLevelInstance.LevelStreaming->ResetLevelInstanceLoaders();
		}
	}
}

bool ULevelInstanceSubsystem::CommitLevelInstance(ILevelInstanceInterface* LevelInstance, bool bDiscardEdits, TSet<FName>* DirtyPackages)
{
	check(LevelInstanceEdit.Get() == GetLevelInstanceEdit(LevelInstance));
	check(CanCommitLevelInstance(LevelInstance));
	if (CommitLevelInstanceInternal(LevelInstanceEdit, bDiscardEdits, /*bDiscardOnFailure=*/false, DirtyPackages))
	{
		ILevelInstanceEditorModule& EditorModule = FModuleManager::GetModuleChecked<ILevelInstanceEditorModule>("LevelInstanceEditor");
		EditorModule.DeactivateEditorMode();

		return true;
	}

	return false;
}

bool ULevelInstanceSubsystem::CommitLevelInstanceInternal(TUniquePtr<FLevelInstanceEdit>& InLevelInstanceEdit, bool bDiscardEdits, bool bDiscardOnFailure, TSet<FName>* DirtyPackages)
{
	TGuardValue<bool> CommitScope(bIsCommittingLevelInstance, true);
	ILevelInstanceInterface* LevelInstance = GetLevelInstance(InLevelInstanceEdit->GetLevelInstanceID());
	check(InLevelInstanceEdit);
	UWorld* EditingWorld = InLevelInstanceEdit->GetEditWorld();
	check(EditingWorld);
			
	// Check with EditorObject if Discard is possible
	if (!InLevelInstanceEdit->CanDiscard())
	{
		bDiscardEdits = false;
	}

	// Build list of Packages to save
	TSet<FName> PackagesToSave;

	// First dirty packages belonging to the edit Level or external level actors that were moved into the level
	TArray<UPackage*> EditPackagesToSave;
	InLevelInstanceEdit->GetPackagesToSave(EditPackagesToSave);
	for (UPackage* PackageToSave : EditPackagesToSave)
	{
		PackagesToSave.Add(PackageToSave->GetFName());
	}

	// Second dirty packages passed in to the Commit method
	if (DirtyPackages)
	{
		PackagesToSave.Append(*DirtyPackages);
	}
		
	// Did some change get saved outside of the commit (regular saving in editor while editing)
	bool bChangesCommitted = InLevelInstanceEdit->HasCommittedChanges();
	if (PackagesToSave.Num() > 0 && !bDiscardEdits)
	{
		const bool bPromptUserToSave = false;
		const bool bSaveMapPackages = true;
		const bool bSaveContentPackages = true;
		const bool bFastSave = false;
		const bool bNotifyNoPackagesSaved = false;
		const bool bCanBeDeclined = true;

		bool bSaveSucceeded = FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined, nullptr,
			[&PackagesToSave, EditingWorld](UPackage* DirtyPackage)
			{
				if (PackagesToSave.Contains(DirtyPackage->GetFName()))
				{
					return false;
				}
				return ShouldIgnoreDirtyPackage(DirtyPackage, EditingWorld);
			});
				
		if (!bSaveSucceeded && !bDiscardOnFailure)
		{
			return false;
		}

		// Consider changes committed is was already set to true because of outside saves or if the save succeeded.
		bChangesCommitted |= bSaveSucceeded;
	}

	FScopedSlowTask SlowTask(0, LOCTEXT("EndEditLevelInstance", "Unloading Level..."), !GetWorld()->IsGameWorld());
	SlowTask.MakeDialog();

	GEditor->SelectNone(false, true);

	const FString EditPackage = LevelInstance->GetWorldAssetPackage();

	// Remove from streaming level...
	ResetEdit(InLevelInstanceEdit);

	if (bChangesCommitted)
	{
		ULevel::ScanLevelAssets(EditPackage);
	}
	
	// Backup ID on Commit in case Actor gets recreated
	const FLevelInstanceID LevelInstanceID = LevelInstance->GetLevelInstanceID();

	// Notify (Actor might get destroyed by this call if its a packed bp)
	LevelInstance->OnCommit(bChangesCommitted);

	// Update pointer since BP Compilation might have invalidated LevelInstance
	LevelInstance = GetLevelInstance(LevelInstanceID);

	ActorDescContainerInstanceManager.OnLevelInstanceActorCommitted(LevelInstance);

	TArray<TPair<ULevelInstanceSubsystem*, FLevelInstanceID>> LevelInstancesToUpdate;
	// Gather list to update
	for (TObjectIterator<UWorld> It(RF_ClassDefaultObject | RF_ArchetypeObject, true); It; ++It)
	{
		UWorld* CurrentWorld = *It;
		if (IsValid(CurrentWorld))
		{
			if (ULevelInstanceSubsystem* LevelInstanceSubsystem = CurrentWorld->GetSubsystem<ULevelInstanceSubsystem>())
			{
				TArray<ILevelInstanceInterface*> WorldLevelInstances = LevelInstanceSubsystem->GetLevelInstances(EditPackage);
				for (ILevelInstanceInterface* CurrentLevelInstance : WorldLevelInstances)
				{
					if (CurrentLevelInstance == LevelInstance || bChangesCommitted)
					{
						LevelInstancesToUpdate.Add({ LevelInstanceSubsystem, CurrentLevelInstance->GetLevelInstanceID() });
					}
				}
			}
		}
	}

	// Do update
	for (const auto& KeyValuePair : LevelInstancesToUpdate)
	{
		if (ILevelInstanceInterface* LevelInstanceToUpdate = KeyValuePair.Key->GetLevelInstance(KeyValuePair.Value))
		{
			LevelInstanceToUpdate->UpdateLevelInstanceFromWorldAsset();
		}
	}

	LevelInstance = GetLevelInstance(LevelInstanceID);
	
	// Notify Ancestors
	FLevelInstanceID LevelInstanceToSelectID = LevelInstanceID;
	TArray<FLevelInstanceID> AncestorIDs;
	ForEachLevelInstanceAncestors(CastChecked<AActor>(LevelInstance), [&LevelInstanceToSelectID, &AncestorIDs](ILevelInstanceInterface* AncestorLevelInstance)
	{
		LevelInstanceToSelectID = AncestorLevelInstance->GetLevelInstanceID();
		AncestorIDs.Add(AncestorLevelInstance->GetLevelInstanceID());
		return true;
	});

	for (const FLevelInstanceID& AncestorID : AncestorIDs)
	{
		OnCommitChild(AncestorID, bChangesCommitted);
	}
		
	if (ILevelInstanceInterface* LevelInstanceToSelect = GetLevelInstance(LevelInstanceToSelectID))
	{
		GEditor->SelectActor(CastChecked<AActor>(LevelInstanceToSelect), true, true);
	}
				
	// Wait for Level Instances to be loaded
	BlockOnLoading();

	// Send out Event if changes were committed
	if (bChangesCommitted)
	{
		LevelInstanceChangedEvent.Broadcast(FName(*EditPackage));
	}

	GEngine->BroadcastLevelActorListChanged();

	// Restore actor editor context
	UActorEditorContextSubsystem::Get()->PopContext();

	return true;
}

ILevelInstanceInterface* ULevelInstanceSubsystem::GetParentLevelInstance(const AActor* Actor) const
{
	check(Actor);
	const ULevel* OwningLevel = Actor->GetLevel();
	check(OwningLevel);
	return GetOwningLevelInstance(OwningLevel);
}

void ULevelInstanceSubsystem::BlockOnLoading()
{
	// Make sure blocking loads can happen and are not part of transaction
	TGuardValue<ITransaction*> TransactionGuard(GUndo, nullptr);

	// Blocking until LevelInstance is loaded and all its child LevelInstances
	while (LevelInstancesToLoadOrUpdate.Num())
	{
		UpdateStreamingState();
	}
}

void ULevelInstanceSubsystem::BlockLoadLevelInstance(ILevelInstanceInterface* LevelInstance)
{
	check(!LevelInstance->IsEditing());
	RequestLoadLevelInstance(LevelInstance, true);

	BlockOnLoading();
}

void ULevelInstanceSubsystem::BlockUnloadLevelInstance(ILevelInstanceInterface* LevelInstance)
{
	check(!LevelInstance->IsEditing());
	RequestUnloadLevelInstance(LevelInstance);

	BlockOnLoading();
}

bool ULevelInstanceSubsystem::HasChildEdit(const ILevelInstanceInterface* LevelInstance) const
{
	const int32* ChildEditCountPtr = ChildEdits.Find(LevelInstance->GetLevelInstanceID());
	return ChildEditCountPtr && *ChildEditCountPtr;
}

void ULevelInstanceSubsystem::OnCommitChild(const FLevelInstanceID& LevelInstanceID, bool bChildChanged)
{
	int32& ChildEditCount = ChildEdits.FindChecked(LevelInstanceID);
	check(ChildEditCount > 0);
	ChildEditCount--;

	if (ILevelInstanceInterface* LevelInstance = GetLevelInstance(LevelInstanceID))
	{
		LevelInstance->OnCommitChild(bChildChanged);
	}
}

void ULevelInstanceSubsystem::OnEditChild(const FLevelInstanceID& LevelInstanceID)
{
	int32& ChildEditCount = ChildEdits.FindOrAdd(LevelInstanceID, 0);
	// Child edit count can reach 2 maximum in the Context of creating a LevelInstance inside an already editing child level instance
	// through CreateLevelInstanceFrom
	check(ChildEditCount < 2);
	ChildEditCount++;

	if (ILevelInstanceInterface* LevelInstance = GetLevelInstance(LevelInstanceID))
	{
		LevelInstance->OnEditChild();
	}
}

TArray<ILevelInstanceInterface*> ULevelInstanceSubsystem::GetLevelInstances(const FString& WorldAssetPackage)
{
	TArray<ILevelInstanceInterface*> MatchingLevelInstances;
	for (const auto& KeyValuePair : RegisteredLevelInstances)
	{
		if (KeyValuePair.Value->GetWorldAssetPackage() == WorldAssetPackage)
		{
			MatchingLevelInstances.Add(KeyValuePair.Value);
		}
	}

	return MatchingLevelInstances;
}

void ULevelInstanceSubsystem::ForEachLevelInstanceActorAncestors(const ULevel* Level, TFunctionRef<bool(AActor*)> Operation) const
{
	AActor* CurrentActor = Cast<AActor>(GetOwningLevelInstance(Level));
	while (CurrentActor)
	{
		if (!Operation(CurrentActor))
		{
			break;
		}
		CurrentActor = Cast<AActor>(GetParentLevelInstance(CurrentActor));
	};
}

TArray<AActor*> ULevelInstanceSubsystem::GetParentLevelInstanceActors(const ULevel* Level) const
{
	TArray<AActor*> ParentActors;
	ForEachLevelInstanceActorAncestors(Level, [&ParentActors](AActor* Parent)
	{
		ParentActors.Add(Parent);
		return true;
	});
	return ParentActors;
}

// Builds and returns a string based the LevelInstance hierarchy using actor labels.
// Ex: ParentLevelInstanceActorLabel.ChildLevelInstanceActorLabel.ActorLabel
FString ULevelInstanceSubsystem::PrefixWithParentLevelInstanceActorLabels(const FString& ActorLabel, const ULevel* Level) const
{
	TStringBuilder<128> Builder;
	Builder = ActorLabel;
	ForEachLevelInstanceActorAncestors(Level, [&Builder](AActor* Parent)
	{
		if (Builder.Len())
		{
			Builder.Prepend(TEXT("."));
		}
		Builder.Prepend(Parent->GetActorLabel());
		return true;
	});
	return Builder.ToString();
}

bool ULevelInstanceSubsystem::CheckForLoop(const ILevelInstanceInterface* LevelInstance, TArray<TPair<FText, TSoftObjectPtr<UWorld>>>* LoopInfo, const ILevelInstanceInterface** LoopStart)
{
	return CheckForLoop(LevelInstance, LevelInstance->GetWorldAsset(), LoopInfo, LoopStart);
}

bool ULevelInstanceSubsystem::CheckForLoop(const ILevelInstanceInterface* LevelInstance, TSoftObjectPtr<UWorld> WorldAsset, TArray<TPair<FText, TSoftObjectPtr<UWorld>>>* LoopInfo, const ILevelInstanceInterface** LoopStart)
{
	bool bValid = true;
		
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = LevelInstance->GetLevelInstanceSubsystem())
	{
		LevelInstanceSubsystem->ForEachLevelInstanceAncestorsAndSelf(CastChecked<AActor>(LevelInstance), [&bValid, &WorldAsset, LevelInstance, LoopInfo, LoopStart](const ILevelInstanceInterface* CurrentLevelInstance)
		{
			FName PackageToTest(*WorldAsset.GetLongPackageName());
			// Check to exclude NAME_None since Preview Levels are in the transient package
			// Check the level we are spawned in to detect the loop (this will handle loops caused by LevelInstances and by regular level streaming)
			const AActor* CurrentActor = CastChecked<AActor>(CurrentLevelInstance);
			if (PackageToTest != NAME_None && CurrentActor->GetLevel()->GetPackage()->GetLoadedPath() == FPackagePath::FromPackageNameChecked(PackageToTest))
			{
				bValid = false;
				if (LoopStart)
				{
					*LoopStart = CurrentLevelInstance;
				}
			}

			if (LoopInfo)
			{
				TSoftObjectPtr<UWorld> CurrentAsset = CurrentLevelInstance == LevelInstance ? WorldAsset : CurrentLevelInstance->GetWorldAsset();
				FText LevelInstanceName = FText::FromString(CurrentActor->GetPathName());
				FText Description = FText::Format(LOCTEXT("LevelInstanceLoopLink", "-> Actor: {0} loads"), LevelInstanceName);
				LoopInfo->Emplace(Description, CurrentAsset);
			}
			
			return bValid;
		});
	}
	
	return bValid;
}

bool ULevelInstanceSubsystem::CanUsePackage(FName InPackageName)
{
	return !ULevel::GetIsLevelPartitionedFromPackage(InPackageName) || ULevel::GetPartitionedLevelCanBeUsedByLevelInstanceFromPackage(InPackageName);
}

bool ULevelInstanceSubsystem::CanUseWorldAsset(const ILevelInstanceInterface* LevelInstance, TSoftObjectPtr<UWorld> WorldAsset, FString* OutReason)
{
	// Do not validate when running convert commandlet as package might not exist yet.
	if (UWorldPartitionSubsystem::IsRunningConvertWorldPartitionCommandlet())
	{
		return true;
	}

	if (WorldAsset.IsNull())
	{
		return true;
	}

	FString PackageName;
	if (!FPackageName::DoesPackageExist(WorldAsset.GetLongPackageName()))
	{
		if (OutReason)
		{
			*OutReason = FString::Format(TEXT("Attempting to set Level Instance to package {0} which does not exist. Ensure the level was saved before attepting to set the level instance world asset."), { WorldAsset.GetLongPackageName() });
		}
		return false;
	}

	TArray<TPair<FText, TSoftObjectPtr<UWorld>>> LoopInfo;
	const ILevelInstanceInterface* LoopStart = nullptr;

	if (!ULevelInstanceSubsystem::CanUsePackage(*WorldAsset.GetLongPackageName()))
	{
		if (OutReason)
		{
			*OutReason = FString::Format(TEXT("LevelInstance doesn't support partitioned world {0}, make sure to flag world partition's 'Can be Used by Level Instance'.\n"), { WorldAsset.GetLongPackageName() });
		}

		return false;
	}

	if (!CheckForLoop(LevelInstance, WorldAsset, OutReason ? &LoopInfo : nullptr, OutReason ? &LoopStart : nullptr))
	{
		if (OutReason)
		{
			if (ensure(LoopStart))
			{
				const AActor* LoopStartActor = CastChecked<AActor>(LoopStart);
				TSoftObjectPtr<UWorld> LoopStartAsset(LoopStartActor->GetLevel()->GetTypedOuter<UWorld>());
				*OutReason = FString::Format(TEXT("Setting LevelInstance to {0} would cause loop {1}:{2}\n"), { WorldAsset.GetLongPackageName(), LoopStartActor->GetName(), LoopStartAsset.GetLongPackageName() });
				for (int32 i = LoopInfo.Num() - 1; i >= 0; --i)
				{
					OutReason->Append(FString::Format(TEXT("{0} {1}\n"), { *LoopInfo[i].Key.ToString(), *LoopInfo[i].Value.GetLongPackageName() }));
				}
			}
		}

		return false;
	}

	return true;
}

#endif

#undef LOCTEXT_NAMESPACE

