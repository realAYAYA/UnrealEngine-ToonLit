// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessingTypes.h"
#include "MassEntityManager.h"
#include "MassCommonTypes.h"
#include "MassTranslator.h"
#include "Templates/SharedPointer.h"
#include "Misc/Guid.h"
#include "Hash/CityHash.h"
#include "MassEntityTemplate.generated.h"


class UMassEntityTraitBase;
struct FMassEntityView;
struct FMassEntityTemplateIDFactory;
struct FMassEntityTemplate;


//ID of the template an entity is using
USTRUCT()
struct MASSSPAWNER_API FMassEntityTemplateID
{
	GENERATED_BODY()

	FMassEntityTemplateID() 
		: FlavorHash(0), TotalHash(InvalidHash)
	{}

private:
	friend FMassEntityTemplateIDFactory;
	// use FMassEntityTemplateIDFactory to access this constructor flavor
	explicit FMassEntityTemplateID(const FGuid& InGuid, const int32 InFlavorHash = 0)
		: ConfigGuid(InGuid), FlavorHash(InFlavorHash)
	{
		 const uint64 GuidHash = CityHash64((char*)&ConfigGuid, sizeof(FGuid));
		 TotalHash = CityHash128to64({GuidHash, (uint64)InFlavorHash});
	}

public:
	uint64 GetHash64() const 
	{
		return TotalHash;
	}
	
	void Invalidate()
	{
		TotalHash = InvalidHash;
	}

	bool operator==(const FMassEntityTemplateID& Other) const
	{
		return (TotalHash == Other.TotalHash);
	}
	
	bool operator!=(const FMassEntityTemplateID& Other) const
	{
		return !(*this == Other);
	}

	/** 
	 * Note that since the function is 32-hashing a 64-bit value it's not guaranteed to produce globally unique values.
	 * But also note that it's still fine to use FMassEntityTemplateID as a TMap key type, since TMap is using 32bit hash
	 * to assign buckets rather than identify individual values.
	 */
	friend uint32 GetTypeHash(const FMassEntityTemplateID& TemplateID)
	{
		return GetTypeHash(TemplateID.TotalHash);
	}

	bool IsValid() const { return (TotalHash != InvalidHash); }

	FString ToString() const;

protected:
	UPROPERTY(VisibleAnywhere, Category="Mass")
	FGuid ConfigGuid;

	UPROPERTY()
	uint32 FlavorHash;

	UPROPERTY()
	uint64 TotalHash;

private:
	static constexpr uint64 InvalidHash = 0;
};


/** 
 * Serves as data used to define and build finalized FMassEntityTemplate instances. Describes composition and initial
 * values of fragments for entities created with this data, and lets users modify and extend the data. Once finalized as 
 * FMassEntityTemplate the data will become immutable. 
 */
USTRUCT()
struct MASSSPAWNER_API FMassEntityTemplateData
{
	GENERATED_BODY()

	typedef TFunction<void(UObject& /*Owner*/, FMassEntityView& /*EntityView*/, const EMassTranslationDirection /*CurrentDirection*/)> FObjectFragmentInitializerFunction;

	FMassEntityTemplateData() = default;
	explicit FMassEntityTemplateData(const FMassEntityTemplate& InFinalizedTemplate);

	bool IsEmpty() const { return Composition.IsEmpty(); }

	TConstArrayView<FObjectFragmentInitializerFunction> GetObjectFragmentInitializers() const { return ObjectInitializers; }
	const FString& GetTemplateName() const { return TemplateName; }
	const FMassArchetypeCompositionDescriptor& GetCompositionDescriptor() const { return Composition; }
	const FMassArchetypeSharedFragmentValues& GetSharedFragmentValues() const { return SharedFragmentValues; }
	TConstArrayView<FInstancedStruct> GetInitialFragmentValues() const { return InitialFragmentValues; }

	TArray<FMassEntityTemplateData::FObjectFragmentInitializerFunction>& GetMutableObjectFragmentInitializers() { return ObjectInitializers; }

	void SetTemplateName(const FString& Name) { TemplateName = Name; }
	
	template<typename T>
	void AddFragment()
	{
		static_assert(TIsDerivedFrom<T, FMassFragment>::IsDerived, "Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types.");
		Composition.Fragments.Add<T>();
	}

	void AddFragment(const UScriptStruct& FragmentType)
	{
		checkf(FragmentType.IsChildOf(FMassFragment::StaticStruct()), TEXT("Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types."));
		Composition.Fragments.Add(FragmentType);
	}

	// @todo this function is doing nothing if a given fragment's initial value has already been created. This seems inconsistent with the other AddFragment functions (especially AddFragment_GetRef).
	void AddFragment(FConstStructView Fragment)
	{
		const UScriptStruct* FragmentType = Fragment.GetScriptStruct();
		checkf(FragmentType && FragmentType->IsChildOf(FMassFragment::StaticStruct()), TEXT("Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types."));
		if (!Composition.Fragments.Contains(*FragmentType))
		{
			Composition.Fragments.Add(*FragmentType);
			InitialFragmentValues.Emplace(Fragment);
		}
		else if (!InitialFragmentValues.ContainsByPredicate(FStructTypeEqualOperator(FragmentType)))
		{
			InitialFragmentValues.Emplace(Fragment);
		}
	}

	template<typename T>
	T& AddFragment_GetRef()
	{
		static_assert(TIsDerivedFrom<T, FMassFragment>::IsDerived, "Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types.");
		if (!Composition.Fragments.Contains<T>())
		{
			Composition.Fragments.Add<T>();
		}
		else if (FInstancedStruct* Fragment = InitialFragmentValues.FindByPredicate(FStructTypeEqualOperator(T::StaticStruct())))
		{
			return Fragment->template GetMutable<T>();
		}

		// Add a default initial fragment value
		return InitialFragmentValues.Emplace_GetRef(T::StaticStruct()).template GetMutable<T>();
	}

	template<typename T>
	T* GetMutableFragment()
	{
		static_assert(TIsDerivedFrom<T, FMassFragment>::IsDerived, "Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types.");
		FInstancedStruct* Fragment = InitialFragmentValues.FindByPredicate(FStructTypeEqualOperator(T::StaticStruct()));
		return Fragment ? &Fragment->template GetMutable<T>() : (T*)nullptr;
	}

	template<typename T>
	void AddTag()
	{
		static_assert(TIsDerivedFrom<T, FMassTag>::IsDerived, "Given struct doesn't represent a valid mass tag type. Make sure to inherit from FMassTag or one of its child-types.");
		Composition.Tags.Add<T>();
	}
	
	void AddTag(const UScriptStruct& TagType)
	{
		checkf(TagType.IsChildOf(FMassTag::StaticStruct()), TEXT("Given struct doesn't represent a valid mass tag type. Make sure to inherit from FMassTag or one of its child-types."));
		Composition.Tags.Add(TagType);
	}

	template<typename T>
	void RemoveTag()
	{
		static_assert(TIsDerivedFrom<T, FMassTag>::IsDerived, "Given struct doesn't represent a valid mass tag type. Make sure to inherit from FMassTag or one of its child-types.");
		Composition.Tags.Remove<T>();
	}

	void RemoveTag(const UScriptStruct& TagType)
	{
		checkf(TagType.IsChildOf(FMassTag::StaticStruct()), TEXT("Given struct doesn't represent a valid mass tag type. Make sure to inherit from FMassTag or one of its child-types."));
		Composition.Tags.Remove(TagType);
	}

	const FMassTagBitSet& GetTags() const { return Composition.Tags; }
	FMassTagBitSet& GetMutableTags() { return Composition.Tags; }

	template<typename T>
	void AddChunkFragment()
	{
		static_assert(TIsDerivedFrom<T, FMassChunkFragment>::IsDerived, "Given struct doesn't represent a valid chunk fragment type. Make sure to inherit from FMassChunkFragment or one of its child-types.");
		Composition.ChunkFragments.Add<T>();
	}

	void AddConstSharedFragment(const FConstSharedStruct& SharedFragment)
	{
		const UScriptStruct* FragmentType = SharedFragment.GetScriptStruct();
		if(ensureMsgf(FragmentType && FragmentType->IsChildOf(FMassSharedFragment::StaticStruct()), TEXT("Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.")))
		{
			if (!Composition.SharedFragments.Contains(*FragmentType))
			{
				Composition.SharedFragments.Add(*FragmentType);
				SharedFragmentValues.AddConstSharedFragment(SharedFragment);
			}
#if DO_ENSURE
			else
			{
				const FConstSharedStruct* Struct = SharedFragmentValues.GetConstSharedFragments().FindByPredicate(FStructTypeEqualOperator(SharedFragment));
				ensureMsgf(Struct && *Struct == SharedFragment, TEXT("Adding 2 different const shared fragment of the same type is not allowed"));

			}
#endif // DO_ENSURE
		}
	}

	void AddSharedFragment(const FSharedStruct& SharedFragment)
	{
		const UScriptStruct* FragmentType = SharedFragment.GetScriptStruct();
		if(ensureMsgf(FragmentType && FragmentType->IsChildOf(FMassSharedFragment::StaticStruct()), TEXT("Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.")))
		{
			if (!Composition.SharedFragments.Contains(*FragmentType))
			{
				Composition.SharedFragments.Add(*FragmentType);
				SharedFragmentValues.AddSharedFragment(SharedFragment);
			}
	#if DO_ENSURE
			else
			{
				const FSharedStruct* Struct = SharedFragmentValues.GetSharedFragments().FindByPredicate(FStructTypeEqualOperator(SharedFragment));
				ensureMsgf(Struct && *Struct == SharedFragment, TEXT("Adding 2 different shared fragment of the same type is not allowed"));

			}
	#endif // DO_ENSURE
		}
	}

	template<typename T>
	bool HasFragment() const
	{
		return Composition.Fragments.Contains<T>();
	}
	
	bool HasFragment(const UScriptStruct& ScriptStruct) const
	{
		return Composition.Fragments.Contains(ScriptStruct);
	}

	template<typename T>
	bool HasTag() const
	{
		return Composition.Tags.Contains<T>();
	}

	template<typename T>
	bool HasChunkFragment() const
	{
		return Composition.ChunkFragments.Contains<T>();
	}

	template<typename T>
	bool HasSharedFragment() const
	{
		return Composition.SharedFragments.Contains<T>();
	}

	bool HasSharedFragment(const UScriptStruct& ScriptStruct) const
	{
		return Composition.SharedFragments.Contains(ScriptStruct);
	}

	void Sort()
	{
		SharedFragmentValues.Sort();
	}

	/** Compares contents of two archetypes (this and Other). Returns whether both are equivalent.
	 *  @Note that the function can be slow, depending on how elaborate the template is. This function is meant for debugging purposes. */
	bool SlowIsEquivalent(const FMassEntityTemplateData& Other) const;

	FMassArchetypeCreationParams& GetArchetypeCreationParams() { return CreationParams; }
	
protected:
	FMassArchetypeCompositionDescriptor Composition;
	FMassArchetypeSharedFragmentValues SharedFragmentValues;

	// Initial fragment values, this is not part of the archetype as it is the spawner job to set them.
	TArray<FInstancedStruct> InitialFragmentValues;

	// These functions will be called to initialize entity's UObject-based fragments
	TArray<FObjectFragmentInitializerFunction> ObjectInitializers;

	FMassArchetypeCreationParams CreationParams;

	FString TemplateName;
};

/**
 * A finalized and const wrapper for FMassEntityTemplateData, associated with a Mass archetype and template ID. 
 * Designed to never be changed. If a change is needed a copy of the hosted FMassEntityTemplateData needs to be made and 
 * used to create another finalized FMassEntityTemplate (via FMassEntityTemplateManager).
 */
struct MASSSPAWNER_API FMassEntityTemplate final : public TSharedFromThis<FMassEntityTemplate> 
{
	friend TSharedFromThis<FMassEntityTemplate>;

	FMassEntityTemplate() = default;
	FMassEntityTemplate(const FMassEntityTemplateData& InData, FMassEntityManager& EntityManager, FMassEntityTemplateID InTemplateID);
	FMassEntityTemplate(FMassEntityTemplateData&& InData, FMassEntityManager& EntityManager, FMassEntityTemplateID InTemplateID);

	/** InArchetype is expected to be valid. The function will crash-check it. */
	void SetArchetype(const FMassArchetypeHandle& InArchetype);
	const FMassArchetypeHandle& GetArchetype() const { return Archetype; }

	bool IsValid() const { return Archetype.IsValid(); }

	void SetTemplateID(FMassEntityTemplateID InTemplateID) { TemplateID = InTemplateID; }
	FMassEntityTemplateID GetTemplateID() const { return TemplateID; }

	FString DebugGetDescription(FMassEntityManager* EntityManager = nullptr) const;
	FString DebugGetArchetypeDescription(FMassEntityManager& EntityManager) const;

	static TSharedRef<FMassEntityTemplate> MakeFinalTemplate(FMassEntityManager& EntityManager, FMassEntityTemplateData&& TempTemplateData, FMassEntityTemplateID InTemplateID);

	//-----------------------------------------------------------------------------
	// FMassEntityTemplateData getters
	//-----------------------------------------------------------------------------
	FORCEINLINE TConstArrayView<FMassEntityTemplateData::FObjectFragmentInitializerFunction> GetObjectFragmentInitializers() const { return TemplateData.GetObjectFragmentInitializers(); }
	FORCEINLINE const FString& GetTemplateName() const { return TemplateData.GetTemplateName(); }
	FORCEINLINE const FMassArchetypeCompositionDescriptor& GetCompositionDescriptor() const { return TemplateData.GetCompositionDescriptor(); }
	FORCEINLINE const FMassArchetypeSharedFragmentValues& GetSharedFragmentValues() const { return TemplateData.GetSharedFragmentValues(); }
	FORCEINLINE TConstArrayView<FInstancedStruct> GetInitialFragmentValues() const { return TemplateData.GetInitialFragmentValues(); }

	const FMassEntityTemplateData& GetTemplateData() const { return TemplateData; }

private:
	FMassEntityTemplateData TemplateData;
	FMassArchetypeHandle Archetype;
	FMassEntityTemplateID TemplateID;
};


struct MASSSPAWNER_API FMassEntityTemplateIDFactory
{
	static FMassEntityTemplateID Make(const FGuid& ConfigGuid);
	static FMassEntityTemplateID MakeFlavor(const FMassEntityTemplateID& SourceTemplateID, const int32 Flavor);
};
