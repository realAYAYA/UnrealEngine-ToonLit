// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVR/DMXMVRSceneActor.h"

#include "DMXMVRFixtureActorInterface.h"
#include "DMXRuntimeLog.h"
#include "Game/DMXComponent.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXImportGDTF.h"
#include "Library/DMXLibrary.h"
#include "MVR/DMXMVRFixtureActorLibrary.h"
#include "MVR/DMXMVRAssetUserData.h"
#include "MVR/Types/DMXMVRFixtureNode.h"

#include "DatasmithAssetUserData.h"
#include "EngineUtils.h"
#include "Algo/Find.h"
#include "Algo/RemoveIf.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "Editor.h"
#endif 


ADMXMVRSceneActor::ADMXMVRSceneActor()
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	FEditorDelegates::MapChange.AddUObject(this, &ADMXMVRSceneActor::OnMapChange);

	if (GEngine)
	{
		GEngine->OnLevelActorDeleted().AddUObject(this, &ADMXMVRSceneActor::OnActorDeleted);
	}

	if (GEditor)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.AddUObject(this, &ADMXMVRSceneActor::OnAssetPostImport);
	}
#endif // WITH_EDITOR

	MVRSceneRoot = CreateDefaultSubobject<USceneComponent>("MVRSceneRoot");
	SetRootComponent(MVRSceneRoot);
	AddInstanceComponent(MVRSceneRoot);
}

ADMXMVRSceneActor::~ADMXMVRSceneActor()
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	FEditorDelegates::MapChange.RemoveAll(this);

	if (GEngine)
	{
		GEngine->OnLevelActorDeleted().RemoveAll(this);
	}

	if (GEditor)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.RemoveAll(this);
	}
#endif // WITH_EDITOR
}

void ADMXMVRSceneActor::PostLoad()
{
	Super::PostLoad();
	EnsureMVRUUIDsForRelatedActors();
}

void ADMXMVRSceneActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

#if WITH_EDITOR
	// If the actor was created as a Datasmith Element, set the library from there
	const FString DMXLibraryPathString = UDatasmithAssetUserData::GetDatasmithUserDataValueForKey(this, TEXT("DMXLibraryPath"));
	if (!DMXLibraryPathString.IsEmpty() && !DMXLibrary)
	{
		const FSoftObjectPath DMXLibraryPath(DMXLibraryPathString);
		UObject* NewDMXLibraryObject = DMXLibraryPath.TryLoad();
		if (UDMXLibrary* NewDMXLibrary = Cast<UDMXLibrary>(NewDMXLibraryObject))
		{
			SetDMXLibrary(NewDMXLibrary);
		}
	}

	EnsureMVRUUIDsForRelatedActors();
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void ADMXMVRSceneActor::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(FDMXMVRSceneGDTFToActorClassPair, ActorClass))
	{
		GDTFToDefaultActorClasses_PreEditChange = GDTFToDefaultActorClasses;
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void ADMXMVRSceneActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(FDMXMVRSceneGDTFToActorClassPair, ActorClass))
	{
		HandleDefaultActorClassForGDTFChanged();
	}
}
#endif // WITH_EDITOR

UDMXEntityFixturePatch* ADMXMVRSceneActor::GetFixturePatchFromActor(AActor* Actor) const
{
	TArray<UDMXComponent*> DMXComponents;
	Actor->GetComponents<UDMXComponent>(DMXComponents);
	if (!ensureAlwaysMsgf(!DMXComponents.IsEmpty(), TEXT("'%s' implements the DMXMVRFixtureActorInterface, but has no DMX component. The DMX Component is needed to patch and identify the fixture in the MVR Scene."), *Actor->GetName()))
	{
		return nullptr;
	}
	ensureAlwaysMsgf(DMXComponents.Num() == 1, TEXT("'%s' implements the DMXMVRFixtureActorInterface, but has more than one DMX component. A single DMX component is required to clearly identify the fixture by MVR UUID in the MVR Scene."), *Actor->GetName());
	
	UDMXEntityFixturePatch* FixturePatch = DMXComponents[0]->GetFixturePatch();

	return FixturePatch;
}

void ADMXMVRSceneActor::SetFixturePatchOnActor(AActor* Actor, UDMXEntityFixturePatch* FixturePatch)
{
	if (!ensureMsgf(Actor && FixturePatch, TEXT("Trying to Set Fixture Patch on Actor, but Actor or Fixture Patch are invalid.")))
	{
		return;
	}

	TArray<UDMXComponent*> DMXComponents;
	Actor->GetComponents<UDMXComponent>(DMXComponents);
	if (!ensureAlwaysMsgf(!DMXComponents.IsEmpty(), TEXT("'%s' implements the DMXMVRFixtureActorInterface, but has no DMX component. The DMX Component is needed to patch and identify the fixture in the MVR Scene."), *Actor->GetName()))
	{
		return;
	}
	ensureAlwaysMsgf(DMXComponents.Num() == 1, TEXT("'%s' implements the DMXMVRFixtureActorInterface, but has more than one DMX component. A single DMX component is required to clearly identify the fixture by MVR UUID in the MVR Scene."), *Actor->GetName());

	DMXComponents[0]->SetFixturePatch(FixturePatch);
}


#if WITH_EDITOR
void ADMXMVRSceneActor::SetDMXLibrary(UDMXLibrary* NewDMXLibrary)
{
	if (!ensureAlwaysMsgf(!DMXLibrary, TEXT("Tried to set the DMXLibrary for %s, but it already has one set. Changing the library is not supported."), *GetName()))
	{
		return;
	}

	if (!NewDMXLibrary || NewDMXLibrary == DMXLibrary)
	{
		return;
	}
	DMXLibrary = NewDMXLibrary;

	RefreshFromDMXLibrary();
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void ADMXMVRSceneActor::RefreshFromDMXLibrary()
{
	if (!ensureMsgf(DMXLibrary, TEXT("Trying to update MVR Scene from DMX Library, but DMX Library was never set or no longer exists.")))
	{
		return;
	}

	DMXLibrary->UpdateGeneralSceneDescription();
	UDMXMVRGeneralSceneDescription* GeneralSceneDescription = DMXLibrary->GetLazyGeneralSceneDescription();
	if (!GeneralSceneDescription)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!ensureMsgf(World, TEXT("Invalid world when trying to update MVR Scene from DMX Library.")))
	{
		return;
	}

	if (bRespawnDeletedActorsOnRefresh)
	{
		DeletedMVRFixtureUUIDs.Reset();
	}

	const TSharedRef<FDMXMVRFixtureActorLibrary> MVRFixtureActorLibrary = MakeShared<FDMXMVRFixtureActorLibrary>();
	const TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

	// Destroy actors no longer present in the DMX Library, remember any other spawned actor
	TMap<UDMXEntityFixturePatch*, AActor*> FixturePatchToSpawnedActorMap;
	for (const TSoftObjectPtr<AActor>& SoftActorPtr : RelatedActors)
	{
		if (!SoftActorPtr.IsValid())
		{
			continue;
		}
		AActor* Actor = SoftActorPtr.Get();

		UDMXEntityFixturePatch* FixturePatch = GetFixturePatchFromActor(Actor);
		if (!FixturePatch || !FixturePatches.Contains(FixturePatch))
		{
			Actor->Destroy();
			continue;
		}

		FixturePatchToSpawnedActorMap.Add(FixturePatch, Actor);
	}

	// Spawn newly added and if requested, previously deleted actors
	for (UDMXEntityFixturePatch* FixturePatch : FixturePatches)
	{
		if (FixturePatchToSpawnedActorMap.Contains(FixturePatch))
		{
			continue;
		}

		if (!bRespawnDeletedActorsOnRefresh &&
			DeletedMVRFixtureUUIDs.Contains(FixturePatch->GetMVRFixtureUUID()))
		{
			continue;
		}

		UClass* ActorClass = MVRFixtureActorLibrary->FindMostAppropriateActorClassForPatch(FixturePatch);
		if (!ActorClass)
		{
			continue;
		}

		const FGuid& MVRFixtureUUID = FixturePatch->GetMVRFixtureUUID();
		const UDMXMVRFixtureNode* FixtureNode = GeneralSceneDescription->FindFixtureNode(MVRFixtureUUID);
		if (!FixtureNode)
		{
			continue;
		}

		const FTransform Transform = FixtureNode->GetTransformAbsolute();
		SpawnMVRActor(ActorClass, FixturePatch, Transform);
	}

	// Update Transforms if requested
	if (bUpdateTransformsOnRefresh)
	{
		for (const TSoftObjectPtr<AActor>& SoftRelatedActor : RelatedActors)
		{
			if (!SoftRelatedActor.IsValid())
			{
				continue;
			}

			AActor* RelatedActor = SoftRelatedActor.Get();
			UDMXEntityFixturePatch* FixturePatch = GetFixturePatchFromActor(RelatedActor);
			if (!FixturePatch)
			{
				continue;
			}

			const FGuid& MVRFixtureUUID = FixturePatch->GetMVRFixtureUUID();
			const UDMXMVRFixtureNode* FixtureNode = GeneralSceneDescription->FindFixtureNode(MVRFixtureUUID);
			if (!FixtureNode)
			{
				continue;
			}

			const FTransform Transform = FixtureNode->GetTransformAbsolute();
			RelatedActor->SetActorTransform(Transform);
		}
	}

	UpdateGDTFToDefaultActorClasses(MVRFixtureActorLibrary);
}
#endif // WITH_EDITOR

#if WITH_EDITOR
TArray<AActor*> ADMXMVRSceneActor::GetActorsSpawnedForGDTF(const UDMXImportGDTF* GDTF) const
{
	TArray<AActor*> Result;
	if (!GDTF)
	{
		return Result;
	}

	for (const TSoftObjectPtr<AActor>& SoftActorPtr : RelatedActors)
	{
		if (!SoftActorPtr.IsValid())
		{
			continue;
		}
		AActor* Actor = SoftActorPtr.Get();

		UDMXEntityFixturePatch* FixturePatch = GetFixturePatchFromActor(Actor);
		if (!FixturePatch || !FixturePatch->GetFixtureType())
		{
			continue;
		}

		if (FixturePatch->GetFixtureType()->DMXImport == GDTF)
		{
			Result.Add(Actor);
		}
	}

	return Result;
}
#endif // WITH_EDITOR

void ADMXMVRSceneActor::EnsureMVRUUIDsForRelatedActors()
{
	for (const TSoftObjectPtr<AActor>& RelatedActor : RelatedActors)
	{
		if (AActor* Actor = RelatedActor.Get())
		{
			const FString MVRFixtureUUID = UDMXMVRAssetUserData::GetMVRAssetUserDataValueForKey(*Actor, UDMXMVRAssetUserData::MVRFixtureUUIDMetaDataKey);
			if (MVRFixtureUUID.IsEmpty())
			{
				// Try to acquire the MVR Fixture UUID
				if (UDMXEntityFixturePatch* FixturePatch = GetFixturePatchFromActor(Actor))
				{
					UDMXMVRAssetUserData::SetMVRAssetUserDataValueForKey(*Actor, UDMXMVRAssetUserData::MVRFixtureUUIDMetaDataKey, FixturePatch->GetMVRFixtureUUID().ToString());
				}
			}
		}
	}
}

void ADMXMVRSceneActor::UpdateGDTFToDefaultActorClasses(const TSharedRef<FDMXMVRFixtureActorLibrary>& MVRFixtureActorLibrary)
{
	const TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	for (UDMXEntityFixturePatch* FixturePatch : FixturePatches)
	{
		const UDMXEntityFixtureType* FixtureType = FixturePatch->GetFixtureType();
		if (FixtureType && FixtureType->DMXImport)
		{
			const FDMXMVRSceneGDTFToActorClassPair* ExistingGDTFToActorClassPairPtr = Algo::FindByPredicate(GDTFToDefaultActorClasses, [FixtureType](const FDMXMVRSceneGDTFToActorClassPair& GDTFToActorClassPair)
				{
					return GDTFToActorClassPair.GDTF == FixtureType->DMXImport;
				});
			if (ExistingGDTFToActorClassPairPtr)
			{
				continue;
			}

			UClass* ActorClass = MVRFixtureActorLibrary->FindMostAppropriateActorClassForPatch(FixturePatch);
			FDMXMVRSceneGDTFToActorClassPair GDTFToActorClassPair;
			GDTFToActorClassPair.GDTF = FixtureType->DMXImport;
			GDTFToActorClassPair.ActorClass = ActorClass;

			GDTFToDefaultActorClasses.Add(GDTFToActorClassPair);
		}
	}

	// Remove those GDTFs that are no longer used by any actor
	GDTFToDefaultActorClasses.SetNum(Algo::RemoveIf(GDTFToDefaultActorClasses, [this](const FDMXMVRSceneGDTFToActorClassPair& GDTFToActorClassPair)
		{
			const TSoftObjectPtr<AActor>* ActorThatUsesGDTF = Algo::FindByPredicate(RelatedActors, [this, GDTFToActorClassPair](const TSoftObjectPtr<AActor>& Actor)
				{
					const UDMXEntityFixturePatch* FixturePatch = Actor.IsValid() ? GetFixturePatchFromActor(Actor.Get()) : nullptr;
					const UDMXEntityFixtureType* FixtureType = FixturePatch ? FixturePatch->GetFixtureType() : nullptr;
					const UDMXImport* GDTF = FixtureType ? FixtureType->DMXImport : nullptr;
					
					return GDTFToActorClassPair.GDTF == GDTF;
				});
			return ActorThatUsesGDTF == nullptr;
		}));
}

#if WITH_EDITOR
void ADMXMVRSceneActor::OnMapChange(uint32 MapEventFlags)
{
	// Whenever a sub-level is loaded, we need to apply the fix
	if (MapEventFlags == MapChangeEventFlags::NewMap)
	{
		EnsureMVRUUIDsForRelatedActors();
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void ADMXMVRSceneActor::OnActorDeleted(AActor* DeletedActor)
{
	if (DeletedActor == this)
	{
		for (const TSoftObjectPtr<AActor>& RelatedActor : RelatedActors)
		{
			if (AActor* Actor = RelatedActor.IsValid() ? RelatedActor.Get() : nullptr)
			{
				Actor->Modify();
				Actor->Destroy();
			}
		}
	}
	else
	{
		const int32 RelatedActorIndex = RelatedActors.Find(DeletedActor);
		if (RelatedActorIndex != INDEX_NONE)
		{
			// This will add this actor to the transaction if there is one currently recording
			Modify();

			const UDMXEntityFixturePatch* FixturePatch = GetFixturePatchFromActor(DeletedActor);
			if (FixturePatch)
			{
				DeletedMVRFixtureUUIDs.Add(FixturePatch->GetMVRFixtureUUID());
			}

			RelatedActors[RelatedActorIndex]->Reset();
		}
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void ADMXMVRSceneActor::OnAssetPostImport(UFactory* InFactory, UObject* ActorAdded)
{
	for (TObjectIterator<AActor> It; It; ++It)
	{
		AActor* Actor = *It;

		const int32 RelatedActorIndex = RelatedActors.Find(Actor);
		if (RelatedActorIndex != INDEX_NONE)
		{
			// This will add this actor to the transaction if there is one currently recording
			Modify();

			RelatedActors[RelatedActorIndex] = Actor;
		}
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void ADMXMVRSceneActor::HandleDefaultActorClassForGDTFChanged()
{
	// Handle element changes, but not add/remove
	if (GDTFToDefaultActorClasses_PreEditChange.Num() != GDTFToDefaultActorClasses.Num())
	{
		return;
	}

	int32 IndexOfChangedElement = INDEX_NONE;
	for (const FDMXMVRSceneGDTFToActorClassPair& GDTFToDefaultActorClassPair : GDTFToDefaultActorClasses)
	{
		IndexOfChangedElement = GDTFToDefaultActorClasses_PreEditChange.IndexOfByPredicate([&GDTFToDefaultActorClassPair](const FDMXMVRSceneGDTFToActorClassPair& GDTFToActorClassPair)
			{
				return
					GDTFToActorClassPair.GDTF == GDTFToDefaultActorClassPair.GDTF &&
					GDTFToActorClassPair.ActorClass != GDTFToDefaultActorClassPair.ActorClass;
			});

		if (IndexOfChangedElement != INDEX_NONE)
		{
			break;
		}
	}

	if (IndexOfChangedElement == INDEX_NONE)
	{
		return;
	}

	const TSubclassOf<AActor> Class = GDTFToDefaultActorClasses[IndexOfChangedElement].ActorClass.Get();
	if (!Class.Get())
	{
		return;
	}

	for (const TSoftObjectPtr<AActor>& RelatedActor : TArray<TSoftObjectPtr<AActor>>(RelatedActors))
	{
		AActor* Actor = RelatedActor.Get();
		if (!Actor)
		{
			continue;
		}

		UDMXEntityFixturePatch* FixturePatch = GetFixturePatchFromActor(Actor);
		if (FixturePatch && 
			FixturePatch->GetFixtureType() && 
			FixturePatch->GetFixtureType()->DMXImport == GDTFToDefaultActorClasses[IndexOfChangedElement].GDTF)
		{
			ReplaceMVRActor(Actor, Class);
		}
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
AActor* ADMXMVRSceneActor::SpawnMVRActor(const TSubclassOf<AActor>&ActorClass, UDMXEntityFixturePatch* FixturePatch, const FTransform& Transform, AActor* Template)
{
	UWorld* World = GetWorld();
	if (!ensureAlwaysMsgf(World, TEXT("Trying to spawn MVR Fixture in MVR Scene, but the world is not valid.")))
	{
		return nullptr;
	}

	if (!ensureAlwaysMsgf(FixturePatch, TEXT("Trying to spawn MVR Fixture in MVR Scene, but the Fixture Patch is not valid.")))
	{
		return nullptr;
	}

	if (!FixturePatch->GetFixtureType() || !FixturePatch->GetFixtureType()->DMXImport)
	{
		UE_LOG(LogDMXRuntime, Warning, TEXT("Cannot spawn fixture for Fixture Patch '%s'. It's Fixture Type does not source from a GDTF."), *FixturePatch->Name);
		return nullptr;
	}

	FName ActorName = *FixturePatch->Name;
	if (!IsGloballyUniqueObjectName(*FixturePatch->Name))
	{
		ActorName = MakeUniqueObjectName(World, AActor::StaticClass(), *FixturePatch->Name);
	}

	FActorSpawnParameters ActorSpawnParameters;
	ActorSpawnParameters.Template = Template;
	ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActorSpawnParameters.Name = ActorName;
	ActorSpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	AActor* NewFixtureActor = World->SpawnActor<AActor>(ActorClass, Transform, ActorSpawnParameters);
	if (!NewFixtureActor)
	{
		return nullptr;
	}
	NewFixtureActor->SetActorLabel(FixturePatch->Name);

	NewFixtureActor->RegisterAllComponents();
	USceneComponent* RootComponentOfChildActor = NewFixtureActor->GetRootComponent();
	if (!RootComponentOfChildActor)
	{
		UE_LOG(LogDMXRuntime, Warning, TEXT("Cannot spawn MVR Fixture Actor of Class %s, the Actor does not specifiy a root component."), *ActorClass->GetName());
		NewFixtureActor->Destroy();
		return nullptr;
	}

#if WITH_EDITOR
	// Create Property Change Events so editor objects related to the actor have a chance to update (e.g. Details, World Outliner).
	PreEditChange(ADMXMVRSceneActor::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(ADMXMVRSceneActor, RelatedActors)));
	NewFixtureActor->PreEditChange(nullptr);
#endif 

	// Attach, set MVR Fixture UUID, set Fixture Patch, remember as a Related Actor
	RootComponentOfChildActor->SetMobility(EComponentMobility::Movable);
	RootComponentOfChildActor->AttachToComponent(MVRSceneRoot, FAttachmentTransformRules::KeepWorldTransform);
	const FGuid& MVRFixtureUUID = FixturePatch->GetMVRFixtureUUID();
	UDMXMVRAssetUserData::SetMVRAssetUserDataValueForKey(*NewFixtureActor, UDMXMVRAssetUserData::MVRFixtureUUIDMetaDataKey, MVRFixtureUUID.ToString());
	SetFixturePatchOnActor(NewFixtureActor, FixturePatch);
	RelatedActors.Add(NewFixtureActor);

#if WITH_EDITOR
	PostEditChange();
	NewFixtureActor->PostEditChange();
#endif

	return NewFixtureActor;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
AActor* ADMXMVRSceneActor::ReplaceMVRActor(AActor* ActorToReplace, const TSubclassOf<AActor>& ClassOfNewActor)
{
	if (!ensureAlwaysMsgf(ActorToReplace, TEXT("Trying to replace MVR Fixture in MVR Scene, but the Actor to replace is not valid.")))
	{
		return nullptr;
	}
	
	if (ActorToReplace->GetClass() == ClassOfNewActor)
	{
		// No need to replace
		return nullptr;
	}

	const FString MVRFixtureUUIDString = UDMXMVRAssetUserData::GetMVRAssetUserDataValueForKey(*ActorToReplace, UDMXMVRAssetUserData::MVRFixtureUUIDMetaDataKey);
	FGuid MVRFixtureUUID;
	if (FGuid::Parse(MVRFixtureUUIDString, MVRFixtureUUID))
	{
		// Try to find a Fixture Patch in following order:
		// By the MVR Fixture Actor Interface, it may customize the getter
		// By a DMX Component present in the Actor, it might have overriden the patch
		// By MVR Fixture UUID in the DMX Library
		UDMXEntityFixturePatch* FixturePatch = nullptr;
		if (IDMXMVRFixtureActorInterface* MVRFixtureActorInterface = Cast<IDMXMVRFixtureActorInterface>(ActorToReplace))
		{
			FixturePatch = GetFixturePatchFromActor(ActorToReplace);
		}
		
		if (!FixturePatch)
		{
			if (UActorComponent* Component = ActorToReplace->GetComponentByClass(UDMXComponent::StaticClass()))
			{
				FixturePatch = CastChecked<UDMXComponent>(Component)->GetFixturePatch();
			}
		}

		if (!FixturePatch && DMXLibrary)
		{
			const TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
			UDMXEntityFixturePatch* const* FixturePatchPtr = FixturePatches.FindByPredicate([&MVRFixtureUUID](const UDMXEntityFixturePatch* FixturePatch)
				{
					return FixturePatch->GetMVRFixtureUUID() == MVRFixtureUUID;
				});

			if (FixturePatchPtr)
			{
				FixturePatch = *FixturePatchPtr;
			}
		}

		if (AActor* NewFixtureActor = SpawnMVRActor(ClassOfNewActor, FixturePatch, ActorToReplace->GetTransform()))
		{
			RelatedActors.Remove(ActorToReplace);
			ActorToReplace->Destroy();
			return NewFixtureActor;
		}
	}

	return nullptr;
}
#endif // WITH_EDITOR
