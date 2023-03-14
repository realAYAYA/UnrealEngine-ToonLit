// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessingTypes.h"
#include "MassEntityManager.h"
#include "MassCommonTypes.h"
#include "MassTranslator.h"
#include "MassEntityTemplate.generated.h"


struct FMassEntityView;


UENUM()
enum class EMassEntityTemplateIDType : uint8
{
	None,
	ScriptStruct,
	Class,
	Instance
};

//ID of the template an entity is using
USTRUCT()
struct MASSSPAWNER_API FMassEntityTemplateID
{
	GENERATED_BODY()

	FMassEntityTemplateID(uint32 InHash, EMassEntityTemplateIDType InType)
	: Hash(InHash)
	, Type(InType)
	{}

	FMassEntityTemplateID() = default;

	uint32 GetHash() const { return Hash; }
	void SetHash(uint32 InHash) { Hash = InHash; }

	EMassEntityTemplateIDType GetType() const { return Type; }
	void SetType(EMassEntityTemplateIDType InType) { Type = InType; }

	bool operator==(const FMassEntityTemplateID& Other) const
	{
		return (Hash == Other.Hash) && (Type == Other.Type);
	}

	friend uint32 GetTypeHash(const FMassEntityTemplateID& TemplateID)
	{
		return HashCombine(TemplateID.Hash, (uint32)TemplateID.Type);
	}

	bool IsValid() { return Type != EMassEntityTemplateIDType::None; }

	FString ToString() const;

protected:
	UPROPERTY()
	uint32 Hash = 0;

	UPROPERTY()
	EMassEntityTemplateIDType Type = EMassEntityTemplateIDType::None;
};

/** @todo document	*/
USTRUCT()
struct MASSSPAWNER_API FMassEntityTemplate
{
	GENERATED_BODY()

	typedef TFunction<void(UObject& /*Owner*/, FMassEntityView& /*EntityView*/, const EMassTranslationDirection /*CurrentDirection*/)> FObjectFragmentInitializerFunction;

	FMassEntityTemplate() = default;
	/** InArchetype is expected to be valid. The function will crash-check it. */
	void SetArchetype(const FMassArchetypeHandle& InArchetype);
	const FMassArchetypeHandle& GetArchetype() const { return Archetype; }

	TConstArrayView<FObjectFragmentInitializerFunction> GetObjectFragmentInitializers() const { return ObjectInitializers; }
	TArray<FObjectFragmentInitializerFunction>& GetMutableObjectFragmentInitializers() { return ObjectInitializers; }

	bool IsValid() const { return Archetype.IsValid() && (Composition.IsEmpty() == false); }
	bool IsEmpty() const { return Composition.IsEmpty(); }

	void SetTemplateID(FMassEntityTemplateID InTemplateID) { TemplateID = InTemplateID; }
	FMassEntityTemplateID GetTemplateID() const { return TemplateID; }

	void SetTemplateName(const FString& Name) { TemplateName = Name; }
	const FString& GetTemplateName() const { return TemplateName; }
	
	const FMassArchetypeCompositionDescriptor& GetCompositionDescriptor() const { return Composition; }
	const FMassArchetypeSharedFragmentValues& GetSharedFragmentValues() const { return SharedFragmentValues; }
	TConstArrayView<FInstancedStruct> GetInitialFragmentValues() const { return InitialFragmentValues; }

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

	void AddFragment(FConstStructView Fragment)
	{
		const UScriptStruct* FragmentType = Fragment.GetScriptStruct();
		checkf(FragmentType && FragmentType->IsChildOf(FMassFragment::StaticStruct()), TEXT("Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types."));
		if (!Composition.Fragments.Contains(*FragmentType))
		{
			Composition.Fragments.Add(*FragmentType);
			InitialFragmentValues.Add(Fragment);
		}
		else if (!InitialFragmentValues.ContainsByPredicate(FStructTypeEqualOperator(FragmentType)))
		{
			InitialFragmentValues.Add(Fragment);
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
		else if (const FInstancedStruct* Fragment = InitialFragmentValues.FindByPredicate(FStructTypeEqualOperator(T::StaticStruct())))
		{
			return Fragment->template GetMutable<T>();
		}

		// Add a default initial fragment value
		return InitialFragmentValues.Emplace_GetRef(T::StaticStruct()).template GetMutable<T>();
	}

	template<typename T>
	void AddTag()
	{
		Composition.Tags.Add<T>();
	}
	
	void AddTag(const UScriptStruct& TagType)
	{
		Composition.Tags.Add(TagType);
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

	FString DebugGetDescription(FMassEntityManager* EntityManager = nullptr) const;
	FString DebugGetArchetypeDescription(FMassEntityManager& EntityManager) const;

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

private:
	FMassArchetypeHandle Archetype;

	FMassArchetypeCompositionDescriptor Composition;
	FMassArchetypeSharedFragmentValues SharedFragmentValues;
	
	// Initial fragment values, this is not part of the archetype as it is the spawner job to set them.
	TArray<FInstancedStruct> InitialFragmentValues;

	// These functions will be called to initialize entity's UObject-based fragments
	TArray<FObjectFragmentInitializerFunction> ObjectInitializers;

	FMassEntityTemplateID TemplateID;

	FString TemplateName;
};
