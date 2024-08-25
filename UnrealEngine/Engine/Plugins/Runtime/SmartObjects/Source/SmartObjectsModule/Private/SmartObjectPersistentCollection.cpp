// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectPersistentCollection.h"

#include "GameplayTagAssetInterface.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "VisualLogger/VisualLogger.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "SmartObjectContainerRenderingComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectPersistentCollection)

namespace UE::SmartObject
{
	struct FEntryFinder
	{
		FEntryFinder(const FSmartObjectHandle& InHandle) : Handle(InHandle)
		{}

		bool operator()(const FSmartObjectCollectionEntry& ExistingEntry) const
		{
			return ExistingEntry.GetHandle() == Handle;
		}

		const FSmartObjectHandle Handle;
	};
}

//----------------------------------------------------------------------//
// FSmartObjectCollectionEntry 
//----------------------------------------------------------------------//
FSmartObjectCollectionEntry::FSmartObjectCollectionEntry(const FSmartObjectHandle SmartObjectHandle, const USmartObjectComponent& SmartObjectComponent, const uint32 DefinitionIndex)
	: Path(&SmartObjectComponent)
	, Transform(SmartObjectComponent.GetComponentTransform())
	, Bounds(SmartObjectComponent.GetSmartObjectBounds())
	, Handle(SmartObjectHandle)
	, DefinitionIdx(DefinitionIndex)
{
	if (const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(SmartObjectComponent.GetOwner()))
	{
		TagInterface->GetOwnedGameplayTags(Tags);
	}
}

USmartObjectComponent* FSmartObjectCollectionEntry::GetComponent() const
{
	return CastChecked<USmartObjectComponent>(Path.ResolveObject(), ECastCheckedType::NullAllowed);
}

//----------------------------------------------------------------------//
// FSmartObjectContainer
//----------------------------------------------------------------------//
void FSmartObjectContainer::Append(const FSmartObjectContainer& Other)
{
	if (Other.IsEmpty())
	{
		// nothing to do here
		return;
	}

	Bounds += Other.Bounds;

	// append definitions and create a mapping
	TArray<int32> DefinitionsMapping;
	DefinitionsMapping.Reserve(Other.Definitions.Num());
	for (const TObjectPtr<const USmartObjectDefinition>& SODefinition : Other.Definitions)
	{
		DefinitionsMapping.Add(Definitions.AddUnique(SODefinition));
	}	

	for (const FSmartObjectCollectionEntry& Entry : Other.CollectionEntries)
	{
		FSmartObjectCollectionEntry& NewEntry = CollectionEntries.Add_GetRef(Entry);
		// remap the definition index
		NewEntry.DefinitionIdx = DefinitionsMapping[Entry.GetDefinitionIndex()];
	}

	RegisteredIdToObjectMap.Append(Other.RegisteredIdToObjectMap);
}

int32 FSmartObjectContainer::Remove(const FSmartObjectContainer& Other)
{
	if (Other.IsEmpty())
	{
		// nothing to do here
		return 0;
	}

	int32 EntriesRemovedCount = 0;

	for (int32 InputIndex = 0; InputIndex < Other.CollectionEntries.Num();)
	{
		const FSmartObjectCollectionEntry& Entry = Other.CollectionEntries[InputIndex];

		const int32 LocalIndex = CollectionEntries.IndexOfByPredicate([Handle = Entry.GetHandle()](const FSmartObjectCollectionEntry& Element) 
			{
				return Element.GetHandle() == Handle;
			});

		// found something
		if (LocalIndex != INDEX_NONE)
		{
			RegisteredIdToObjectMap.Remove(Entry.GetHandle());

			// check if there's a sequence of matching entries - in case 'Other' represents a container 
			// that has been appended in the past
			int32 NumMatchingSequentialEntries = 1;

			for (int32 NextLocalIndex = LocalIndex + 1, NextInputIndex = InputIndex + 1
				; (NextLocalIndex < CollectionEntries.Num()) && (NextInputIndex < Other.CollectionEntries.Num())
				; ++NextLocalIndex, ++NextInputIndex)
			{
				const FSmartObjectCollectionEntry& AnotherLocalEntry = CollectionEntries[NextLocalIndex];
				const FSmartObjectCollectionEntry& AnotherInputEntry = Other.CollectionEntries[NextInputIndex];
				if (AnotherLocalEntry.GetHandle() != AnotherInputEntry.GetHandle())
				{
					break;
				}
				RegisteredIdToObjectMap.Remove(AnotherInputEntry.GetHandle());
				++NumMatchingSequentialEntries;
			}

			// not using *Swap flavor to maintain the order of appended entries in case we remove whole batches 
			CollectionEntries.RemoveAt(LocalIndex, NumMatchingSequentialEntries, EAllowShrinking::No);
			EntriesRemovedCount += NumMatchingSequentialEntries;
			InputIndex += NumMatchingSequentialEntries;
		}
		else
		{
			++InputIndex;
		}
	}

	// if anything removed we need to update the bounds
	if (EntriesRemovedCount)
	{
		Bounds = FBox(ForceInitToZero);

		for (const FSmartObjectCollectionEntry& Entry : CollectionEntries)
		{
			Bounds += Entry.GetBounds();
		}
	}

	return EntriesRemovedCount;
}

uint32 GetTypeHash(const FSmartObjectContainer& Container)
{
	// Note; the flaw of this hashing function is that the value depends on the specific order of 
	// entries, i.e. permutations of order result in different values. 
	uint32 Hash = HashCombine(GetTypeHash(Container.Bounds.Min), GetTypeHash(Container.Bounds.Max));
	
	TArray<uint32> DefinitionHashes;
	DefinitionHashes.AddZeroed(Container.Definitions.Num());
	for (int32 DefIndex = 0; DefIndex < DefinitionHashes.Num(); ++DefIndex)
	{ 
		if (!Container.Definitions[DefIndex])
		{
			continue;
		}

		const FSoftObjectPath ObjectPath = Container.Definitions[DefIndex];
		const FString AssetPathString = ObjectPath.GetAssetPathString();
		DefinitionHashes[DefIndex] = GetTypeHash(AssetPathString);
	}

	for (const FSmartObjectCollectionEntry& Entry : Container.CollectionEntries)
	{
		const int32 DefIndex = Entry.GetDefinitionIndex();
		if (DefinitionHashes.IsValidIndex(DefIndex))
		{
			uint32 EntryHash = HashCombine(GetTypeHash(Entry.GetHandle()), DefinitionHashes[DefIndex]);
			Hash = HashCombine(Hash, EntryHash);
		}
	}

	return Hash;
}

FSmartObjectCollectionEntry* FSmartObjectContainer::AddSmartObject(USmartObjectComponent& SOComponent, bool& bOutAlreadyInCollection)
{
	// marking as `false` until an actual entry is found. 
	bOutAlreadyInCollection = false;

	const UWorld* World = Owner ? Owner->GetWorld() : (const UWorld*)nullptr;
	if (World == nullptr)
	{
		UE_VLOG_UELOG(Owner, LogSmartObject, Error, TEXT("'%s' can't be registered to collection '%s': no associated world")
			, *SOComponent.GetPathName(SOComponent.GetOwner()), *GetPathNameSafe(Owner));
		return nullptr;
	}
	
	if (SOComponent.GetRegisteredHandle().IsValid())
	{
		FSmartObjectCollectionEntry* Entry = CollectionEntries.FindByPredicate(UE::SmartObject::FEntryFinder(SOComponent.GetRegisteredHandle()));
		
		UE_CVLOG_UELOG(Entry == nullptr, Owner, LogSmartObject, Warning, TEXT("%s: Attempting to add '%s' to collection '%s', but it already seems registered with a different container. Adding a single SmartObjectComponent to multiple collections is not supported.")
			, ANSI_TO_TCHAR(__FUNCTION__), *SOComponent.GetPathName(SOComponent.GetOwner()), *GetPathNameSafe(Owner));

		bOutAlreadyInCollection = (Entry != nullptr);
		return Entry;
	}

	check(World);
	const FSmartObjectHandle Handle = FSmartObjectHandleFactory::CreateHandleForComponent(*World, SOComponent);

	if (const FSoftObjectPath* ExistingSmartObjectPath = RegisteredIdToObjectMap.Find(Handle))
	{
		const FSoftObjectPath SmartObjectPath = &SOComponent;
		ensureMsgf(*ExistingSmartObjectPath == SmartObjectPath, TEXT("There's already an entry for a given handle that points to a different SmartObject. New SmartObject %s, Existing one %s")
			, *ExistingSmartObjectPath->ToString(), *SmartObjectPath.ToString());

		FSmartObjectCollectionEntry* Entry = CollectionEntries.FindByPredicate(UE::SmartObject::FEntryFinder(Handle));

		if (ensureMsgf(Entry, TEXT("An Entry is expected to be found since the handle has already been found in the RegisteredIdToObjectMap")))
		{
			UE_VLOG_UELOG(Owner, LogSmartObject, VeryVerbose, TEXT("'%s[%s]' already registered to collection '%s'")
				, *SOComponent.GetPathName(SOComponent.GetOwner()), *LexToString(Handle), *GetPathNameSafe(Owner));

			bOutAlreadyInCollection = true;
			return Entry;
		}
	}

	const USmartObjectDefinition* Definition = SOComponent.GetDefinition();
	checkf(Definition != nullptr, TEXT("Shouldn't reach this point with an invalid definition asset"));

	return AddSmartObjectInternal(Handle, *Definition, SOComponent);
}

FSmartObjectCollectionEntry* FSmartObjectContainer::AddSmartObjectInternal(const FSmartObjectHandle Handle, const USmartObjectDefinition& Definition, const USmartObjectComponent& SOComponent)
{
	// this function is not supposed to be called without checking if a given smart object is already present in the collection first
	checkSlow(RegisteredIdToObjectMap.Find(Handle) == nullptr);
		
	const uint32 DefinitionIndex = Definitions.AddUnique(&Definition);

	UE_VLOG_UELOG(Owner, LogSmartObject, Verbose, TEXT("Adding '%s[%s]' to collection '%s'"), *SOComponent.GetPathName(SOComponent.GetOwner()), *LexToString(Handle), *GetPathNameSafe(Owner));
	const int32 NewEntryIndex = CollectionEntries.Emplace(Handle, SOComponent, DefinitionIndex);

	RegisteredIdToObjectMap.Add(Handle, CollectionEntries[NewEntryIndex].GetPath());

	Bounds += CollectionEntries[NewEntryIndex].GetBounds();

	return &CollectionEntries[NewEntryIndex];
}

bool FSmartObjectContainer::RemoveSmartObject(USmartObjectComponent& SOComponent)
{
	FSmartObjectHandle Handle = SOComponent.GetRegisteredHandle();
	if (!Handle.IsValid())
	{
		UE_VLOG_UELOG(Owner, LogSmartObject, Verbose, TEXT("Skipped removal of '%s[%s]' from collection '%s'. Handle is not valid"),
			*SOComponent.GetPathName(SOComponent.GetOwner()), *LexToString(Handle), *GetPathNameSafe(Owner));
		return false;
	}

	UE_VLOG_UELOG(Owner, LogSmartObject, Verbose, TEXT("Removing '%s[%s]' from collection '%s'"), *SOComponent.GetPathName(SOComponent.GetOwner()), *LexToString(Handle), *GetPathNameSafe(Owner));
	const int32 Index = CollectionEntries.IndexOfByPredicate(
		[&Handle](const FSmartObjectCollectionEntry& Entry)
		{
			return Entry.GetHandle() == Handle;
		});

	if (Index != INDEX_NONE)
	{
		CollectionEntries.RemoveAt(Index);
		RegisteredIdToObjectMap.Remove(Handle);
	}

	SOComponent.InvalidateRegisteredHandle();

	return Index != INDEX_NONE;
}

#if WITH_EDITORONLY_DATA
bool FSmartObjectContainer::UpdateSmartObject(const USmartObjectComponent& SOComponent)
{
	const FSmartObjectHandle SOHandle = SOComponent.GetRegisteredHandle();

	if (RegisteredIdToObjectMap.Contains(SOHandle) == false)
	{
		return false;
	}

	FSmartObjectCollectionEntry* UpdatedEntry = CollectionEntries.FindByPredicate(UE::SmartObject::FEntryFinder(SOHandle));

	if (!ensureMsgf(UpdatedEntry, TEXT("FSmartObjectContainer.RegisteredIdToObjectMap contains the handle, but there's no entry for it. This is pretty serious.")))
	{
		return false;
	}

	const USmartObjectDefinition* Definition = SOComponent.GetDefinition();
	if (Definition == nullptr)
	{
		UE_VLOG_UELOG(Owner, LogSmartObject, Error, TEXT("Updating '%s[%s]' in collection '%s' while the SmartObjectDefinition is None. Maintaining the previous definition.")
			, *SOComponent.GetPathName(SOComponent.GetOwner()), *LexToString(SOHandle), *GetPathNameSafe(Owner));
	}
	else
	{
		// check if the definition changed
		const uint32 PrevDefinitionIndex = UpdatedEntry->GetDefinitionIndex();

		if (Definitions.IsValidIndex(PrevDefinitionIndex) == false || Definitions[PrevDefinitionIndex] != Definition)
		{
			const uint32 NewDefinitionIndex = uint32(Definitions.AddUnique(Definition));
			UpdatedEntry->SetDefinitionIndex(NewDefinitionIndex);

			// check if the old definition is still being used, if not remove it from Definitions and update the indices
			bool bPrevDefinitionStillUsed = false;
			for (const FSmartObjectCollectionEntry& Entry : CollectionEntries)
			{
				if (Entry.GetDefinitionIndex() == PrevDefinitionIndex)
				{
					bPrevDefinitionStillUsed = true;
					break;
				}
			}

			// we only care if the definition being removed is not last. If it's last we can just remove it
			// since it has no bearing on the other entries 
			const uint32 LastIndex = uint32(Definitions.Num() - 1);
			if (bPrevDefinitionStillUsed == false && PrevDefinitionIndex != LastIndex)
			{
				for (FSmartObjectCollectionEntry& Entry : CollectionEntries)
				{
					if (Entry.GetDefinitionIndex() == LastIndex)
					{
						Entry.SetDefinitionIndex(PrevDefinitionIndex);
					}
				}
			}
			Definitions.RemoveAtSwap(PrevDefinitionIndex, 1, EAllowShrinking::No);
		}
	}

	return true;
}
#endif // WITH_EDITORONLY_DATA

USmartObjectComponent* FSmartObjectContainer::GetSmartObjectComponent(const FSmartObjectHandle SmartObjectHandle) const
{
	const FSoftObjectPath* Path = RegisteredIdToObjectMap.Find(SmartObjectHandle);
	return Path != nullptr ? CastChecked<USmartObjectComponent>(Path->ResolveObject(), ECastCheckedType::NullAllowed) : nullptr;
}

const USmartObjectDefinition* FSmartObjectContainer::GetDefinitionForEntry(const FSmartObjectCollectionEntry& Entry) const
{
	const bool bIsValidIndex = Definitions.IsValidIndex(Entry.GetDefinitionIndex());
	if (!bIsValidIndex)
	{
		UE_VLOG_UELOG(Owner, LogSmartObject, Error, TEXT("Using invalid index (%d) to retrieve definition from collection '%s'"), Entry.GetDefinitionIndex(), *GetPathNameSafe(Owner));
		return nullptr;
	}

	const USmartObjectDefinition* Definition = Definitions[Entry.GetDefinitionIndex()];
	ensureMsgf(Definition != nullptr, TEXT("Collection is expected to contain only valid definition entries"));
	return Definition;
}

void FSmartObjectContainer::ValidateDefinitions()
{
	for (const USmartObjectDefinition* Definition : Definitions)
	{
		UE_CVLOG_UELOG(Definition == nullptr, Owner, LogSmartObject, Warning
			, TEXT("Null definition found at index (%d) in collection '%s'. Collection needs to be rebuilt and saved.")
			, Definitions.IndexOfByKey(Definition)
			, *GetPathNameSafe(Owner));

		if (Definition != nullptr)
		{
			Definition->Validate();
		}
	}
}

//----------------------------------------------------------------------//
// ASmartObjectPersistentCollection 
//----------------------------------------------------------------------//
ASmartObjectPersistentCollection::ASmartObjectPersistentCollection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SmartObjectContainer(this)
{
	PrimaryActorTick.bCanEverTick = false;
	bNetLoadOnClient = false;
	SetCanBeDamaged(false);

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;

	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	RootComponent = SpriteComponent;

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> NoteTextureObject;
			FName ID;
			FText NAME;
			FConstructorStatics()
				: NoteTextureObject(TEXT("/SmartObjects/S_SmartObject"))
				, ID(TEXT("SmartObjects"))
				, NAME(NSLOCTEXT("SpriteCategory", "SmartObject", "SmartObject"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.NoteTextureObject.Get();
			SpriteComponent->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME;
			SpriteComponent->Mobility = EComponentMobility::Static;
		}

		RenderingComponent = CreateEditorOnlyDefaultSubobject<USmartObjectContainerRenderingComponent>(TEXT("RenderingComponent"));
		if (RenderingComponent)
		{
			RenderingComponent->SetupAttachment(RootComponent);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void ASmartObjectPersistentCollection::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITORONLY_DATA
	UWorld* World = GetWorld();
	if (World && !World->IsGameWorld())
	{
		OnSmartObjectChangedDelegateHandle = USmartObjectComponent::GetOnSmartObjectChanged().AddUObject(this, &ASmartObjectPersistentCollection::OnSmartObjectComponentChanged);
	}
#endif // WITH_EDITORONLY_DATA
}

void ASmartObjectPersistentCollection::Destroyed()
{
#if WITH_EDITORONLY_DATA
	USmartObjectComponent::GetOnSmartObjectChanged().Remove(OnSmartObjectChangedDelegateHandle);
#endif // WITH_EDITORONLY_DATA

	// Handle editor delete.
	UnregisterWithSubsystem(ANSI_TO_TCHAR(__FUNCTION__));
	Super::Destroyed();
}

void ASmartObjectPersistentCollection::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Handle Level unload, PIE end, SIE end, game end.
	UnregisterWithSubsystem(ANSI_TO_TCHAR(__FUNCTION__));
	Super::EndPlay(EndPlayReason);
}

void ASmartObjectPersistentCollection::PostActorCreated()
{
	// Register after being initially spawned.
	Super::PostActorCreated();
	RegisterWithSubsystem(ANSI_TO_TCHAR(__FUNCTION__));
}

void ASmartObjectPersistentCollection::PreRegisterAllComponents()
{
	Super::PreRegisterAllComponents();

	// Handle UWorld::AddToWorld(), i.e. turning on level visibility
	if (const ULevel* Level = GetLevel())
	{
		// This function gets called in editor all the time, we're only interested the case where level is being added to world.
		if (Level->bIsAssociatingLevel)
		{
			RegisterWithSubsystem(ANSI_TO_TCHAR(__FUNCTION__));
		}
	}
}

void ASmartObjectPersistentCollection::PostUnregisterAllComponents()
{
	// Handle UWorld::RemoveFromWorld(), i.e. turning off level visibility
	if (const ULevel* Level = GetLevel())
	{
		// This function gets called in editor all the time, we're only interested the case where level is being removed from world.
		if (Level->bIsDisassociatingLevel)
		{
			UnregisterWithSubsystem(ANSI_TO_TCHAR(__FUNCTION__));
		}
	}

	Super::PostUnregisterAllComponents();
}

bool ASmartObjectPersistentCollection::RegisterWithSubsystem(const FString& Context)
{
	if (bRegistered)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("'%s' %s - Failed: already registered"), *GetPathName(), *Context);
		return false;
	}

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("'%s' %s - Failed: ignoring default object"), *GetPathName(), *Context);
		return false;
	}

	USmartObjectSubsystem* SmartObjectSubsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (SmartObjectSubsystem == nullptr)
	{
		// Collection might attempt to register before the subsystem is created. At its initialization the subsystem gathers
		// all collections and registers them. For this reason we use a log instead of an error.
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("'%s' %s - Failed: unable to find smart object subsystem"), *GetPathName(), *Context);
		return false;
	}

	const ESmartObjectCollectionRegistrationResult Result = SmartObjectSubsystem->RegisterCollection(*this);
	UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("'%s' %s - %s"), *GetPathName(), *Context, *UEnum::GetValueAsString(Result));
	return true;
}

bool ASmartObjectPersistentCollection::UnregisterWithSubsystem(const FString& Context)
{
	if (!bRegistered)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("'%s' %s - Failed: not registered"), *GetPathName(), *Context);
		return false;
	}

	USmartObjectSubsystem* SmartObjectSubsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (SmartObjectSubsystem == nullptr)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("'%s' %s - Failed: unable to find smart object subsystem"), *GetPathName(), *Context);
		return false;
	}

	SmartObjectSubsystem->UnregisterCollection(*this);
	UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("'%s' %s - Succeeded"), *GetPathName(), *Context);
	return true;
}

void ASmartObjectPersistentCollection::OnRegistered()
{
	bRegistered = true;
}

void ASmartObjectPersistentCollection::OnUnregistered()
{
	bRegistered = false;
}

#if WITH_EDITOR
void ASmartObjectPersistentCollection::PostEditUndo()
{
	Super::PostEditUndo();

	if (IsPendingKillPending())
	{
		UnregisterWithSubsystem(ANSI_TO_TCHAR(__FUNCTION__));
	}
	else
	{
		RegisterWithSubsystem(ANSI_TO_TCHAR(__FUNCTION__));
	}
}

void ASmartObjectPersistentCollection::ClearCollection()
{
	if (SmartObjectContainer.IsEmpty() == false)
	{
		ResetCollection();
		MarkPackageDirty();
		MarkComponentsRenderStateDirty();
	}
}

void ASmartObjectPersistentCollection::RebuildCollection()
{
	if (USmartObjectSubsystem* SmartObjectSubsystem = USmartObjectSubsystem::GetCurrent(GetWorld()))
	{
		const uint32 CollectionHash = GetTypeHash(SmartObjectContainer);

		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Rebuilding collection '%s' from component list"), *GetPathName());

		ResetCollection(SmartObjectContainer.CollectionEntries.Num());

		SmartObjectSubsystem->PopulateCollection(*this);

		if (GetTypeHash(SmartObjectContainer) != CollectionHash)
		{
			// Dirty package since this is an explicit user action that resulted in collection changes
			MarkPackageDirty();
			MarkComponentsRenderStateDirty();
		}
	}
}

void ASmartObjectPersistentCollection::AppendToCollection(const TConstArrayView<USmartObjectComponent*> InComponents)
{
	UWorld* World = GetWorld();
	check(World);

	for (int ComponentIndex = 0; ComponentIndex < InComponents.Num(); ++ComponentIndex)
	{
		USmartObjectComponent* const Component = InComponents[ComponentIndex];

		if (Component != nullptr)
		{
			if (Component->GetRegisteredHandle().IsValid() == false || Component->GetRegistrationType() == ESmartObjectRegistrationType::Dynamic)
			{
				Component->InvalidateRegisteredHandle();

				const USmartObjectDefinition* Definition = Component->GetDefinition();
				check(Definition);

				const FSmartObjectHandle Handle = FSmartObjectHandleFactory::CreateHandleForComponent(*World, *Component);

				const FSmartObjectCollectionEntry* Entry = SmartObjectContainer.AddSmartObjectInternal(Handle, *Definition, *Component);
				check(Entry);
				Component->SetRegisteredHandle(Entry->GetHandle(), ESmartObjectRegistrationType::BindToExistingInstance);
			}
			// costly tests below, but we only perform these when WITH_EDITOR
			else if (InComponents.IsValidIndex(ComponentIndex + 1) 
				&& MakeArrayView(&InComponents[ComponentIndex + 1], InComponents.Num() - (ComponentIndex + 1)).Find(Component) != INDEX_NONE)
			{
				UE_VLOG_UELOG(Owner, LogSmartObject, Warning, TEXT("%s: found '%s' duplicates while adding component array to %s.")
					, ANSI_TO_TCHAR(__FUNCTION__), *Component->GetPathName(Component->GetOwner()), *GetPathName());
			}
			else if (SmartObjectContainer.CollectionEntries.ContainsByPredicate(UE::SmartObject::FEntryFinder(Component->GetRegisteredHandle())))
			{
				// When populated by World building commandlet same actor can be loaded multiple time so simply use a verbose log when it happens
				UE_VLOG_UELOG(Owner, LogSmartObject, Verbose, TEXT("%s: Attempting to add '%s' to collection '%s', but it has already been added previously.")
					, ANSI_TO_TCHAR(__FUNCTION__), *Component->GetPathName(Component->GetOwner()), *GetPathName());
			}
			else
			{
				UE_VLOG_UELOG(Owner, LogSmartObject, Warning, TEXT("%s: Attempting to add '%s' to collection '%s', but it has already been added to a different container.")
					, ANSI_TO_TCHAR(__FUNCTION__), *Component->GetPathName(Component->GetOwner()), *GetPathName());
			}
		}
	}

	SmartObjectContainer.CollectionEntries.Shrink();
	SmartObjectContainer.RegisteredIdToObjectMap.Shrink();
	SmartObjectContainer.Definitions.Shrink();
}

void ASmartObjectPersistentCollection::ResetCollection(const int32 ExpectedNumElements)
{
	UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Reseting collection '%s'"), *GetPathName());

	SmartObjectContainer.Bounds = FBox(ForceInitToZero);
	for (FSmartObjectCollectionEntry& Entry : SmartObjectContainer.CollectionEntries)
	{
		if (USmartObjectComponent* Component = Entry.GetComponent())
		{
			Component->InvalidateRegisteredHandle();
		}
	}
	SmartObjectContainer.CollectionEntries.Reset(ExpectedNumElements);
	SmartObjectContainer.RegisteredIdToObjectMap.Empty(ExpectedNumElements);
	SmartObjectContainer.Definitions.Reset();
}

void ASmartObjectPersistentCollection::OnSmartObjectComponentChanged(const USmartObjectComponent& Instance)
{
	if (bUpdateCollectionOnSmartObjectsChange)
	{
		SmartObjectContainer.UpdateSmartObject(Instance);
	}
}
#endif // WITH_EDITOR

