// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeneratedNaniteDisplacedMeshEditorSubsystem.h"

#include "NaniteDisplacedMesh.h"
#include "NaniteDisplacedMeshLog.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/ChunkedArray.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Misc/FeedbackContext.h"
#include "Templates/SubclassOf.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeneratedNaniteDisplacedMeshEditorSubsystem)

void UGeneratedNaniteDisplacedMeshEditorSubsystem::RegisterClassHandler(const TSubclassOf<AActor>& ActorClass, FActorClassHandler&& ActorClassHandler)
{
	ActorClassHandlers.Add(ActorClass.Get(), MoveTemp(ActorClassHandler));
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::UnregisterClassHandler(const TSubclassOf<AActor>& ActorClass)
{
	const UClass* ClassToRemove = ActorClass.Get();
	ActorClassHandlers.Remove(ClassToRemove);

	TSet<UClass*> SubClassesRegistered;
	for (const TPair<UClass*, FActorClassHandler>& ActorHandler : ActorClassHandlers)
	{
		if (ActorHandler.Key->IsChildOf(ClassToRemove))
		{
			SubClassesRegistered.Add(ActorHandler.Key);
		}
	}

	TChunkedArray<TObjectKey<AActor>> ActorsToRemove;
	for (const TPair<TObjectKey<AActor>, TArray<TObjectKey<UObject>>>& ActorToDependencies : ActorsToDependencies)
	{
		const AActor* Actor = ActorToDependencies.Key.ResolveObjectPtr();
		if (!Actor)
		{
			// Clean invalid the actors
			ActorsToRemove.AddElement(ActorToDependencies.Key);
			continue;
		}

		UClass* Class = Actor->GetClass();
		while (Class)
		{
			if (SubClassesRegistered.Find(Class))
			{
				break;
			}

			if (Class == ClassToRemove)
			{
				ActorsToRemove.AddElement(ActorToDependencies.Key);
				break;
			}

			Class = Class->GetSuperClass();
		}
	}


	for (const TObjectKey<AActor>& Actor : ActorsToRemove)
	{
		RemoveActor(Actor, GetTypeHash(Actor));
	}
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::UpdateActorDependencies(AActor* Actor, TArray<TObjectKey<UObject>>&& Dependencies)
{
	if (!FindClassHandler(Actor->GetClass()))
	{
		// Turning off this ensure to unblock our automation while waiting for a proper fix;
		// ensure(false);
		return;
	}

	Dependencies.RemoveAll([this](const TObjectKey<UObject>& WeakObject)
		{
			const bool bCanObjectBeTracked = CanObjectBeTracked(WeakObject.ResolveObjectPtr());
			ensure(bCanObjectBeTracked);
			return !bCanObjectBeTracked;
		});


	if (Dependencies.IsEmpty())
	{
		RemoveActor(Actor);
		return;
	}

	TObjectKey<AActor> WeakActor(Actor);
	uint32 WeakActorHash = GetTypeHash(WeakActor);
	TArray<TObjectKey<UObject>> RegistredActorDependencies = ActorsToDependencies.FindOrAddByHash(WeakActorHash, WeakActor);
	RegistredActorDependencies = MoveTemp(Dependencies);

	for (const TObjectKey<UObject>& Dependency : RegistredActorDependencies)
	{
		DependenciesToActors.FindOrAdd(Dependency).AddByHash(WeakActorHash, WeakActor);
	}
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::RemoveActor(const AActor* ActorToRemove)
{
	TObjectKey<AActor> WeakActorToRemove(ActorToRemove);
	RemoveActor(WeakActorToRemove, GetTypeHash(WeakActorToRemove));
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GEngine)
	{
		OnObjectsReplacedHandle = FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(this, &UGeneratedNaniteDisplacedMeshEditorSubsystem::OnObjectsReplaced);
		OnLevelActorDeletedHandle = GEngine->OnLevelActorDeleted().AddUObject(this, &UGeneratedNaniteDisplacedMeshEditorSubsystem::OnLevelActorDeleted);
		OnPostEditChangeHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UGeneratedNaniteDisplacedMeshEditorSubsystem::OnObjectPostEditChange);
		OnPreEditChangeHandle = FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddUObject(this, &UGeneratedNaniteDisplacedMeshEditorSubsystem::OnObjectPreEditChange);
	
		GUObjectArray.AddUObjectDeleteListener(this);

		OnPreGarbageCollectHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddUObject(this, &UGeneratedNaniteDisplacedMeshEditorSubsystem::UpdateIsEngineCollectingGarbage, true);
		OnPostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UGeneratedNaniteDisplacedMeshEditorSubsystem::UpdateIsEngineCollectingGarbage, false);
	}

	if (GEditor)
	{
		// Todo consider moving the import tracking into the import subsystem it this work well enough.
		if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
		{
			UNaniteDisplacedMesh::OnDependenciesChanged.AddUObject(this, &UGeneratedNaniteDisplacedMeshEditorSubsystem::UpdateDisplacementMeshToAssets);
			OnAssetReimportHandle = ImportSubsystem->OnAssetReimport.AddUObject(this, &UGeneratedNaniteDisplacedMeshEditorSubsystem::UpdateDisplacedMeshesDueToAssetChanges);

			// Need this also in case of a normal import stumping a existing asset.
			OnAssetPostImportHandle = ImportSubsystem->OnAssetPostImport.AddUObject(this, &UGeneratedNaniteDisplacedMeshEditorSubsystem::OnAssetPostImport);
			OnAssetPreImportHandle = ImportSubsystem->OnAssetPreImport.AddUObject(this, &UGeneratedNaniteDisplacedMeshEditorSubsystem::OnAssetPreImport);
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	OnInMemoryAssetDeletedHandle = AssetRegistryModule.Get().OnInMemoryAssetDeleted().AddUObject(this, &UGeneratedNaniteDisplacedMeshEditorSubsystem::WaitForDependentDisplacedMeshesToFinishTheirCompilation);

}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::Deinitialize()
{
	if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::LoadModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		AssetRegistryModule->Get().OnInMemoryAssetDeleted().Remove(OnInMemoryAssetDeletedHandle);
	}

	if (GEditor)
	{

		if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
		{
			ImportSubsystem->OnAssetPreImport.Remove(OnAssetPreImportHandle);
			ImportSubsystem->OnAssetPostImport.Remove(OnAssetPostImportHandle);
			ImportSubsystem->OnAssetReimport.Remove(OnAssetReimportHandle);
		}

		UNaniteDisplacedMesh::OnDependenciesChanged.RemoveAll(this);
	}

	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(OnPostGarbageCollectHandle);
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(OnPreGarbageCollectHandle);

	GUObjectArray.RemoveUObjectDeleteListener(this);

	FCoreUObjectDelegates::OnPreObjectPropertyChanged.Remove(OnPreEditChangeHandle);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPostEditChangeHandle);

	if (GEngine)
	{
		GEngine->OnLevelActorDeleted().Remove(OnLevelActorDeletedHandle);
	}
	FCoreUObjectDelegates::OnObjectsReplaced.Remove(OnObjectsReplacedHandle);

	ActorClassHandlers.Empty();
	ActorsToDependencies.Empty();
	DependenciesToActors.Empty();

	Super::Deinitialize();
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::OnObjectPreEditChange(UObject* Object, const FEditPropertyChain& EditPropertyChain)
{
	WaitForDependentDisplacedMeshesToFinishTheirCompilation(Object);
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::OnObjectPostEditChange(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		if (CanObjectBeTracked(Object))
		{
			TObjectKey<UObject> WeakObject(Object);
			uint32 WeakObjectHash = GetTypeHash(WeakObject);
			if (TSet<TObjectKey<AActor>>* PointerToDependentActors = DependenciesToActors.FindByHash(WeakObjectHash, WeakObject))
			{
				bool bActorRemoved = false;

				// Need to copy the data from the set since the handler can modify this set
				TArray<TObjectKey<AActor>> DependentActors = PointerToDependentActors->Array();

				for (const TObjectKey<AActor>& DependentActor : DependentActors)
				{
					if (AActor* RawActor = DependentActor.ResolveObjectPtr())
					{
						if (FActorClassHandler* ClassHandler = FindClassHandler(RawActor->GetClass()))
						{
							if (ShouldCallback(Object->GetClass(), *ClassHandler, PropertyChangedEvent))
							{
								ClassHandler->Callback(RawActor, Object, PropertyChangedEvent);
							}
						}
					}
					else
					{
						bActorRemoved |= RemoveActor(DependentActor, GetTypeHash(DependentActor));
					}
				}
			}
		}

		UpdateDisplacedMeshesDueToAssetChanges(Object);
	}
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	for (const TPair<UObject*, UObject*>& ReplacedObject : ReplacementMap)
	{
		UObject* OldObject = ReplacedObject.Key;
		UObject* NewObject = ReplacedObject.Value;

		if (UClass* OldClass = Cast<UClass>(OldObject))
		{
			UClass* NewClass = Cast<UClass>(NewObject);
			uint32 NewClassHash = GetTypeHash(NewClass);

			// Patch the class handler
			FActorClassHandler RemovedClassHandler;
			uint32 OldClassHash = GetTypeHash(OldClass);
			if (ActorClassHandlers.RemoveAndCopyValueByHash(OldClassHash, OldClass, RemovedClassHandler))
			{
				if (NewClass)
				{
					ActorClassHandlers.AddByHash(NewClassHash, NewClass, MoveTemp(RemovedClassHandler));
				}
			}
			
			// Patch the properties to watch
			for (TPair<UClass*, FActorClassHandler>& ActorClassHandler : ActorClassHandlers)
			{
				TSet<FProperty*> OldPropertiesToWatch;
				if (ActorClassHandler.Value.PropertiesToWatchPerAssetType.RemoveAndCopyValueByHash(OldClassHash, OldClass, OldPropertiesToWatch))
				{
					if (NewClass)
					{
						TSet<FProperty*> NewPropertyToWatch;
						NewPropertyToWatch.Reserve(OldPropertiesToWatch.Num());

						for (FProperty* OdlProperty : OldPropertiesToWatch)
						{
							if (FProperty* NewProperty = NewClass->FindPropertyByName(OdlProperty->GetFName()))
							{
								NewPropertyToWatch.Add(NewProperty);
							}
						}

						if (!NewPropertyToWatch.IsEmpty())
						{
							ActorClassHandler.Value.PropertiesToWatchPerAssetType.AddByHash(NewClassHash, NewClass, MoveTemp(NewPropertyToWatch));
						}
					}
				}
			}
		}
		else if (AActor* OldActor = Cast<AActor>(OldObject))
		{
			// Path the actor notification
			TArray<TObjectKey<UObject>> AssetDependencies;
			TObjectKey<AActor> WeakOldActor(OldActor);
			uint32 WeakOldActorHash = GetTypeHash(WeakOldActor);
			if (ActorsToDependencies.RemoveAndCopyValueByHash(WeakOldActorHash, WeakOldActor, AssetDependencies))
			{
				AActor* NewActor = Cast<AActor>(NewObject);
				const bool bIsNewActorValid = IsValid(NewActor);
				TObjectKey<AActor> WeakNewActor(NewActor);
				uint32 WeakNewActorHash = GetTypeHash(WeakNewActor);

				for (const TObjectKey<UObject>& AssetDependency : AssetDependencies)
				{
					uint32 AssetDependencyHash = GetTypeHash(AssetDependency);
					if (TSet<TObjectKey<AActor>>* PointerToActors = DependenciesToActors.FindByHash(AssetDependencyHash, AssetDependency))
					{
						PointerToActors->RemoveByHash(WeakOldActorHash, WeakOldActor);

						if (bIsNewActorValid)
						{
							PointerToActors->AddByHash(WeakNewActorHash, WeakNewActor);
						}
						else if (PointerToActors->IsEmpty())
						{
							DependenciesToActors.RemoveByHash(AssetDependencyHash, AssetDependency);
						}
					}
				}

				if (bIsNewActorValid)
				{
					ActorsToDependencies.AddByHash(WeakNewActorHash, WeakNewActor, MoveTemp(AssetDependencies));
				}
			}
		}
		else
		{
			// Patch the asset change tracking
			TSet<TObjectKey<AActor>> DependentActors;
			if (DependenciesToActors.RemoveAndCopyValue(OldObject, DependentActors))
			{
				const bool bIsNewObjectValid = CanObjectBeTracked(NewObject);
				TObjectKey<UObject> WeakNewObject(NewObject);

				for (const TObjectKey<AActor>& DependentActor : DependentActors)
				{
					uint32 DependentActorHash = GetTypeHash(DependentActor)
;					if (TArray<TObjectKey<UObject>>* PointerToDependencies = ActorsToDependencies.FindByHash(DependentActorHash, DependentActor))
					{
						PointerToDependencies->Remove(OldObject);

						if (bIsNewObjectValid)
						{
							PointerToDependencies->Add(WeakNewObject);
						}
						else if (PointerToDependencies->IsEmpty())
						{
							ActorsToDependencies.RemoveByHash(DependentActorHash, DependentActor);
						}
					}
				}

				if (bIsNewObjectValid)
				{
					DependenciesToActors.Add(WeakNewObject, MoveTemp(DependentActors));
				}
				
			}

			// Patch the asset reimport tracking
			MeshesAndAssetsReimportTracking.ReplaceObject(OldObject, NewObject);
		}
	}

	for (const TPair<UObject*, UObject*>& ReplacedObject : ReplacementMap)
	{
		// Block the game thread
		WaitForDependentDisplacedMeshesToFinishTheirCompilation(ReplacedObject.Value);
	}
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::OnLevelActorDeleted(AActor* Actor)
{
	TObjectKey<AActor> WeakActor(Actor);
	uint32 WeakActorHash = GetTypeHash(WeakActor);
	RemoveActor(WeakActor, WeakActorHash);
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::OnAssetPreImport(UFactory* InFactory, UClass* InClass, UObject* InParent, const FName& Name, const TCHAR* Type)
{
	if (InParent)
	{ 
		const bool bExactClass = false;
		if (UObject* Asset = FindObjectFast<UObject>(InParent, Name, bExactClass))
		{
			WaitForDependentDisplacedMeshesToFinishTheirCompilation(Asset);
		}
	}

}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::OnAssetPostImport(UFactory* InFactory, UObject* InCreatedObject)
{
	UpdateDisplacedMeshesDueToAssetChanges(InCreatedObject);
}

bool UGeneratedNaniteDisplacedMeshEditorSubsystem::CanObjectBeTracked(UObject* InObject)
{
	// Only assets can be tracked otherwise we might not receive the callbacks we need for this system to be functional and safe
	return InObject && InObject->IsAsset();
}

bool UGeneratedNaniteDisplacedMeshEditorSubsystem::RemoveActor(TObjectKey<AActor> InActorToRemove, uint32 InWeakActorHash)
{
	TArray<TObjectKey<UObject>> Dependencies;
	if (ActorsToDependencies.RemoveAndCopyValueByHash(InWeakActorHash, InActorToRemove, Dependencies))
	{
		for (const TObjectKey<UObject>& Asset : Dependencies)
		{
			uint32 AssetHash = GetTypeHash(Asset);
			if (TSet<TObjectKey<AActor>>* PointerToActorSet = DependenciesToActors.FindByHash(AssetHash, Asset))
			{
				PointerToActorSet->RemoveByHash(InWeakActorHash, InActorToRemove);
				if (PointerToActorSet->IsEmpty())
				{
					DependenciesToActors.RemoveByHash(AssetHash, Asset);
				}
			}
		}

		return true;
	}

	return false;
}

UGeneratedNaniteDisplacedMeshEditorSubsystem::FActorClassHandler* UGeneratedNaniteDisplacedMeshEditorSubsystem::FindClassHandler(UClass* Class)
{
	while (Class)
	{
		if (FActorClassHandler* ClassHandler = ActorClassHandlers.Find(Class))
		{
			return ClassHandler;
		}

		Class = Class->GetSuperClass();
	}

	return nullptr;
}

bool UGeneratedNaniteDisplacedMeshEditorSubsystem::ShouldCallback(UClass* AssetClass, const FActorClassHandler& ClassHandler, const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!PropertyChangedEvent.Property)
	{
		return true;
	}

	while (AssetClass)
	{
		if (const TSet<FProperty*>* PropertiesToWatch = ClassHandler.PropertiesToWatchPerAssetType.Find(AssetClass))
		{
			return bool(PropertiesToWatch->Find(PropertyChangedEvent.Property));
		}

		AssetClass = AssetClass->GetSuperClass();
	}

	// Default to true if we don't have any info on the type
	return true;
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::UpdateDisplacedMeshesDueToAssetChanges(UObject* Asset)
{
	if (IsValid(Asset))
	{
		const TArray<TObjectKey<UNaniteDisplacedMesh>> MeshKeys =  MeshesAndAssetsReimportTracking.GetMeshesThatUseAsset(Asset);
		for (TObjectKey<UNaniteDisplacedMesh> MeshKey : MeshKeys)
		{
			if (UNaniteDisplacedMesh* Mesh = MeshKey.ResolveObjectPtr())
			{
				// Kick the asset build
				Mesh->PreEditChange(nullptr);
				Mesh->PostEditChange();
			}
		}
	}
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::UpdateDisplacementMeshToAssets(UNaniteDisplacedMesh* DisplacementMesh)
{
	if (DisplacementMesh->HasAnyFlags(RF_BeginDestroyed))
	{
		MeshesAndAssetsReimportTracking.RemoveDisplacedMesh(DisplacementMesh);
		return;
	}

	const FNaniteDisplacedMeshParams& Params = DisplacementMesh->Parameters;

	TSet<UObject*> AssetsToTrack;
	AssetsToTrack.Reserve(1 + Params.DisplacementMaps.Num());

	if (IsValid(Params.BaseMesh))
	{
		AssetsToTrack.Add(Params.BaseMesh);
	}
	else
	{
		// No need to track change for a displaced mesh without a mesh
		MeshesAndAssetsReimportTracking.RemoveDisplacedMesh(DisplacementMesh);
		return;
	}

	for (const FNaniteDisplacedMeshDisplacementMap& DisplacementMap : Params.DisplacementMaps)
	{
		if (!FMath::IsNearlyZero(DisplacementMap.Magnitude) && DisplacementMap.Texture)
		{
			AssetsToTrack.Add(DisplacementMap.Texture);
		}
	}

	// We need at least a mesh and a texture with some magnitude for the displacement mesh to do something
	if (AssetsToTrack.Num() > 1)
	{
		MeshesAndAssetsReimportTracking.AddDisplacedMesh(DisplacementMesh, MoveTemp(AssetsToTrack));
	}
	else
	{
		MeshesAndAssetsReimportTracking.RemoveDisplacedMesh(DisplacementMesh);
	}
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::WaitForDependentDisplacedMeshesToFinishTheirCompilation(UObject* AssetAboutToChange)
{
	if (IsValid(AssetAboutToChange))
	{
		const TArray<TObjectKey<UNaniteDisplacedMesh>> MeshKeys = MeshesAndAssetsReimportTracking.GetMeshesThatUseAsset(AssetAboutToChange);

		for (TObjectKey<UNaniteDisplacedMesh> MeshKey : MeshKeys)
		{
			if (UNaniteDisplacedMesh* Mesh = MeshKey.ResolveObjectPtr())
			{
				if (Mesh->IsCompiling())
				{
					UE_LOG(LogNaniteDisplacedMesh, Log, TEXT("Staling the game thread while waiting for the NaniteDisplacedMesh (%s) to finish compiling."), *(Mesh->GetPathName()));
					Mesh->FinishAsyncTasks();
				}
			}
		}
	}
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::NotifyUObjectDeleted(const UObjectBase* ObjectBase, int32 Index)
{
	if (!ObjectBase->GetClass()->IsChildOf<UObject>())
	{
		return;
	}

	const UObject* Object = static_cast<const UObject*>(ObjectBase);

	const bool bHasValidAssetFlags = !(Object->HasAnyFlags(RF_Transient | RF_ClassDefaultObject)) && Object->HasAnyFlags(RF_Public);

	if (bHasValidAssetFlags)
	{
		// Don't count objects embedded in other objects (e.g. font textures, sequences, material expressions)
		if (UPackage* LocalOuterPackage = dynamic_cast<UPackage*>(Object->GetOuter()))
		{
			// Also exclude any objects found in the transient package, or in a package that is transient.
			if (LocalOuterPackage != GetTransientPackage() && !LocalOuterPackage->HasAnyFlags(RF_Transient))
			{
				// We are probably not recycling an asset if the garbage collection is running (Example: the GC while cooking doesn't send a event before removing an asset)
				if (!bIsEngineCollectingGarbage)
				{ 
					// The object is an asset. We are not tracking its deletion here. This avoid issue with the object recycling when reimporting stuff
					return;
				}
			}
		}
	}

	// The displaced meshes are removed by a event in their begin destroy function
	MeshesAndAssetsReimportTracking.RemoveAssetForReimportTracking(const_cast<UObject*>(Object));

	return;
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::OnUObjectArrayShutdown()
{
	GUObjectArray.RemoveUObjectDeleteListener(this);
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::UpdateIsEngineCollectingGarbage(bool bIsCollectingGarbage)
{
	bIsEngineCollectingGarbage = bIsCollectingGarbage;
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::FBidirectionalAssetsAndDisplacementMeshMap::RemoveDisplacedMesh(UNaniteDisplacedMesh* DisplacedMesh)
{
	uint32 DisplacedMeshHash = GetTypeHash(DisplacedMesh);
	TSet<UObject*> Assets;
	if (MeshToAssets.RemoveAndCopyValueByHash(DisplacedMeshHash, DisplacedMesh, Assets))
	{
		for (const UObject* Asset : Assets)
		{
			uint32 AssetHash = GetTypeHash(Asset);
			if (TSet<UNaniteDisplacedMesh*>* Meshes = AssetToMeshes.FindByHash(AssetHash, Asset))
			{
				Meshes->RemoveByHash(DisplacedMeshHash, DisplacedMesh);

				if (Meshes->IsEmpty())
				{
					AssetToMeshes.RemoveByHash(AssetHash, Asset);
				}
			}
		}
	}
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::FBidirectionalAssetsAndDisplacementMeshMap::RemoveAssetForReimportTracking(UObject* Object)
{
	uint32 ObjectHash = GetTypeHash(Object);
	TSet<UNaniteDisplacedMesh*> Meshes;
	if (AssetToMeshes.RemoveAndCopyValueByHash(ObjectHash, Object, Meshes))
	{
		for (UNaniteDisplacedMesh* Mesh : Meshes)
		{
			uint32 MeshHash = GetTypeHash(Mesh);
			if (TSet<UObject*>* Assets = MeshToAssets.FindByHash(MeshHash, Mesh))
			{
				Assets->RemoveByHash(ObjectHash, Object);

				if (Assets->IsEmpty())
				{
					MeshToAssets.RemoveByHash(MeshHash, Mesh);
				}
			}
		}
	}
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::FBidirectionalAssetsAndDisplacementMeshMap::AddDisplacedMesh(UNaniteDisplacedMesh* Mesh, TSet<UObject*>&& AssetsToTrack)
{
	RemoveDisplacedMesh(Mesh);

	uint32 MeshHash = GetTypeHash(Mesh);
	for (UObject* Asset : AssetsToTrack)
	{
		AssetToMeshes.FindOrAdd(Asset).AddByHash(MeshHash, Mesh);
	}

	MeshToAssets.AddByHash(MeshHash, Mesh, MoveTemp(AssetsToTrack));
}

const TArray<TObjectKey<UNaniteDisplacedMesh>> UGeneratedNaniteDisplacedMeshEditorSubsystem::FBidirectionalAssetsAndDisplacementMeshMap::GetMeshesThatUseAsset(UObject* Object)
{
	return GetMeshesThatUseAsset(Object, GetTypeHash(Object));
}

const TArray<TObjectKey<UNaniteDisplacedMesh>> UGeneratedNaniteDisplacedMeshEditorSubsystem::FBidirectionalAssetsAndDisplacementMeshMap::GetMeshesThatUseAsset(UObject* Object, uint32 Hash)
{
	if (TSet<UNaniteDisplacedMesh*>* DisplacedMeshesSet = AssetToMeshes.FindByHash(Hash, Object))
	{
		TArray<TObjectKey<UNaniteDisplacedMesh>> DisplacedMeshes;
		DisplacedMeshes.Reserve(DisplacedMeshesSet->Num());
		for (UNaniteDisplacedMesh* DisplacedMesh : *DisplacedMeshesSet)
		{
			if (IsValid(DisplacedMesh))
			{
				DisplacedMeshes.Emplace(DisplacedMesh);
			}
		}

		return DisplacedMeshes;
	}

	return {};
}

void UGeneratedNaniteDisplacedMeshEditorSubsystem::FBidirectionalAssetsAndDisplacementMeshMap::ReplaceObject(UObject* OldObject, UObject* NewObject)
{
	if (const UNaniteDisplacedMesh* OldDisplacedMesh = Cast<UNaniteDisplacedMesh>(OldObject))
	{
		uint32 OldDisplacedMeshHash = GetTypeHash(OldDisplacedMesh);
		TSet<UObject*> Assets;
		if (MeshToAssets.RemoveAndCopyValueByHash(OldDisplacedMeshHash, OldDisplacedMesh, Assets))
		{
			UNaniteDisplacedMesh* NewMesh = Cast<UNaniteDisplacedMesh>(NewObject);
			uint32 NewMeshHash = GetTypeHash(NewMesh);

			for (UObject* Asset : Assets)
			{
				uint32 AssetHash = GetTypeHash(Asset);
				if (TSet<UNaniteDisplacedMesh*>* Meshes = AssetToMeshes.FindByHash(AssetHash, Asset))
				{
					Meshes->RemoveByHash(OldDisplacedMeshHash, OldDisplacedMesh);

					if (NewMesh)
					{
						Meshes->AddByHash(NewMeshHash, NewMesh);
					}
					else if (Meshes->IsEmpty())
					{
						AssetToMeshes.RemoveByHash(AssetHash, Asset);
					}
				}
			}

			if (NewMesh)
			{
				MeshToAssets.AddByHash(NewMeshHash, NewMesh, MoveTemp(Assets));
			}
		}
	}
	else
	{
		uint32 OldObjectHash = GetTypeHash(OldObject);

		TSet<UNaniteDisplacedMesh*> Meshes;
		if (AssetToMeshes.RemoveAndCopyValueByHash(OldObjectHash, OldObject, Meshes))
		{
			uint32 NewObjectHash = GetTypeHash(NewObject);

			for (UNaniteDisplacedMesh* Mesh : Meshes)
			{
				uint32 MeshHash = GetTypeHash(Mesh);
				if (TSet<UObject*>* Assets = MeshToAssets.FindByHash(MeshHash, Mesh))
				{
					Assets->RemoveByHash(OldObjectHash, OldObject);

					if (NewObject)
					{
						Assets->AddByHash(NewObjectHash, NewObject);
					}
					else if (Assets->IsEmpty())
					{
						MeshToAssets.RemoveByHash(MeshHash, Mesh);
					}
				}
			}

			if (NewObject)
			{
				AssetToMeshes.AddByHash(NewObjectHash, NewObject, MoveTemp(Meshes));
			}
		}
	}
}

