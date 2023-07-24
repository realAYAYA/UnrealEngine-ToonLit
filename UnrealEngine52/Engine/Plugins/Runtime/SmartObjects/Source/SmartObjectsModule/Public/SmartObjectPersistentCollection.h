// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "SmartObjectTypes.h"
#include "SmartObjectPersistentCollection.generated.h"

class UBillboardComponent;
namespace EEndPlayReason { enum Type : int; }

class USmartObjectDefinition;
class USmartObjectComponent;
class USmartObjectContainerRenderingComponent;
struct FSmartObjectContainer;
class USmartObjectSubsystem;
class ASmartObjectPersistentCollection;

/** Struct representing a unique registered component in the collection actor */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectCollectionEntry
{
	GENERATED_BODY()

	FSmartObjectCollectionEntry() = default;
	FSmartObjectCollectionEntry(const FSmartObjectHandle SmartObjectHandle, const USmartObjectComponent& SmartObjectComponent, const uint32 DefinitionIndex);

	const FSmartObjectHandle& GetHandle() const { return Handle; }
	const FSoftObjectPath& GetPath() const	{ return Path; }
	USmartObjectComponent* GetComponent() const;
	FTransform GetTransform() const { return Transform; }
	const FBox& GetBounds() const { return Bounds; }
	FBox GetWorldBounds() const { return Bounds.MoveTo(Transform.GetLocation()); }
	uint32 GetDefinitionIndex() const { return DefinitionIdx; }
	const FGameplayTagContainer& GetTags() const { return Tags; }

#if WITH_EDITORONLY_DATA
	/** Created new FSmartObjectHandle based on entry's contents. Required due to changes in how the handle is calculated. */
	void RecreateHandle(const UWorld& World);
#endif // WITH_EDITORONLY_DATA

	friend FString LexToString(const FSmartObjectCollectionEntry& CollectionEntry)
	{
		return FString::Printf(TEXT("%s - %s"), *CollectionEntry.Path.ToString(), *LexToString(CollectionEntry.Handle));
	}

protected:
	// Only the collection can access the path since the way we reference the component
	// might change to better support streaming so keeping this as encapsulated as possible
	friend FSmartObjectContainer;

#if WITH_EDITORONLY_DATA
	void SetDefinitionIndex(const uint32 InDefinitionIndex) { DefinitionIdx = InDefinitionIndex; }
#endif // WITH_EDITORONLY_DATA

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	FGameplayTagContainer Tags;

	UPROPERTY()
	FSoftObjectPath Path;

	UPROPERTY()
	FTransform Transform;

	UPROPERTY()
	FBox Bounds = FBox(ForceInitToZero);

	UPROPERTY(VisibleAnywhere, Category = SmartObject, meta = (ShowOnlyInnerProperties))
	FSmartObjectHandle Handle;

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	uint32 DefinitionIdx = INDEX_NONE;
};


USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectContainer
{
	GENERATED_BODY()

	explicit FSmartObjectContainer(UObject* InOwner = nullptr) : Owner(InOwner) {}

	/**
	 * Creates a new entry for a given component.
	 * @param SOComponent SmartObject Component for which a new entry must be created
	 * @param bAlreadyInCollection Output parameter to indicate if an existing entry was returned instead of a newly created one.
	 * @return Pointer to the created or existing entry. An unset value indicates a registration error.
	 */
	FSmartObjectCollectionEntry* AddSmartObject(USmartObjectComponent& SOComponent, bool& bOutAlreadyInCollection);
	bool RemoveSmartObject(USmartObjectComponent& SOComponent);
	
#if WITH_EDITORONLY_DATA
	/** 
	 * If SOComponent is already contained by this FSmartObjectContainer instance then data relating to it will get updated 
	 * @return whether this container instance contains SOComponent
	 */
	bool UpdateSmartObject(const USmartObjectComponent& SOComponent);
#endif // WITH_EDITORONLY_DATA

	USmartObjectComponent* GetSmartObjectComponent(const FSmartObjectHandle SmartObjectHandle) const;
	const USmartObjectDefinition* GetDefinitionForEntry(const FSmartObjectCollectionEntry& Entry) const;

	TConstArrayView<FSmartObjectCollectionEntry> GetEntries() const { return CollectionEntries; }

	void SetBounds(const FBox& InBounds) { Bounds = InBounds; }
	const FBox& GetBounds() const { return Bounds; }

	bool IsEmpty() const { return CollectionEntries.Num() == 0; }

	void Append(const FSmartObjectContainer& Other);
	int32 Remove(const FSmartObjectContainer& Other);

	void ValidateDefinitions();

	/** Note that this implementation is only expected to be used in the editor - it's pretty slow */
	friend SMARTOBJECTSMODULE_API uint32 GetTypeHash(const FSmartObjectContainer& Instance);

protected:
	friend USmartObjectSubsystem;
	friend ASmartObjectPersistentCollection;

	// assumes SOComponent to not be part of the collection yet
	FSmartObjectCollectionEntry* AddSmartObjectInternal(const FSmartObjectHandle Handle, const USmartObjectDefinition& Definition, const USmartObjectComponent& SOComponent);

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	FBox Bounds = FBox(ForceInitToZero);

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TArray<FSmartObjectCollectionEntry> CollectionEntries;

	UPROPERTY()
	TMap<FSmartObjectHandle, FSoftObjectPath> RegisteredIdToObjectMap;

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TArray<TObjectPtr<const USmartObjectDefinition>> Definitions;

	// used for reporting and debugging
	UPROPERTY()
	TObjectPtr<const UObject> Owner;

#if WITH_EDITORONLY_DATA
	FString GetFullName() const { return Owner ? Owner->GetFullName() : TEXT("None"); }
#endif // WITH_EDITORONLY_DATA
};


/** Actor holding smart object persistent data */
UCLASS(NotBlueprintable, hidecategories = (Rendering, Replication, Collision, Input, HLOD, Actor, LOD, Cooking, WorldPartition))
class SMARTOBJECTSMODULE_API ASmartObjectPersistentCollection : public AActor
{
	GENERATED_BODY()

public:
	const TArray<FSmartObjectCollectionEntry>& GetEntries() const { return SmartObjectContainer.CollectionEntries; }
	void SetBounds(const FBox& InBounds) { SmartObjectContainer.Bounds = InBounds; }
	const FBox& GetBounds() const { return SmartObjectContainer.Bounds;	}

	const FSmartObjectContainer& GetSmartObjectContainer() const { return SmartObjectContainer; };
	FSmartObjectContainer& GetMutableSmartObjectContainer() { return SmartObjectContainer; };

	bool IsEmpty() const { return SmartObjectContainer.IsEmpty(); }

#if WITH_EDITORONLY_DATA
	void ResetCollection(const int32 ExpectedNumElements = 0);
	bool ShouldDebugDraw() const { return bEnableDebugDrawing; }
#endif // WITH_EDITORONLY_DATA

protected:
	friend class USmartObjectSubsystem;

	explicit ASmartObjectPersistentCollection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostLoad() override;
	virtual void PostActorCreated() override;
	virtual void Destroyed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PreRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;

#if WITH_EDITOR
	virtual void PostEditUndo() override;
	
	/** Removes all entries from the collection. */
	UFUNCTION(CallInEditor, Category = SmartObject)
	void ClearCollection();

	/** Rebuild entries in the collection using all the SmartObjectComponents currently loaded in the level. */
	UFUNCTION(CallInEditor, Category = SmartObject)
	void RebuildCollection();

	/** Adds contents of InComponents to the stored SmartObjectContainer. Note that function does not clear out 
	 * the existing contents of SmartObjectContainer. Call ClearCollection or RebuildCollection if that is required. */
	void AppendToCollection(const TConstArrayView<USmartObjectComponent*> InComponents);

	void OnSmartObjectComponentChanged(const USmartObjectComponent& Instance);
#endif // WITH_EDITOR

	virtual bool RegisterWithSubsystem(const FString& Context);
	virtual bool UnregisterWithSubsystem(const FString& Context);

	void OnRegistered();
	bool IsRegistered() const { return bRegistered; }
	void OnUnregistered();

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	FSmartObjectContainer SmartObjectContainer;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	TObjectPtr<UBillboardComponent> SpriteComponent;

	UPROPERTY(transient)
	TObjectPtr<USmartObjectContainerRenderingComponent> RenderingComponent;

private:
	FDelegateHandle OnSmartObjectChangedDelegateHandle;

protected:
	UPROPERTY(EditAnywhere, Category = SmartObject, AdvancedDisplay)
	bool bUpdateCollectionOnSmartObjectsChange = true;

	UPROPERTY(EditAnywhere, Category = SmartObject, AdvancedDisplay)
	bool bEnableDebugDrawing = true;
#endif // WITH_EDITORONLY_DATA

protected:
	bool bRegistered = false;
};
