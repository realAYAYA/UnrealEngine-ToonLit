// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassExternalSubsystemTraits.h"
#include "MassEntityQuery.h"
#include "MassSubsystemAccess.h"


struct MASSENTITY_API FMassExecutionContext
{
private:

	template< typename ViewType >
	struct TFragmentView 
	{
		FMassFragmentRequirementDescription Requirement;
		ViewType FragmentView;

		TFragmentView() {}
		explicit TFragmentView(const FMassFragmentRequirementDescription& InRequirement) : Requirement(InRequirement) {}

		bool operator==(const UScriptStruct* FragmentType) const { return Requirement.StructType == FragmentType; }
	};
	using FFragmentView = TFragmentView<TArrayView<FMassFragment>>;
	TArray<FFragmentView, TInlineAllocator<8>> FragmentViews;

	using FChunkFragmentView = TFragmentView<FStructView>;
	TArray<FChunkFragmentView, TInlineAllocator<4>> ChunkFragmentViews;

	using FConstSharedFragmentView = TFragmentView<FConstStructView>;
	TArray<FConstSharedFragmentView, TInlineAllocator<4>> ConstSharedFragmentViews;

	using FSharedFragmentView = TFragmentView<FStructView>;
	TArray<FSharedFragmentView, TInlineAllocator<4>> SharedFragmentViews;

	FMassSubsystemAccess SubsystemAccess;
	
	// mz@todo make this shared ptr thread-safe and never auto-flush in MT environment. 
	TSharedPtr<FMassCommandBuffer> DeferredCommandBuffer;
	TArrayView<FMassEntityHandle> EntityListView;
	
	/** If set this indicates the exact archetype and its chunks to be processed. 
	 *  @todo this data should live somewhere else, preferably be just a parameter to Query.ForEachEntityChunk function */
	FMassArchetypeEntityCollection EntityCollection;
	
	/** @todo rename to "payload" */
	FInstancedStruct AuxData;
	float DeltaTimeSeconds = 0.0f;
	int32 ChunkSerialModificationNumber = -1;
	FMassArchetypeCompositionDescriptor CurrentArchetypeCompositionDescriptor;

	TSharedRef<FMassEntityManager> EntityManager;

#if WITH_MASSENTITY_DEBUG
	FString DebugExecutionDescription;
#endif
	
	/** Used to control when the context is allowed to flush commands collected in DeferredCommandBuffer. This mechanism 
	 * is mainly utilized to avoid numerous small flushes in favor of fewer larger ones. */
	bool bFlushDeferredCommands = true;

	TArrayView<FFragmentView> GetMutableRequirements() { return FragmentViews; }
	TArrayView<FChunkFragmentView> GetMutableChunkRequirements() { return ChunkFragmentViews; }
	TArrayView<FConstSharedFragmentView> GetMutableConstSharedRequirements() { return ConstSharedFragmentViews; }
	TArrayView<FSharedFragmentView> GetMutableSharedRequirements() { return SharedFragmentViews; }

	void GetSubsystemRequirementBits(FMassExternalSubsystemBitSet& OutConstSubsystemsBitSet, FMassExternalSubsystemBitSet& OutMutableSubsystemsBitSet)
	{
		SubsystemAccess.GetSubsystemRequirementBits(OutConstSubsystemsBitSet, OutMutableSubsystemsBitSet);
	}

	void SetSubsystemRequirementBits(const FMassExternalSubsystemBitSet& InConstSubsystemsBitSet, const FMassExternalSubsystemBitSet& InMutableSubsystemsBitSet)
	{
		SubsystemAccess.SetSubsystemRequirementBits(InConstSubsystemsBitSet, InMutableSubsystemsBitSet);
	}

	EMassExecutionContextType ExecutionType = EMassExecutionContextType::Local;

	friend FMassArchetypeData;
	friend FMassEntityQuery;

public:
	explicit FMassExecutionContext(FMassEntityManager& InEntityManager, const float InDeltaTimeSeconds = 0.f, const bool bInFlushDeferredCommands = true);

	FMassEntityManager& GetEntityManagerChecked() { return EntityManager.Get(); }

#if WITH_MASSENTITY_DEBUG
	const FString& DebugGetExecutionDesc() const { return DebugExecutionDescription; }
	void DebugSetExecutionDesc(const FString& Description) { DebugExecutionDescription = Description; }
#endif

	/** Sets bFlushDeferredCommands. Note that setting to True while the system is being executed doesn't result in
	 *  immediate commands flushing */
	void SetFlushDeferredCommands(const bool bNewFlushDeferredCommands) { bFlushDeferredCommands = bNewFlushDeferredCommands; } 
	void SetDeferredCommandBuffer(const TSharedPtr<FMassCommandBuffer>& InDeferredCommandBuffer) { DeferredCommandBuffer = InDeferredCommandBuffer; }
	void SetEntityCollection(const FMassArchetypeEntityCollection& InEntityCollection);
	void SetEntityCollection(FMassArchetypeEntityCollection&& InEntityCollection);
	void ClearEntityCollection() { EntityCollection.Reset(); }
	void SetAuxData(const FInstancedStruct& InAuxData) { AuxData = InAuxData; }
	void SetExecutionType(EMassExecutionContextType InExecutionType) { check(InExecutionType != EMassExecutionContextType::MAX); ExecutionType = InExecutionType; }

	float GetDeltaTimeSeconds() const
	{
		return DeltaTimeSeconds;
	}

	UWorld* GetWorld();

	TSharedPtr<FMassCommandBuffer> GetSharedDeferredCommandBuffer() const { return DeferredCommandBuffer; }
	FMassCommandBuffer& Defer() const { checkSlow(DeferredCommandBuffer.IsValid()); return *DeferredCommandBuffer.Get(); }

	TConstArrayView<FMassEntityHandle> GetEntities() const { return EntityListView; }
	int32 GetNumEntities() const { return EntityListView.Num(); }

	FMassEntityHandle GetEntity(const int32 Index) const
	{
		return EntityListView[Index];
	}

	bool DoesArchetypeHaveFragment(const UScriptStruct& FragmentType) const
	{
		return CurrentArchetypeCompositionDescriptor.Fragments.Contains(FragmentType);
	}

	template<typename T>
	bool DoesArchetypeHaveFragment() const
	{
		static_assert(TIsDerivedFrom<T, FMassFragment>::IsDerived, "Given struct is not of a valid fragment type.");
		return CurrentArchetypeCompositionDescriptor.Fragments.Contains<T>();
	}

	bool DoesArchetypeHaveTag(const UScriptStruct& TagType) const
	{
		return CurrentArchetypeCompositionDescriptor.Tags.Contains(TagType);
	}

	template<typename T>
	bool DoesArchetypeHaveTag() const
	{
		static_assert(TIsDerivedFrom<T, FMassTag>::IsDerived, "Given struct is not of a valid tag type.");
		return CurrentArchetypeCompositionDescriptor.Tags.Contains<T>();
	}

	/** Chunk related operations */
	void SetCurrentChunkSerialModificationNumber(const int32 SerialModificationNumber) { ChunkSerialModificationNumber = SerialModificationNumber; }
	int32 GetChunkSerialModificationNumber() const { return ChunkSerialModificationNumber; }

	template<typename T>
	T* GetMutableChunkFragmentPtr()
	{
		static_assert(TIsDerivedFrom<T, FMassChunkFragment>::IsDerived, "Given struct doesn't represent a valid chunk fragment type. Make sure to inherit from FMassChunkFragment or one of its child-types.");

		const UScriptStruct* Type = T::StaticStruct();
		FChunkFragmentView* FoundChunkFragmentData = ChunkFragmentViews.FindByPredicate([Type](const FChunkFragmentView& Element) { return Element.Requirement.StructType == Type; } );
		return FoundChunkFragmentData ? FoundChunkFragmentData->FragmentView.GetPtr<T>() : static_cast<T*>(nullptr);
	}
	
	template<typename T>
	T& GetMutableChunkFragment()
	{
		T* ChunkFragment = GetMutableChunkFragmentPtr<T>();
		checkf(ChunkFragment, TEXT("Chunk Fragment requirement not found: %s"), *T::StaticStruct()->GetName());
		return *ChunkFragment;
	}

	template<typename T>
	const T* GetChunkFragmentPtr() const
	{
		static_assert(TIsDerivedFrom<T, FMassChunkFragment>::IsDerived, "Given struct doesn't represent a valid chunk fragment type. Make sure to inherit from FMassChunkFragment or one of its child-types.");

		const UScriptStruct* Type = T::StaticStruct();
		const FChunkFragmentView* FoundChunkFragmentData = ChunkFragmentViews.FindByPredicate([Type](const FChunkFragmentView& Element) { return Element.Requirement.StructType == Type; } );
		return FoundChunkFragmentData ? FoundChunkFragmentData->FragmentView.GetPtr<T>() : static_cast<const T*>(nullptr);
	}
	
	template<typename T>
	const T& GetChunkFragment() const
	{
		const T* ChunkFragment = GetChunkFragmentPtr<T>();
		checkf(ChunkFragment, TEXT("Chunk Fragment requirement not found: %s"), *T::StaticStruct()->GetName());
		return *ChunkFragment;
	}

	/** Shared fragment related operations */
	template<typename T>
	const T* GetConstSharedFragmentPtr() const
	{
		static_assert(TIsDerivedFrom<T, FMassSharedFragment>::IsDerived, "Given struct doesn't represent a valid Shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");

		const FConstSharedFragmentView* FoundSharedFragmentData = ConstSharedFragmentViews.FindByPredicate([](const FConstSharedFragmentView& Element) { return Element.Requirement.StructType == T::StaticStruct(); });
		return FoundSharedFragmentData ? FoundSharedFragmentData->FragmentView.GetPtr<const T>() : static_cast<const T*>(nullptr);
	}

	template<typename T>
	const T& GetConstSharedFragment() const
	{
		const T* SharedFragment = GetConstSharedFragmentPtr<const T>();
		checkf(SharedFragment, TEXT("Shared Fragment requirement not found: %s"), *T::StaticStruct()->GetName());
		return *SharedFragment;
	}


	template<typename T>
	T* GetMutableSharedFragmentPtr()
	{
		static_assert(TIsDerivedFrom<T, FMassSharedFragment>::IsDerived, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");

		FSharedFragmentView* FoundSharedFragmentData = SharedFragmentViews.FindByPredicate([](const FSharedFragmentView& Element) { return Element.Requirement.StructType == T::StaticStruct(); });
		return FoundSharedFragmentData ? FoundSharedFragmentData->FragmentView.GetPtr<T>() : static_cast<T*>(nullptr);
	}

	template<typename T>
	const T* GetSharedFragmentPtr() const
	{
		static_assert(TIsDerivedFrom<T, FMassSharedFragment>::IsDerived, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");

		const FSharedFragmentView* FoundSharedFragmentData = SharedFragmentViews.FindByPredicate([](const FSharedFragmentView& Element) { return Element.Requirement.StructType == T::StaticStruct(); });
		return FoundSharedFragmentData ? FoundSharedFragmentData->FragmentView.GetPtr<T>() : static_cast<const T*>(nullptr);
	}

	template<typename T>
	T& GetMutableSharedFragment()
	{
		T* SharedFragment = GetMutableSharedFragmentPtr<T>();
		checkf(SharedFragment, TEXT("Shared Fragment requirement not found: %s"), *T::StaticStruct()->GetName());
		return *SharedFragment;
	}

	template<typename T>
	const T& GetSharedFragment() const
	{
		const T* SharedFragment = GetSharedFragmentPtr<T>();
		checkf(SharedFragment, TEXT("Shared Fragment requirement not found: %s"), *T::StaticStruct()->GetName());
		return *SharedFragment;
	}

	/* Fragments related operations */
	template<typename TFragment>
	TArrayView<TFragment> GetMutableFragmentView()
	{
		const UScriptStruct* FragmentType = TFragment::StaticStruct();
		const FFragmentView* View = FragmentViews.FindByPredicate([FragmentType](const FFragmentView& Element) { return Element.Requirement.StructType == FragmentType; });
		checkfSlow(View != nullptr, TEXT("Requested fragment type not bound"));
		checkfSlow(View->Requirement.AccessMode == EMassFragmentAccess::ReadWrite, TEXT("Requested fragment has not been bound for writing"));
		return MakeArrayView<TFragment>((TFragment*)View->FragmentView.GetData(), View->FragmentView.Num());
	}

	template<typename TFragment>
	TConstArrayView<TFragment> GetFragmentView() const
	{
		const UScriptStruct* FragmentType = TFragment::StaticStruct();
		const FFragmentView* View = FragmentViews.FindByPredicate([FragmentType](const FFragmentView& Element) { return Element.Requirement.StructType == FragmentType; });
		checkfSlow(View != nullptr, TEXT("Requested fragment type not bound. Make sure the Frament was requested in ReadOnly or ReadWrite mode."));
		return TConstArrayView<TFragment>((const TFragment*)View->FragmentView.GetData(), View->FragmentView.Num());
	}

	TConstArrayView<FMassFragment> GetFragmentView(const UScriptStruct* FragmentType) const
	{
		const FFragmentView* View = FragmentViews.FindByPredicate([FragmentType](const FFragmentView& Element) { return Element.Requirement.StructType == FragmentType; });
		checkSlow(View);
		return TConstArrayView<FMassFragment>((const FMassFragment*)View->FragmentView.GetData(), View->FragmentView.Num());;
	}

	TArrayView<FMassFragment> GetMutableFragmentView(const UScriptStruct* FragmentType) 
	{
		const FFragmentView* View = FragmentViews.FindByPredicate([FragmentType](const FFragmentView& Element) { return Element.Requirement.StructType == FragmentType; });
		checkSlow(View);
		return View->FragmentView;
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T* GetMutableSubsystem()
	{
		return SubsystemAccess.GetMutableSubsystem<T>();
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T& GetMutableSubsystemChecked()
	{
		return SubsystemAccess.GetMutableSubsystemChecked<T>();
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T* GetSubsystem()
	{
		return SubsystemAccess.GetSubsystem<T>();
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T& GetSubsystemChecked()
	{
		return SubsystemAccess.GetSubsystemChecked<T>();
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T* GetMutableSubsystem(const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetMutableSubsystem<T>(SubsystemClass);
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T& GetMutableSubsystemChecked(const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetMutableSubsystemChecked<T>(SubsystemClass);
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T* GetSubsystem(const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetSubsystem<T>(SubsystemClass);
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T& GetSubsystemChecked(const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetSubsystemChecked<T>(SubsystemClass);
	}

	/** Sparse chunk related operation */
	const FMassArchetypeEntityCollection& GetEntityCollection() const { return EntityCollection; }

	const FInstancedStruct& GetAuxData() const { return AuxData; }
	FInstancedStruct& GetMutableAuxData() { return AuxData; }
	
	template<typename TFragment>
	bool ValidateAuxDataType() const
	{
		const UScriptStruct* FragmentType = GetAuxData().GetScriptStruct();
		return FragmentType != nullptr && FragmentType == TFragment::StaticStruct();
	}

	void FlushDeferred();

	void ClearExecutionData();
	void SetCurrentArchetypeCompositionDescriptor(const FMassArchetypeCompositionDescriptor& Descriptor)
	{
		CurrentArchetypeCompositionDescriptor = Descriptor;
	}

	/** 
	 * Processes SubsystemRequirements to fetch and cache all the indicated subsystems. If a UWorld is required to fetch
	 * a specific subsystem then the one associated with the stored EntityManager will be used.
	 *
	 * @param SubsystemRequirements indicates all the subsystems that are expected to be accessed. Requesting a subsystem 
	 *	not indicated by the SubsystemRequirements will result in a failure.
	 * 
	 * @return `true` if all required subsystems have been found, `false` otherwise.
	 */
	bool CacheSubsystemRequirements(const FMassSubsystemRequirements& SubsystemRequirements);

protected:
	void SetSubsystemRequirements(const FMassSubsystemRequirements& SubsystemRequirements)
	{
		SubsystemAccess.SetSubsystemRequirements(SubsystemRequirements);
	}

	void SetFragmentRequirements(const FMassFragmentRequirements& FragmentRequirements);

	void ClearFragmentViews()
	{
		for (FFragmentView& View : FragmentViews)
		{
			View.FragmentView = TArrayView<FMassFragment>();
		}
		for (FChunkFragmentView& View : ChunkFragmentViews)
		{
			View.FragmentView.Reset();
		}
		for (FConstSharedFragmentView& View : ConstSharedFragmentViews)
		{
			View.FragmentView.Reset();
		}
		for (FSharedFragmentView& View : SharedFragmentViews)
		{
			View.FragmentView.Reset();
		}
	}

public:
	//////////////////////////////////////////////////////////////////////////
	// DEPRECATED functions

	template<typename T>
	UE_DEPRECATED(5.2, "The Get*Subsystem(World) functions have been deprecated. Use the world-less flavor.")
	T* GetMutableSubsystem(const UWorld* World)
	{
		return SubsystemAccess.GetMutableSubsystem<T>();
	}

	template<typename T>
	UE_DEPRECATED(5.2, "The Get*Subsystem(World) functions have been deprecated. Use the world-less flavor.")
	T& GetMutableSubsystemChecked(const UWorld* World)
	{
		return SubsystemAccess.GetMutableSubsystemChecked<T>();
	}

	template<typename T>
	UE_DEPRECATED(5.2, "The Get*Subsystem(World) functions have been deprecated. Use the world-less flavor.")
	const T* GetSubsystem(const UWorld* World)
	{
		return SubsystemAccess.GetSubsystem<T>();
	}

	template<typename T>
	UE_DEPRECATED(5.2, "The Get*Subsystem(World) functions have been deprecated. Use the world-less flavor.")
	const T& GetSubsystemChecked(const UWorld* World)
	{
		return SubsystemAccess.GetSubsystemChecked<T>();
	}

	template<typename T>
	UE_DEPRECATED(5.2, "The Get*Subsystem(World) functions have been deprecated. Use the world-less flavor.")
	T* GetMutableSubsystem(const UWorld* World, const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetMutableSubsystem<T>(SubsystemClass);
	}

	template<typename T>
	UE_DEPRECATED(5.2, "The Get*Subsystem(World) functions have been deprecated. Use the world-less flavor.")
	T& GetMutableSubsystemChecked(const UWorld* World, const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetMutableSubsystemChecked<T>(SubsystemClass);
	}

	template<typename T>
	UE_DEPRECATED(5.2, "The Get*Subsystem(World) functions have been deprecated. Use the world-less flavor.")
	const T* GetSubsystem(const UWorld* World, const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetSubsystem<T>(SubsystemClass);
	}

	template<typename T>
	UE_DEPRECATED(5.2, "The Get*Subsystem(World) functions have been deprecated. Use the world-less flavor.")
	const T& GetSubsystemChecked(const UWorld* World, const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetSubsystemChecked<T>(SubsystemClass);
	}

	UE_DEPRECATED(5.2, "This version of CacheSubsystemRequirements is deprecated. Use the one without the UWorld parameter")
	bool CacheSubsystemRequirements(const UWorld*, const FMassSubsystemRequirements& SubsystemRequirements)
	{
		return CacheSubsystemRequirements(SubsystemRequirements);
	}

	UE_DEPRECATED(5.4, "Deprecated in favor of 'SetCurrentArchetypeCompositionDescriptor' as this provides information on the entire archetype.")
	void SetCurrentArchetypesTagBitSet(const FMassTagBitSet&) {}
};
