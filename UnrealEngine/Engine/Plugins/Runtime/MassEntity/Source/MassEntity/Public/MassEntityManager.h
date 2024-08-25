// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/GCObject.h"
#include "MassEntityTypes.h"
#include "MassProcessingTypes.h"
#include "InstancedStruct.h"
#include "MassEntityQuery.h"
#include "StructUtilsTypes.h"
#include "MassObserverManager.h"
#include "Containers/MpscQueue.h"
#include "MassRequirementAccessDetector.h"
#include "Templates/FunctionFwd.h"


struct FMassEntityQuery;
struct FMassExecutionContext;
struct FMassArchetypeData;
struct FMassCommandBuffer;
struct FMassArchetypeEntityCollection;
class FOutputDevice;
struct FMassDebugger;
enum class EMassFragmentAccess : uint8;

/** 
 * The type responsible for hosting Entities managing Archetypes.
 * Entities are stored as FEntityData entries in a chunked array. 
 * Each valid entity is assigned to an Archetype that stored fragments associated with a given entity at the moment. 
 * 
 * FMassEntityManager supplies API for entity creation (that can result in archetype creation) and entity manipulation.
 * Even though synchronized manipulation methods are available in most cases the entity operations are performed via a command 
 * buffer. The default command buffer can be obtained with a Defer() call. @see FMassCommandBuffer for more details.
 * 
 * FMassEntityManager are meant to be stored with a TSharedPtr or TSharedRef. Some of Mass API pass around 
 * FMassEntityManager& but programmers can always use AsShared() call to obtain a shared ref for a given manager instance 
 * (as supplied by deriving from TSharedFromThis<FMassEntityManager>).
 * IMPORTANT: if you create your own FMassEntityManager instance remember to call Initialize() before using it.
 */
struct MASSENTITY_API FMassEntityManager : public TSharedFromThis<FMassEntityManager>, public FGCObject
{
	friend FMassEntityQuery;
	friend FMassDebugger;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNewArchetypeDelegate, const FMassArchetypeHandle&);

private:
	// Index 0 is reserved so we can treat that index as an invalid entity handle
	constexpr static int32 NumReservedEntities = 1;

	struct FEntityData
	{
		TSharedPtr<FMassArchetypeData> CurrentArchetype;
		int32 SerialNumber = 0;

		void Reset()
		{
			CurrentArchetype.Reset();
			SerialNumber = 0;
		}

		bool IsValid() const
		{
			return SerialNumber != 0 && CurrentArchetype.IsValid();
		}
	};
	
public:
	struct FScopedProcessing
	{
		explicit FScopedProcessing(std::atomic<int32>& InProcessingScopeCount) : ScopedProcessingCount(InProcessingScopeCount)
		{
			++ScopedProcessingCount;
		}
		~FScopedProcessing()
		{
			--ScopedProcessingCount;
		}
	private:
		std::atomic<int32>& ScopedProcessingCount;
	};
	using FStructInitializationCallback = TFunctionRef<void(void* Fragment, const UScriptStruct& FragmentType)>;

	const static FMassEntityHandle InvalidEntity;

	explicit FMassEntityManager(UObject* InOwner = nullptr);
	FMassEntityManager(const FMassEntityManager& Other) = delete;
	virtual ~FMassEntityManager();

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FMassEntityManager");
	}
	// End of FGCObject interface
	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize);

	void Initialize();
	void PostInitialize();
	void Deinitialize();

	/** 
	 * A special, relaxed but slower version of CreateArchetype functions that allows FragmentAngTagsList to contain 
	 * both fragments and tags. 
	 */
	FMassArchetypeHandle CreateArchetype(TConstArrayView<const UScriptStruct*> FragmentsAndTagsList, const FMassArchetypeCreationParams& CreationParams = FMassArchetypeCreationParams());

	/**
	 * A special, relaxed but slower version of CreateArchetype functions that allows FragmentAngTagsList to contain
	 * both fragments and tags. This version takes an original archetype and copies it layout, then appends any fragments and tags from the
	 * provided list if they're not already in the original archetype.
	 */
	FMassArchetypeHandle CreateArchetype(FMassArchetypeHandle SourceArchetype, 
		TConstArrayView<const UScriptStruct*> FragmentsAndTagsList, const FMassArchetypeCreationParams& CreationParams = FMassArchetypeCreationParams());

	/**
	 * CreateArchetype from a composition descriptor and initial values
	 *
	 * @param Composition of fragment, tag and chunk fragment types
	 * @param CreationParams Parameters used during archetype construction
	 * @return a handle of a new archetype 
	 */
	FMassArchetypeHandle CreateArchetype(const FMassArchetypeCompositionDescriptor& Composition, const FMassArchetypeCreationParams& CreationParams = FMassArchetypeCreationParams());

	/** 
	 *  Creates an archetype like SourceArchetype + InFragments. 
	 *  @param SourceArchetype the archetype used to initially populate the list of fragments of the archetype being created. 
	 *  @param InFragments list of unique fragments to add to fragments fetched from SourceArchetype. Note that 
	 *   adding an empty list is not supported and doing so will result in failing a `check`
	 *  @param CreationParams Parameters used during archetype construction
	 *  @return a handle of a new archetype
	 *  @note it's caller's responsibility to ensure that NewFragmentList is not empty and contains only fragment
	 *   types that SourceArchetype doesn't already have. If the caller cannot guarantee it use of AddFragment functions
	 *   family is recommended.
	 */
	FMassArchetypeHandle CreateArchetype(const TSharedPtr<FMassArchetypeData>& SourceArchetype, const FMassFragmentBitSet& InFragments, const FMassArchetypeCreationParams& CreationParams = FMassArchetypeCreationParams());

	/** Fetches the archetype for a given Entity. If Entity is not valid it will still return a handle, just with an invalid archetype */
	FMassArchetypeHandle GetArchetypeForEntity(FMassEntityHandle Entity) const;
	/**
	 * Fetches the archetype for a given Entity. Note that it's callers responsibility the given Entity handle is valid.
	 * If you can't ensure that call GetArchetypeForEntity.
	 */
	FMassArchetypeHandle GetArchetypeForEntityUnsafe(FMassEntityHandle Entity) const;

	/** Method to iterate on all the fragment types of an archetype */
	static void ForEachArchetypeFragmentType(const FMassArchetypeHandle& ArchetypeHandle, TFunction< void(const UScriptStruct* /*FragmentType*/)> Function);

	/**
	 * Go through all archetypes and compact entities
	 * @param TimeAllowed to do entity compaction, once it reach that time it will stop and return
	 */
	void DoEntityCompaction(const double TimeAllowed);

	/**
	 * Creates fully built entity ready to be used by the subsystem
	 * @param Archetype you want this entity to be
	 * @param SharedFragmentValues to be associated with the entity
	 * @return FMassEntityHandle id of the newly created entity */
	FMassEntityHandle CreateEntity(const FMassArchetypeHandle& ArchetypeHandle, const FMassArchetypeSharedFragmentValues& SharedFragmentValues = {});

	/**
	 * Creates fully built entity ready to be used by the subsystem
	 * @param FragmentInstanceList is the fragments to create the entity from and initialize values
	 * @param SharedFragmentValues to be associated with the entity
	 * @param ArchetypeDebugName Name to identify the archetype while debugging
	 * @return FMassEntityHandle id of the newly created entity */
	FMassEntityHandle CreateEntity(TConstArrayView<FInstancedStruct> FragmentInstanceList, const FMassArchetypeSharedFragmentValues& SharedFragmentValues = {}, const FMassArchetypeCreationParams& CreationParams = FMassArchetypeCreationParams());

	/**
	 * A dedicated structure for ensuring the "on entities creation" observers get notified only once all other 
	 * initialization operations are done and this creation context instance gets released. */
	struct FEntityCreationContext
	{
		explicit FEntityCreationContext(const int32 InNumSpawned = 0)
			: NumberSpawned(InNumSpawned)
		{}
		~FEntityCreationContext() { if (OnSpawningFinished) OnSpawningFinished(*this); }

		const FMassArchetypeEntityCollection& GetEntityCollection() const { return EntityCollection; }
	private:
		friend FMassEntityManager;
		int32 NumberSpawned;
		FMassArchetypeEntityCollection EntityCollection;
		TFunction<void(FEntityCreationContext&)> OnSpawningFinished;
	};

	/**
	 * A version of CreateEntity that's creating a number of entities (Count) in one go
	 * @param Archetype you want this entity to be
	 * @param SharedFragmentValues to be associated with the entities
	 * @param ReservedEntities a list of reserved entities that have not yet been assigned to an archetype.
	 * @return a creation context that will notify all the interested observers about newly created fragments once the context is released */
	TSharedRef<FEntityCreationContext> BatchCreateReservedEntities(const FMassArchetypeHandle& ArchetypeHandle,
		const FMassArchetypeSharedFragmentValues& SharedFragmentValues, TConstArrayView<FMassEntityHandle> ReservedEntities);
	FORCEINLINE TSharedRef<FEntityCreationContext> BatchCreateReservedEntities(const FMassArchetypeHandle& ArchetypeHandle,
		TConstArrayView<FMassEntityHandle> OutEntities)
	{
		return BatchCreateReservedEntities(ArchetypeHandle, FMassArchetypeSharedFragmentValues(), OutEntities);
	}
	/**
	 * A version of CreateEntity that's creating a number of entities (Count) in one go
	 * @param Archetype you want this entity to be
	 * @param SharedFragmentValues to be associated with the entities
	 * @param Count number of entities to create
	 * @param OutEntities the newly created entities are appended to given array, i.e. the pre-existing content of OutEntities won't be affected by the call
	 * @return a creation context that will notify all the interested observers about newly created fragments once the context is released */
	TSharedRef<FEntityCreationContext> BatchCreateEntities(const FMassArchetypeHandle& ArchetypeHandle, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const int32 Count, TArray<FMassEntityHandle>& OutEntities);
	FORCEINLINE TSharedRef<FEntityCreationContext> BatchCreateEntities(const FMassArchetypeHandle& ArchetypeHandle, const int32 Count, TArray<FMassEntityHandle>& OutEntities)
	{
		return BatchCreateEntities(ArchetypeHandle, FMassArchetypeSharedFragmentValues(), Count, OutEntities);
	}

	/**
	 * Destroys a fully built entity, use ReleaseReservedEntity if entity was not yet built.
	 * @param Entity to destroy */
	void DestroyEntity(FMassEntityHandle Entity);

	/**
	 * Reserves an entity in the subsystem, the entity is still not ready to be used by the subsystem, need to call BuildEntity()
	 * @return FMassEntityHandle id of the reserved entity */
	FMassEntityHandle ReserveEntity();

	/**
	 * Builds an entity for it to be ready to be used by the subsystem
	 * @param Entity to build which was retrieved with ReserveEntity() method
	 * @param Archetype you want this entity to be
	 * @param SharedFragmentValues to be associated with the entity
	 */
	void BuildEntity(FMassEntityHandle Entity, const FMassArchetypeHandle& ArchetypeHandle, const FMassArchetypeSharedFragmentValues& SharedFragmentValues = {});

	/**
	 * Builds an entity for it to be ready to be used by the subsystem
	 * @param Entity to build which was retrieved with ReserveEntity() method
	 * @param FragmentInstanceList is the fragments to create the entity from and initialize values
	 * @param SharedFragmentValues to be associated with the entity
	 */
	void BuildEntity(FMassEntityHandle Entity, TConstArrayView<FInstancedStruct> FragmentInstanceList, const FMassArchetypeSharedFragmentValues& SharedFragmentValues = {});

	/*
	 * Releases a previously reserved entity that was not yet built, otherwise call DestroyEntity
	 * @param Entity to release */
	void ReleaseReservedEntity(FMassEntityHandle Entity);

	/**
	 * Destroys all the entity in the provided array of entities
	 * @param InEntities to destroy
	 */
	void BatchDestroyEntities(TConstArrayView<FMassEntityHandle> InEntities);

	void BatchDestroyEntityChunks(const FMassArchetypeEntityCollection& Collection);

	void AddFragmentToEntity(FMassEntityHandle Entity, const UScriptStruct* FragmentType);
	void AddFragmentToEntity(FMassEntityHandle Entity, const UScriptStruct* FragmentType, const FStructInitializationCallback& Initializer);

	/** 
	 *  Ensures that only unique fragments are added. 
	 *  @note It's caller's responsibility to ensure Entity's and FragmentList's validity. 
	 */
	void AddFragmentListToEntity(FMassEntityHandle Entity, TConstArrayView<const UScriptStruct*> FragmentList);

	void AddFragmentInstanceListToEntity(FMassEntityHandle Entity, TConstArrayView<FInstancedStruct> FragmentInstanceList);
	void RemoveFragmentFromEntity(FMassEntityHandle Entity, const UScriptStruct* FragmentType);
	void RemoveFragmentListFromEntity(FMassEntityHandle Entity, TConstArrayView<const UScriptStruct*> FragmentList);

	void AddTagToEntity(FMassEntityHandle Entity, const UScriptStruct* TagType);
	void RemoveTagFromEntity(FMassEntityHandle Entity, const UScriptStruct* TagType);
	void SwapTagsForEntity(FMassEntityHandle Entity, const UScriptStruct* FromFragmentType, const UScriptStruct* ToFragmentType);

	void BatchBuildEntities(const FMassArchetypeEntityCollectionWithPayload& EncodedEntitiesWithPayload, const FMassFragmentBitSet& FragmentsAffected
		, const FMassArchetypeSharedFragmentValues& SharedFragmentValues = {}, const FMassArchetypeCreationParams& CreationParams = FMassArchetypeCreationParams());
	void BatchBuildEntities(const FMassArchetypeEntityCollectionWithPayload& EncodedEntitiesWithPayload, FMassArchetypeCompositionDescriptor&& Composition
		, const FMassArchetypeSharedFragmentValues& SharedFragmentValues = {}, const FMassArchetypeCreationParams& CreationParams = FMassArchetypeCreationParams());
	void BatchChangeTagsForEntities(TConstArrayView<FMassArchetypeEntityCollection> EntityCollections, const FMassTagBitSet& TagsToAdd, const FMassTagBitSet& TagsToRemove);
	void BatchChangeFragmentCompositionForEntities(TConstArrayView<FMassArchetypeEntityCollection> EntityCollections, const FMassFragmentBitSet& FragmentsToAdd, const FMassFragmentBitSet& FragmentsToRemove);
	void BatchAddFragmentInstancesForEntities(TConstArrayView<FMassArchetypeEntityCollectionWithPayload> EntityCollections, const FMassFragmentBitSet& FragmentsAffected);

	/**
	 * Adds fragments and tags indicated by InOutDescriptor to the Entity. The function also figures out which elements
	 * in InOutDescriptor are missing from the current composition of the given entity and then returns the resulting 
	 * delta via InOutDescriptor.
	 */
	void AddCompositionToEntity_GetDelta(FMassEntityHandle Entity, FMassArchetypeCompositionDescriptor& InOutDescriptor);
	void RemoveCompositionFromEntity(FMassEntityHandle Entity, const FMassArchetypeCompositionDescriptor& InDescriptor);

	const FMassArchetypeCompositionDescriptor& GetArchetypeComposition(const FMassArchetypeHandle& ArchetypeHandle) const;

	/** 
	 * Moves an entity over to a new archetype by copying over fragments common to both archetypes
	 * @param Entity is the entity to move 
	 * @param NewArchetypeHandle the handle to the new archetype
	 */
	void MoveEntityToAnotherArchetype(FMassEntityHandle Entity, FMassArchetypeHandle NewArchetypeHandle);

	/** Copies values from FragmentInstanceList over to Entity's fragment. Caller is responsible for ensuring that 
	 *  the given entity does have given fragments. Failing this assumption will cause a check-fail.*/
	void SetEntityFragmentsValues(FMassEntityHandle Entity, TArrayView<const FInstancedStruct> FragmentInstanceList);

	/** Copies values from FragmentInstanceList over to fragments of given entities collection. The caller is responsible 
	 *  for ensuring that the given entity archetype (FMassArchetypeEntityCollection .Archetype) does have given fragments. 
	 *  Failing this assumption will cause a check-fail. */
	static void BatchSetEntityFragmentsValues(const FMassArchetypeEntityCollection& SparseEntities, TArrayView<const FInstancedStruct> FragmentInstanceList);

	// Return true if it is an valid built entity
	bool IsEntityActive(FMassEntityHandle Entity) const 
	{
		return IsEntityValid(Entity) && IsEntityBuilt(Entity);
	}

	// Returns true if Entity is valid
	bool IsEntityValid(FMassEntityHandle Entity) const;

	// Returns true if Entity is has been fully built (expecting a valid Entity)
	bool IsEntityBuilt(FMassEntityHandle Entity) const;

	// Asserts that IsEntityValid
	void CheckIfEntityIsValid(FMassEntityHandle Entity) const;

	// Asserts that IsEntityBuilt
	void CheckIfEntityIsActive(FMassEntityHandle Entity) const;

	template<typename FragmentType>
	FragmentType& GetFragmentDataChecked(FMassEntityHandle Entity) const
	{
		static_assert(TIsDerivedFrom<FragmentType, FMassFragment>::IsDerived
			, "Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types.");
		return *((FragmentType*)InternalGetFragmentDataChecked(Entity, FragmentType::StaticStruct()));
	}

	template<typename FragmentType>
	FragmentType* GetFragmentDataPtr(FMassEntityHandle Entity) const
	{
		static_assert(TIsDerivedFrom<FragmentType, FMassFragment>::IsDerived
			, "Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types.");
		return (FragmentType*)InternalGetFragmentDataPtr(Entity, FragmentType::StaticStruct());
	}

	FStructView GetFragmentDataStruct(FMassEntityHandle Entity, const UScriptStruct* FragmentType) const
	{
		checkf((FragmentType != nullptr) && FragmentType->IsChildOf(FMassFragment::StaticStruct())
			, TEXT("GetFragmentDataStruct called with an invalid fragment type '%s'"), *GetPathNameSafe(FragmentType));
		return FStructView(FragmentType, static_cast<uint8*>(InternalGetFragmentDataPtr(Entity, FragmentType)));
	}

	template<typename ConstSharedFragmentType>
	ConstSharedFragmentType* GetConstSharedFragmentDataPtr(FMassEntityHandle Entity) const
	{
		static_assert(TIsDerivedFrom<ConstSharedFragmentType, FMassSharedFragment>::IsDerived, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");
		const FConstSharedStruct* ConstSharedStruct = InternalGetConstSharedFragmentPtr(Entity, ConstSharedFragmentType::StaticStruct());
		return (ConstSharedFragmentType*)(ConstSharedStruct ? ConstSharedStruct->GetMemory() : nullptr);
	}

	template<typename ConstSharedFragmentType>
	ConstSharedFragmentType& GetConstSharedFragmentDataChecked(FMassEntityHandle Entity) const
	{
		ConstSharedFragmentType* TypePtr = GetConstSharedFragmentDataPtr<ConstSharedFragmentType>(Entity);
		check(TypePtr);
		return *TypePtr;
	}

	FConstStructView GetConstSharedFragmentDataStruct(FMassEntityHandle Entity, const UScriptStruct* ConstSharedFragmentType) const
	{
		checkf((ConstSharedFragmentType != nullptr) && ConstSharedFragmentType->IsChildOf(FMassSharedFragment::StaticStruct())
			, TEXT("GetConstSharedFragmentDataStruct called with an invalid fragment type '%s'"), *GetPathNameSafe(ConstSharedFragmentType));
		const FConstSharedStruct* ConstSharedStruct = InternalGetConstSharedFragmentPtr(Entity, ConstSharedFragmentType);
		return ConstSharedStruct
			? FConstStructView(*ConstSharedStruct)
			: FConstStructView();
	}

	template<typename SharedFragmentType>
	TConstArrayView<FSharedStruct> GetSharedFragmentsOfType()
	{
		static_assert(TIsDerivedFrom<SharedFragmentType, FMassSharedFragment>::IsDerived
			, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");

		TArray<FSharedStruct>* InstancesOfType = SharedFragmentsTypeMap.Find(SharedFragmentType::StaticStruct());
		return InstancesOfType ? *InstancesOfType : TConstArrayView<FSharedStruct>();
	}

	template<typename SharedFragmentType>
	SharedFragmentType* GetSharedFragmentDataPtr(FMassEntityHandle Entity) const
	{
		static_assert(TIsDerivedFrom<SharedFragmentType, FMassSharedFragment>::IsDerived
			, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");
		const FSharedStruct* FragmentPtr = InternalGetSharedFragmentPtr(Entity, SharedFragmentType::StaticStruct());
		return (SharedFragmentType*)(FragmentPtr ? FragmentPtr->GetMemory() : nullptr);
	}

	template<typename SharedFragmentType>
	SharedFragmentType& GetSharedFragmentDataChecked(FMassEntityHandle Entity) const
	{
		SharedFragmentType* TypePtr = GetSharedFragmentDataPtr<SharedFragmentType>(Entity);
		check(TypePtr);
		return *TypePtr;
	}

	FConstStructView GetSharedFragmentDataStruct(FMassEntityHandle Entity, const UScriptStruct* SharedFragmentType) const
	{
		checkf((SharedFragmentType != nullptr) && SharedFragmentType->IsChildOf(FMassSharedFragment::StaticStruct())
			, TEXT("GetSharedFragmentDataStruct called with an invalid fragment type '%s'"), *GetPathNameSafe(SharedFragmentType));
		const FSharedStruct* FragmentPtr = InternalGetSharedFragmentPtr(Entity, SharedFragmentType);
		return FragmentPtr
			? FConstStructView(*FragmentPtr)
			: FConstStructView();
	}

	uint32 GetArchetypeDataVersion() const { return ArchetypeDataVersion; }

	/**
	 * Creates and initializes a FMassExecutionContext instance.
	 */
	FMassExecutionContext CreateExecutionContext(const float DeltaSeconds);

	FScopedProcessing NewProcessingScope() { return FScopedProcessing(ProcessingScopeCount); }

	/** 
	 * Indicates whether there are processors out there performing operations on this instance of MassEntityManager. 
	 * Used to ensure that mutating operations (like entity destruction) are not performed while processors are running, 
	 * which rely on the assumption that the data layout doesn't change during calculations. 
	 */
	bool IsProcessing() const { return ProcessingScopeCount > 0; }

	FMassCommandBuffer& Defer() const { return *DeferredCommandBuffer.Get(); }
	/** 
	 * @param InCommandBuffer if not set then the default command buffer will be flushed. If set and there's already 
	 *		a command buffer being flushed (be it the main one or a previously requested one) then this command buffer 
	 *		will be queue itself.
	 */
	void FlushCommands(const TSharedPtr<FMassCommandBuffer>& InCommandBuffer = TSharedPtr<FMassCommandBuffer>());

	/** 
	 * Depending on the current state of Manager's command buffer the function will either move all the commands out of 
	 * InOutCommandBuffer into the main command buffer or append it to the list of command buffers waiting to be flushed.
	 * @note as a consequence of the call InOutCommandBuffer can get its contents emptied due some of the undelying code using Move semantics
	 */
	void AppendCommands(TSharedPtr<FMassCommandBuffer>& InOutCommandBuffer);

	/**
	 * Shared fragment creation methods
	 */
	template<typename T>
	const FConstSharedStruct& GetOrCreateConstSharedFragmentByHash(const uint32 Hash, const T& Fragment)
	{
		static_assert(TIsDerivedFrom<T, FMassSharedFragment>::IsDerived, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");
		int32& Index = ConstSharedFragmentsMap.FindOrAddByHash(Hash, Hash, INDEX_NONE);
		if (Index == INDEX_NONE)
		{
			Index = ConstSharedFragments.Add(FSharedStruct::Make(Fragment));
		}
		return ConstSharedFragments[Index];
	}

	template<typename T>
	const FConstSharedStruct& GetOrCreateConstSharedFragment(const T& Fragment)
	{
		const uint32 Hash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(Fragment));
		return GetOrCreateConstSharedFragmentByHash(Hash, Fragment);
	}

	template<typename T, typename... TArgs>
	const FSharedStruct& GetOrCreateSharedFragmentByHash(const uint32 Hash, TArgs&&... InArgs)
	{
		static_assert(TIsDerivedFrom<T, FMassSharedFragment>::IsDerived, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");

		int32& Index = SharedFragmentsMap.FindOrAddByHash(Hash, Hash, INDEX_NONE);
		if (Index == INDEX_NONE)
		{
			Index = SharedFragments.Add(FSharedStruct::Make<T>(Forward<TArgs>(InArgs)...));
			// note that even though we're copying the freshly created FSharedStruct instance it's perfectly fine since 
			// FSharedStruct do guarantee there's not going to be data duplication (via a member shared pointer to hosted data)
			TArray<FSharedStruct>& InstancesOfType = SharedFragmentsTypeMap.FindOrAdd(T::StaticStruct(), {});
			InstancesOfType.Add(SharedFragments[Index]);
		}

		return SharedFragments[Index];
	}

	template<typename T>
	void ForEachSharedFragment(TFunctionRef< void(T& /*SharedFragment*/) > ExecuteFunction)
	{
		if (TArray<FSharedStruct>* InstancesOfType = SharedFragmentsTypeMap.Find(T::StaticStruct()))
		{
			for (const FSharedStruct& SharedStruct : *InstancesOfType)
			{
				ExecuteFunction(SharedStruct.Get<T>());
			}
		}
	}

	template<typename T>
	void ForEachSharedFragmentConditional(TFunctionRef< bool(T& /*SharedFragment*/) > ConditionFunction, TFunctionRef< void(T& /*SharedFragment*/) > ExecuteFunction)
	{
		if (TArray<FSharedStruct>* InstancesOfType = SharedFragmentsTypeMap.Find(T::StaticStruct()))
		{
			for (const FSharedStruct& SharedStruct : *InstancesOfType)
			{
				T& StructInstanceRef = SharedStruct.Get<T>();
				if (ConditionFunction(StructInstanceRef))
				{
					ExecuteFunction(StructInstanceRef);
				}
			}
		}
	}

	FMassObserverManager& GetObserverManager() { return ObserverManager; }

	FOnNewArchetypeDelegate& GetOnNewArchetypeEvent() { return OnNewArchetypeEvent; }
	/** 
	 * Fetches the world associated with the Owner. 
	 * @note that it's ok for a given EntityManager to not have an owner or the owner not being part of a UWorld, depending on the use case
	 */
	UWorld* GetWorld() const { return Owner.IsValid() ? Owner->GetWorld() : nullptr; }
	UObject* GetOwner() const { return Owner.Get(); }

	void SetDebugName(const FString& NewDebugGame);
#if WITH_MASSENTITY_DEBUG
	void DebugPrintArchetypes(FOutputDevice& Ar, const bool bIncludeEmpty = true) const;
	void DebugGetArchetypesStringDetails(FOutputDevice& Ar, const bool bIncludeEmpty = true) const;
	void DebugGetArchetypeFragmentTypes(const FMassArchetypeHandle& Archetype, TArray<const UScriptStruct*>& InOutFragmentList) const;
	int32 DebugGetArchetypeEntitiesCount(const FMassArchetypeHandle& Archetype) const;
	int32 DebugGetArchetypeEntitiesCountPerChunk(const FMassArchetypeHandle& Archetype) const;
	int32 DebugGetEntityCount() const { return Entities.Num() - NumReservedEntities - EntityFreeIndexList.Num(); }
	int32 DebugGetArchetypesCount() const { return AllArchetypes.Num(); }
	void DebugRemoveAllEntities();
	void DebugForceArchetypeDataVersionBump() { ++ArchetypeDataVersion; }
	void DebugGetArchetypeStrings(const FMassArchetypeHandle& Archetype, TArray<FName>& OutFragmentNames, TArray<FName>& OutTagNames);
	FMassEntityHandle DebugGetEntityIndexHandle(const int32 EntityIndex) const { return Entities.IsValidIndex(EntityIndex) ? FMassEntityHandle(EntityIndex, Entities[EntityIndex].SerialNumber) : FMassEntityHandle(); }
	const FString& DebugGetName() const { return DebugName; }

	FMassRequirementAccessDetector& GetRequirementAccessDetector() { return RequirementAccessDetector; }
#endif // WITH_MASSENTITY_DEBUG

protected:
	void GetMatchingArchetypes(const FMassFragmentRequirements& Requirements, TArray<FMassArchetypeHandle>& OutValidArchetypes, const uint32 FromArchetypeDataVersion) const;
	
	/** 
	 * A "similar" archetype is an archetype exactly the same as SourceArchetype except for one composition aspect 
	 * like Fragments or "Tags" 
	 */
	FMassArchetypeHandle InternalCreateSimilarArchetype(const TSharedPtr<FMassArchetypeData>& SourceArchetype, const FMassTagBitSet& OverrideTags);
	FMassArchetypeHandle InternalCreateSimilarArchetype(const TSharedPtr<FMassArchetypeData>& SourceArchetype, const FMassFragmentBitSet& OverrideFragments);

	FMassArchetypeHandle InternalCreateSimilarArchetype(const FMassArchetypeData& SourceArchetypeRef, FMassArchetypeCompositionDescriptor&& NewComposition);

	void InternalAppendFragmentsAndTagsToArchetypeCompositionDescriptor(FMassArchetypeCompositionDescriptor& InOutComposition,
		TConstArrayView<const UScriptStruct*> FragmentsAndTagsList) const;

private:
	void InternalBuildEntity(FMassEntityHandle Entity, const FMassArchetypeHandle& ArchetypeHandle, const FMassArchetypeSharedFragmentValues& SharedFragmentValues);
	void InternalReleaseEntity(FMassEntityHandle Entity);

	/** 
	 *  Adds fragments in FragmentList to Entity. Only the unique fragments will be added.
	 *  @return Bitset for the added fragments (might be empty or a subset of `InFragments` depending on the current archetype fragments)
	 */
	FMassFragmentBitSet InternalAddFragmentListToEntityChecked(FMassEntityHandle Entity, const FMassFragmentBitSet& InFragments);

	/** 
	 *  Similar to InternalAddFragmentListToEntity but expects NewFragmentList not overlapping with current entity's
	 *  fragment list. It's callers responsibility to ensure that's true. Failing this will cause a `check` fail.
	 */
	void InternalAddFragmentListToEntity(FMassEntityHandle Entity, const FMassFragmentBitSet& InFragments);
	/** Note that it's the caller's responsibility to ensure `FragmentType` is a kind of FMassFragment */
	void* InternalGetFragmentDataChecked(FMassEntityHandle Entity, const UScriptStruct* FragmentType) const;
	/** Note that it's the caller's responsibility to ensure `FragmentType` is a kind of FMassFragment */
	void* InternalGetFragmentDataPtr(FMassEntityHandle Entity, const UScriptStruct* FragmentType) const;
	/** Note that it's the caller's responsibility to ensure `ConstSharedFragmentType` is a kind of FMassSharedFragment */
	const FConstSharedStruct* InternalGetConstSharedFragmentPtr(FMassEntityHandle Entity, const UScriptStruct* ConstSharedFragmentType) const;
	/** Note that it's the caller's responsibility to ensure `SharedFragmentType` is a kind of FMassSharedFragment */
	const FSharedStruct* InternalGetSharedFragmentPtr(FMassEntityHandle Entity, const UScriptStruct* SharedFragmentType) const;

	TSharedRef<FEntityCreationContext> InternalBatchCreateReservedEntities(const FMassArchetypeHandle& ArchetypeHandle,
		const FMassArchetypeSharedFragmentValues& SharedFragmentValues, TConstArrayView<FMassEntityHandle> ReservedEntities);
private:
	TChunkedArray<FEntityData> Entities;
	TArray<int32> EntityFreeIndexList;

	std::atomic<bool> bCommandBufferFlushingInProgress = false;
	TSharedPtr<FMassCommandBuffer> DeferredCommandBuffer;
	TMpscQueue<TSharedPtr<FMassCommandBuffer>> FlushedCommandBufferQueue;

	std::atomic<int32> SerialNumberGenerator = 0;
	std::atomic<int32> ProcessingScopeCount = 0;

	// the "version" number increased every time an archetype gets added
	uint32 ArchetypeDataVersion = 0;

	// Map of hash of sorted fragment list to archetypes with that hash
	TMap<uint32, TArray<TSharedPtr<FMassArchetypeData>>> FragmentHashToArchetypeMap;

	// Map to list of archetypes that contain the specified fragment type
	TMap<const UScriptStruct*, TArray<TSharedPtr<FMassArchetypeData>>> FragmentTypeToArchetypeMap;

	// Contains all archetypes ever created. The array always growing and a given archetypes remains at a given index 
	// throughout its lifetime, and the index is never reused for another archetype. 
	TArray<TSharedPtr<FMassArchetypeData>> AllArchetypes;

	// Shared fragments
	TArray<FConstSharedStruct> ConstSharedFragments;
	// Hash/Index in array pair
	TMap<uint32, int32> ConstSharedFragmentsMap;

	TArray<FSharedStruct> SharedFragments;
	// Hash/Index in array pair, indices point at SharedFragments
	TMap<uint32, int32> SharedFragmentsMap;
	// Maps specific struct type to a collection of FSharedStruct instances of that type
	TMap<UScriptStruct*, TArray<FSharedStruct>> SharedFragmentsTypeMap;

	FMassObserverManager ObserverManager;

#if WITH_MASSENTITY_DEBUG
	FMassRequirementAccessDetector RequirementAccessDetector;
	FString DebugName;
#endif // WITH_MASSENTITY_DEBUG

	TWeakObjectPtr<UObject> Owner;

	FOnNewArchetypeDelegate OnNewArchetypeEvent;

	bool bInitialized = false;
	bool bFirstCommandFlush = true;


	//-----------------------------------------------------------------------------
	// DEPRECATED
	//-----------------------------------------------------------------------------
public:
	UE_DEPRECATED(5.3, "This Flavor of CreateArchetype is deprecated. Use the one with FMassArchetypeCreationParams parameter instead.")
	FMassArchetypeHandle CreateArchetype(TConstArrayView<const UScriptStruct*> FragmentsAndTagsList, const FName ArchetypeDebugName);
	UE_DEPRECATED(5.3, "This Flavor of CreateArchetype is deprecated. Use the one with FMassArchetypeCreationParams parameter instead.")
	FMassArchetypeHandle CreateArchetype(FMassArchetypeHandle SourceArchetype, TConstArrayView<const UScriptStruct*> FragmentsAndTagsList, const FName ArchetypeDebugName);
	UE_DEPRECATED(5.3, "This Flavor of CreateArchetype is deprecated. Use the one with FMassArchetypeCreationParams parameter instead.")
	FMassArchetypeHandle CreateArchetype(const FMassArchetypeCompositionDescriptor& Composition, const FName ArchetypeDebugName);
	UE_DEPRECATED(5.3, "This Flavor of CreateArchetype is deprecated. Use the one with FMassArchetypeCreationParams parameter instead.")
	FMassArchetypeHandle CreateArchetype(const TSharedPtr<FMassArchetypeData>& SourceArchetype, const FMassFragmentBitSet& InFragments, const FName ArchetypeDebugName);
	UE_DEPRECATED(5.3, "This Flavor of CreateEntity is deprecated. Use the one with FMassArchetypeCreationParams parameter instead.")
	FMassEntityHandle CreateEntity(TConstArrayView<FInstancedStruct> FragmentInstanceList, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FName ArchetypeDebugName);
	UE_DEPRECATED(5.3, "This Flavor of BatchBuildEntities is deprecated. Use the one with FMassArchetypeCreationParams parameter instead.")
	void BatchBuildEntities(const FMassArchetypeEntityCollectionWithPayload& EncodedEntitiesWithPayload, const FMassFragmentBitSet& FragmentsAffected
		, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FName ArchetypeDebugName);
	UE_DEPRECATED(5.3, "This Flavor of BatchBuildEntities is deprecated. Use the one with FMassArchetypeCreationParams parameter instead.")
	void BatchBuildEntities(const FMassArchetypeEntityCollectionWithPayload& EncodedEntitiesWithPayload, FMassArchetypeCompositionDescriptor&& Composition
		, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FName ArchetypeDebugName);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "MassExecutionContext.h"
#endif
