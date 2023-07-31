// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertSyncClientUtil.h"
#include "ConcertSyncArchives.h"
#include "ConcertTransactionEvents.h"
#include "ConcertLogGlobal.h"
#include "ConcertSyncSettings.h"
#include "ConcertWorkspaceData.h"

#include "UObject/Class.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "UObject/PropertyPortFlags.h"

#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"

#include "RenderingThread.h"
#include "LevelUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Engine/GameEngine.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ModelComponent.h"

#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
	#include "DirectoryWatcherModule.h"
	#include "Editor.h"
	#include "Editor/UnrealEdEngine.h"
	#include "FileHelpers.h"
	#include "GameMapsSettings.h"
	#include "IDirectoryWatcher.h"
	#include "Modules/ModuleManager.h"
	#include "ObjectTools.h"	
	#include "PackageTools.h"
	#include "Selection.h"
	#include "Subsystems/AssetEditorSubsystem.h"
	#include "UnrealEdGlobals.h"
#endif

namespace ConcertSyncClientUtil
{

static TAutoConsoleVariable<int32> CVarDelayApplyingTransactionsWhileEditing(
	TEXT("Concert.DelayTransactionsWhileEditing"), 0,
	TEXT("Focus is lost by the editor when a transaction is applied. This variable suspends applying a transaction until the user has removed focus on editable UI."));

static TAutoConsoleVariable<int32> CVarDelayApplyingTransactionsWaitTimeout(
	TEXT("Concert.DelayTransactionsWhileEditingTimeout"), 5,
	TEXT("When Concert.DelayTransactionsWhileEditing is enabled we make sure the user has not been idle too long to prevent transactions from stacking up. The timeout value is specified in seconds."));

bool IsUserEditing()
{
	static FName SEditableTextType(TEXT("SEditableText"));
	static FName SMultiLineEditableTextType(TEXT("SMultiLineEditableText"));

#if WITH_EDITOR
	if (GUnrealEd)
	{
		auto IsUserEditingWidget = [] () {
			bool bIsEditing = false;
			FSlateApplication& App = FSlateApplication::Get();
			App.ForEachUser([&bIsEditing](FSlateUser& User) {
				TSharedPtr<SWidget> FocusedWidget = User.GetFocusedWidget();
				bool bTextWidgetHasFocus = FocusedWidget && (FocusedWidget->GetType() == SEditableTextType || FocusedWidget->GetType() == SMultiLineEditableTextType);
				bIsEditing |= bTextWidgetHasFocus;
			});
			return bIsEditing;
		};
		return GUnrealEd->IsUserInteracting() || IsUserEditingWidget();
	}
#endif

	return false;
}

bool ShouldDelayTransaction()
{
#if WITH_EDITOR
	if (CVarDelayApplyingTransactionsWhileEditing.GetValueOnAnyThread() > 0)
	{
		static FName SEditableTextType(TEXT("SEditableText"));
		static FName SMultiLineEditableTextType(TEXT("SMultiLineEditableText"));

		const bool bIsEditing = IsUserEditing();
		if (bIsEditing)
		{
			FSlateApplication& App = FSlateApplication::Get();
			double LastUpdateTime = App.GetLastUserInteractionTime();
			double Duration = App.GetCurrentTime() - LastUpdateTime;
			if (static_cast<int32>(Duration) > CVarDelayApplyingTransactionsWaitTimeout.GetValueOnAnyThread())
			{
				return false;
			}
		}
		return bIsEditing;
	}
#endif
	return false;
}

bool CanPerformBlockingAction(const bool bBlockDuringInteraction)
{
	// GUndo is a crude check to make sure that we don't try and apply other transactions while the local user is making a change
	const bool bIsInteracting = bBlockDuringInteraction && GUndo != nullptr;
	return !(bIsInteracting || GIsSavingPackage || IsGarbageCollecting());
}

void UpdatePendingKillState(UObject* InObj, const bool bIsPendingKill)
{
	const bool bWasPendingKill = !IsValid(InObj);
	if (bIsPendingKill == bWasPendingKill)
	{
		return;
	}

	if (bIsPendingKill)
	{
		bool bMarkAsGarbage = true;

		if (AActor* Actor = Cast<AActor>(InObj))
		{
			if (UWorld* ActorWorld = Actor->GetWorld())
			{
#if WITH_EDITOR
				if (GIsEditor)
				{
					bMarkAsGarbage = !ActorWorld->EditorDestroyActor(Actor, /*bShouldModifyLevel*/false);
				}
				else
#endif	// WITH_EDITOR
				{
					bMarkAsGarbage = !ActorWorld->DestroyActor(Actor, /*bNetForce*/false, /*bShouldModifyLevel*/false);
				}
			}
		}

		if (bMarkAsGarbage)
		{
			InObj->MarkAsGarbage();
		}
	}
	else
	{
		InObj->ClearGarbage();
	}
}

void AddActorToOwnerLevel(AActor* InActor)
{
	if (ULevel* Level = InActor->GetLevel())
	{
		if (Level->Actors.Contains(InActor))
		{
			return;
		}

		Level->Actors.Add(InActor);
		Level->ActorsForGC.Add(InActor);

#if WITH_EDITOR
		if (GIsEditor)
		{
			if (Level->IsUsingActorFolders())
			{
				InActor->FixupActorFolder();
			}

			GEngine->BroadcastLevelActorAdded(InActor);

			if (UWorld* World = Level->GetWorld())
			{
				World->BroadcastLevelsChanged();
			}
		}
#endif	// WITH_EDITOR
	}
}

bool ObjectIdsMatch(const FConcertObjectId& One, const FConcertObjectId& Two)
{
	return One.ObjectClassPathName == Two.ObjectClassPathName
		&& One.ObjectOuterPathName == Two.ObjectOuterPathName
		&& One.ObjectName == Two.ObjectName;
}

int32 GetObjectPathDepth(UObject* InObjToTest)
{
	int32 Depth = 0;
	for (UObject* Outer = InObjToTest; Outer; Outer = Outer->GetOuter())
	{
		++Depth;
	}
	return Depth;
}

FGetObjectResult GetObject(const FConcertObjectId& InObjectId, const FName InNewName, const FName InNewOuterPath, const FName InNewPackageName, const bool bAllowCreate)
{
	const bool bIsRename = !InNewName.IsNone();
	const bool bIsOuterChange = !InNewOuterPath.IsNone();
	const bool bIsPackageChange = !InNewPackageName.IsNone();

	const FName ObjectOuterPathToFind = InObjectId.ObjectOuterPathName;
	const FName ObjectOuterPathToCreate = bIsOuterChange ? InNewOuterPath : ObjectOuterPathToFind;

	const FName ObjectNameToFind = InObjectId.ObjectName;
	const FName ObjectNameToCreate = bIsRename ? InNewName : ObjectNameToFind;

	const FName ObjectPackageToAssign = bIsPackageChange ? InNewPackageName : InObjectId.ObjectExternalPackageName;

	auto FindOrLoadClass = [bAllowCreate](const FName InClassName) -> UClass*
	{
		const FString ClassNameStr = InClassName.ToString();

		return bAllowCreate
			? LoadObject<UClass>(nullptr, *ClassNameStr)
			: FindObject<UClass>(nullptr, *ClassNameStr);
	};

	auto AssignExternalPackage = [bIsPackageChange, &ObjectPackageToAssign, &ObjectNameToCreate](UObject* InObject)
	{
		if (bIsPackageChange)
		{
			if (ObjectPackageToAssign.IsNone())
			{
				InObject->SetExternalPackage(nullptr);
			}
			else
			// find the new package to assign to the object
			if (UPackage* NewPackage = FindObject<UPackage>(nullptr, *ObjectPackageToAssign.ToString()))
			{
				InObject->SetExternalPackage(NewPackage);
			}
			else
			{
				UE_LOG(LogConcert, Warning, TEXT("Package '%s' could not be found and assigned to Object '%s'."), *ObjectPackageToAssign.ToString(), *ObjectNameToCreate.ToString());
			}
		}
	};

	// Find the outer for the existing object.
	// Note that we use FSoftObjectPath::ResolveObject() here to ensure that if
	// world partitioning is involved, we're able to resolve a non-partitioned
	// path into an object with a partitioned path (e.g. an editor path to a
	// "-game"/nDisplay path).
	// TODO: If a case arises where we need to go the other direction and get
	// an object with a non-partitioned path from a partitioned path, a
	// different mechanism for that would be needed here.
	if (UObject* ExistingObjectOuter = FSoftObjectPath(ObjectOuterPathToFind.ToString()).ResolveObject())
	{
		// We need the object class to find or create the object
		if (UClass* ObjectClass = FindOrLoadClass(InObjectId.ObjectClassPathName))
		{
			// Find the existing object
			if (UObject* ExistingObject = StaticFindObject(ObjectClass, ExistingObjectOuter, *ObjectNameToFind.ToString(), /*bExactClass*/true))
			{
				EGetObjectResultFlags ResultFlags = EGetObjectResultFlags::None;

				// Perform any renames or outer changes
				if (bIsRename || bIsOuterChange)
				{
					UObject* NewObjectOuter = nullptr;
					if (bIsOuterChange)
					{
						//@todo FH: what if our new outer isn't loaded yet?
						NewObjectOuter = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectOuterPathToCreate.ToString());
					}

					// Find the new object (in case something already created it)
					if (UObject* NewObject = StaticFindObject(ObjectClass, NewObjectOuter ? NewObjectOuter : ExistingObjectOuter, *ObjectNameToCreate.ToString(), /*bExactClass*/true))
					{
						UE_LOG(LogConcert, Warning, TEXT("Attempted to rename '%s' over '%s'. Re-using the found object instead of performing the rename!"), *ExistingObject->GetPathName(), *NewObject->GetPathName());
						UpdatePendingKillState(ExistingObject, /*bIsPendingKill*/true);
						ResultFlags |= EGetObjectResultFlags::NeedsGC;

						ExistingObject = NewObject;
					}
					else
					{
						ExistingObject->Rename(*ObjectNameToCreate.ToString(), NewObjectOuter);
					}
				}

				// Update the object flags
				ExistingObject->SetFlags((EObjectFlags)InObjectId.ObjectPersistentFlags);

				// if we have any package assignment, do it here
				AssignExternalPackage(ExistingObject);

				// We found the object, return it
				return FGetObjectResult(ExistingObject, ResultFlags);
			}
		}
	}

	// Find the outer for the new object.
	// As above, we use FSoftObjectPath::ResolveObject() here to account for
	// the possibility of world partitioning.
	if (UObject* NewObjectOuter = FSoftObjectPath(ObjectOuterPathToCreate.ToString()).ResolveObject())
	{
		// We need the object class to find or create the object
		if (UClass* ObjectClass = FindOrLoadClass(InObjectId.ObjectClassPathName))
		{
			// Find the new object (in case something already created it)
			if (UObject* NewObject = StaticFindObject(ObjectClass, NewObjectOuter, *ObjectNameToCreate.ToString(), /*bExactClass*/true))
			{
				// Update the object flags
				NewObject->SetFlags((EObjectFlags)InObjectId.ObjectPersistentFlags);

				// if we have any package assignment, do it here
				AssignExternalPackage(NewObject);

				return FGetObjectResult(NewObject);
			}

			if (bAllowCreate)
			{
				FGetObjectResult ObjectResult;
				// Create the new object
				if (ObjectClass->IsChildOf<AActor>())
				{
					// Actors should go through SpawnActor where possible
					if (ULevel* OuterLevel = Cast<ULevel>(NewObjectOuter))
					{
						UWorld* OwnerWorld = OuterLevel->GetWorld();
						if (!OwnerWorld)
						{
							OwnerWorld = OuterLevel->GetTypedOuter<UWorld>();
						}

						if (OwnerWorld)
						{
							FActorSpawnParameters SpawnParams;
							SpawnParams.Name = ObjectNameToCreate;
							SpawnParams.OverrideLevel = OuterLevel;
							SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
							SpawnParams.bNoFail = true;
							SpawnParams.ObjectFlags = (EObjectFlags)InObjectId.ObjectPersistentFlags;
							ObjectResult = FGetObjectResult(OwnerWorld->SpawnActor<AActor>(ObjectClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams), EGetObjectResultFlags::NewlyCreated);
						}
						else
						{
							UE_LOG(LogConcert, Warning, TEXT("Actor '%s' could not find an owner World! This is unexpected and the Actor will be created via NewObject rather than SpawnActor."), *ObjectNameToCreate.ToString());
						}
					}
					else
					{
						UE_LOG(LogConcert, Warning, TEXT("Actor '%s' wasn't directly outered to a Level! This is unexpected and the Actor will be created via NewObject rather than SpawnActor."), *ObjectNameToCreate.ToString());
					}
				}
				else
				{
					ObjectResult = FGetObjectResult(NewObject<UObject>(NewObjectOuter, ObjectClass, *ObjectNameToCreate.ToString(), (EObjectFlags)InObjectId.ObjectPersistentFlags), EGetObjectResultFlags::NewlyCreated);
				}
				
				// if we have any package assignment, do it here
				AssignExternalPackage(ObjectResult.Obj);

				return ObjectResult;
			}
		}
	}

	return FGetObjectResult();
}

TArray<const FProperty*> GetExportedProperties(const UStruct* InStruct, const TArray<FName>& InPropertyNames, const bool InIncludeEditorOnlyData)
{
	TArray<const FProperty*> ExportedProperties;
	ExportedProperties.Reserve(InPropertyNames.Num());

	for (const FName& PropertyName : InPropertyNames)
	{
		if (const FProperty* Property = GetExportedProperty(InStruct, PropertyName, InIncludeEditorOnlyData))
		{
			ExportedProperties.Add(Property);
		}
	}

	return ExportedProperties;
}

const FProperty* GetExportedProperty(const UStruct* InStruct, const FName InPropertyName, const bool InIncludeEditorOnlyData)
{
	if (const FProperty* Property = FindFProperty<FProperty>(InStruct, InPropertyName))
	{
		if (ConcertSyncUtil::CanExportProperty(Property, InIncludeEditorOnlyData))
		{
			return Property;
		}
	}

	return nullptr;
}

void SerializeProperties(FConcertLocalIdentifierTable* InLocalIdentifierTable, const UObject* InObject, const TArray<const FProperty*>& InProperties, const bool InIncludeEditorOnlyData, TArray<FConcertSerializedPropertyData>& OutPropertyDatas)
{
	for (const FProperty* Property : InProperties)
	{
		FConcertSerializedPropertyData& PropertyData = OutPropertyDatas.AddDefaulted_GetRef();
		PropertyData.PropertyName = Property->GetFName();
		SerializeProperty(InLocalIdentifierTable, InObject, Property, InIncludeEditorOnlyData, PropertyData.SerializedData);
	}
}

void SerializeProperty(FConcertLocalIdentifierTable* InLocalIdentifierTable, const UObject* InObject, const FProperty* InProperty, const bool InIncludeEditorOnlyData, TArray<uint8>& OutSerializedData)
{
	bool bSkipAssets = false; // TODO: Handle asset updates

	FConcertSyncObjectWriter ObjectWriter(InLocalIdentifierTable, (UObject*)InObject, OutSerializedData, InIncludeEditorOnlyData, bSkipAssets);
	ObjectWriter.SerializeProperty(InProperty, InObject);
}

void SerializeObject(FConcertLocalIdentifierTable* InLocalIdentifierTable, const UObject* InObject, const TArray<const FProperty*>* InProperties, const bool InIncludeEditorOnlyData, TArray<uint8>& OutSerializedData)
{
	bool bSkipAssets = false; // TODO: Handle asset updates

	FConcertSyncObjectWriter ObjectWriter(InLocalIdentifierTable, (UObject*)InObject, OutSerializedData, InIncludeEditorOnlyData, bSkipAssets);
	ObjectWriter.SerializeObject(InObject, InProperties);
}


void FlushPackageLoading(const FName InPackageName)
{
	FlushPackageLoading(InPackageName.ToString());
}

void FlushPackageLoading(const FString& InPackageName, bool bForceBulkDataLoad)
{
	UPackage* ExistingPackage = FindPackage(nullptr, *InPackageName);
	if (ExistingPackage)
	{
		if (!ExistingPackage->IsFullyLoaded())
		{
			FlushAsyncLoading();
			ExistingPackage->FullyLoad();
		}

		if (bForceBulkDataLoad)
		{
			ResetLoaders(ExistingPackage);
		}
		else if (ExistingPackage->GetLinker())
		{
			ExistingPackage->GetLinker()->Detach();
		}
	}
}

#if WITH_EDITOR

FDirectoryWatcherModule& GetDirectoryWatcherModule()
{
	static const FName DirectoryWatcherModuleName = TEXT("DirectoryWatcher");
	return FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(DirectoryWatcherModuleName);
}

FDirectoryWatcherModule* GetDirectoryWatcherModuleIfLoaded()
{
	static const FName DirectoryWatcherModuleName = TEXT("DirectoryWatcher");
	if (FModuleManager::Get().IsModuleLoaded(DirectoryWatcherModuleName))
	{
		return &FModuleManager::GetModuleChecked<FDirectoryWatcherModule>(DirectoryWatcherModuleName);
	}
	return nullptr;
}

IDirectoryWatcher* GetDirectoryWatcher()
{
	FDirectoryWatcherModule& DirectoryWatcherModule = GetDirectoryWatcherModule();
	return DirectoryWatcherModule.Get();
}

IDirectoryWatcher* GetDirectoryWatcherIfLoaded()
{
	if (FDirectoryWatcherModule* DirectoryWatcherModule = GetDirectoryWatcherModuleIfLoaded())
	{
		return DirectoryWatcherModule->Get();
	}
	return nullptr;
}

#endif // WITH_EDITOR

void SynchronizeAssetRegistry()
{
#if WITH_EDITOR
	IDirectoryWatcher* DirectoryWatcher = GetDirectoryWatcherIfLoaded();
	if (!DirectoryWatcher)
	{
		return;
	}

	DirectoryWatcher->Tick(0.0f);
#endif // WITH_EDITOR
}

void HotReloadPackages(TArrayView<const FName> InPackageNames)
{
	if (InPackageNames.Num() == 0)
	{
		return;
	}

#if WITH_EDITOR
	// Flush loading and clean-up any temporary placeholder packages (due to a package previously being missing on disk)
	FlushAsyncLoading();
	{
		bool bRunGC = false;
		for (const FName& PackageName : InPackageNames)
		{
			bRunGC |= FLinkerLoad::RemoveKnownMissingPackage(PackageName);
		}
		if (bRunGC)
		{
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	}

	FlushRenderingCommands();

	// Find the packages in-memory to content hot-reload
	TArray<UPackage*> ExistingPackages;
	ExistingPackages.Reserve(InPackageNames.Num());
	for (const FName& PackageName : InPackageNames)
	{
		UPackage* ExistingPackage = FindPackage(nullptr, *PackageName.ToString());
		if (ExistingPackage)
		{
			if (ExistingPackage->HasAnyPackageFlags(PKG_NewlyCreated))
			{
				ExistingPackage->ClearPackageFlags(PKG_NewlyCreated);
			}
			ExistingPackages.Add(ExistingPackage);
		}
	}

	if (ExistingPackages.Num() > 0)
	{
		FText ErrorMessage;
		UPackageTools::ReloadPackages(ExistingPackages, ErrorMessage, GetDefault<UConcertSyncConfig>()->bInteractiveHotReload ? UPackageTools::EReloadPackagesInteractionMode::Interactive : UPackageTools::EReloadPackagesInteractionMode::AssumePositive);

		if (!ErrorMessage.IsEmpty())
		{
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		}
	}
#endif
}

void PurgePackages(TArrayView<const FName> InPackageNames)
{
	if (InPackageNames.Num() == 0)
	{
		return;
	}

#if WITH_EDITOR
	TArray<UObject*> ObjectsToPurge;
	auto CollectObjectToPurge = [&ObjectsToPurge](UObject* InObject)
	{
		if (InObject->IsAsset() && GIsEditor)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(InObject);
			GEditor->GetSelectedObjects()->Deselect(InObject);
		}
		ObjectsToPurge.Add(InObject);
	};

	// Get the current edited map package to check if its going to be purged.
	bool bEditedMapPurged = false;
	UWorld* CurrentWorld = GetCurrentWorld();
	UPackage* EditedMapPackage = CurrentWorld ? CurrentWorld->GetOutermost(): nullptr;

	// Collect any in-memory packages that should be purged and check if we are including the current map in the purge.
	for (const FName& PackageName : InPackageNames)
	{
		UPackage* ExistingPackage = FindPackage(nullptr, *PackageName.ToString());
		if (ExistingPackage)
		{
			// Prevent any message from the editor saying a package is not saved or doesn't exist on disk.
			ExistingPackage->SetDirtyFlag(false);

			CollectObjectToPurge(ExistingPackage);
			ForEachObjectWithPackage(ExistingPackage, [&CollectObjectToPurge](UObject* InObject)
			{
				CollectObjectToPurge(InObject);
				return true;
			});

			bEditedMapPurged |= EditedMapPackage == ExistingPackage;
		}
	}

	// Broadcast the eminent objects destruction (ex. tell BlueprintActionDatabase to release its reference(s) on Blueprint(s) right now)
	FEditorDelegates::OnAssetsPreDelete.Broadcast(ObjectsToPurge);

	// Mark objects as purgeable.
	for (UObject* Object : ObjectsToPurge)
	{
		if (Object->IsRooted())
		{
			Object->RemoveFromRoot();
		}
		Object->ClearFlags(RF_Public | RF_Standalone);
	}

	// TODO: Revisit force replacing reference, current implementation is too aggressive and causes instability
	// If we have any object that were made purgeable, null out their references so we can garbage collect
	//if (ObjectsToPurge.Num() > 0)
	//{
	//	ObjectTools::ForceReplaceReferences(nullptr, ObjectsToPurge);
	//
	//}

	// Check if the map being edited is going to be purged. (b/c it's being deleted)
	if (bEditedMapPurged)
	{
		// The world being edited was purged and cannot be saved anymore, even with 'Save Current As', replace it by something sensible.
		FString StartupMapPackage = GetDefault<UGameMapsSettings>()->EditorStartupMap.GetLongPackageName();
		if (FPackageName::DoesPackageExist(StartupMapPackage))
		{
			UEditorLoadingAndSavingUtils::NewMapFromTemplate(StartupMapPackage, /*bSaveExistingMap*/false); // Expected to run GC internally.
		}
		else
		{
			UEditorLoadingAndSavingUtils::NewBlankMap(/*bSaveExistingMap*/false); // Expected to run GC internally.
		}
	}
	// if we have object to purge but the map isn't one of them collect garbage (if we purged the map it has already been done)
	else if (ObjectsToPurge.Num() > 0)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
#endif // WITH_EDITOR
}

UWorld* GetCurrentWorld()
{
	UWorld* CurrentWorld = nullptr;
	if (GIsEditor)
	{
		CurrentWorld = GEditor->GetEditorWorldContext().World();
	}
	else if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
	{
		CurrentWorld = GameEngine->GetGameWorld();
	}
	return CurrentWorld;
}

void FillPackageInfo(UPackage* InPackage, UObject* InAsset, const EConcertPackageUpdateType InPackageUpdateType, FConcertPackageInfo& OutPackageInfo)
{
	UObject* Asset = InAsset ? InAsset : InPackage->FindAssetInPackage();
	OutPackageInfo.PackageName = InPackage->GetFName();
	OutPackageInfo.AssetClass = Asset ? Asset->GetClass()->GetPathName() : FString();
	OutPackageInfo.PackageFileExtension = Asset && Asset->IsA<UWorld>()? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
	OutPackageInfo.PackageUpdateType = InPackageUpdateType;
}

}
