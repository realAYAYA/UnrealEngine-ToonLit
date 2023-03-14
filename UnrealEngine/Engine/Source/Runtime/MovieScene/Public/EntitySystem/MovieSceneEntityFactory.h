// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/SortedMap.h"
#include "CoreTypes.h"
#include "EntitySystem/MovieSceneEntityFactoryTypes.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntitySystemDirectedGraph.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/InlineValue.h"

#include <initializer_list>


namespace UE
{
namespace MovieScene
{
struct FEntityAllocation;
struct FEntityRange;


enum class EComplexInclusivityFilterMode
{
	AllOf,
	AnyOf
};


struct FComplexInclusivityFilter
{
	FComponentMask Mask;
	EComplexInclusivityFilterMode Mode;

	FComplexInclusivityFilter(const FComponentMask& InMask, EComplexInclusivityFilterMode InMode)
		: Mask(InMask), Mode(InMode)
	{}

	static FComplexInclusivityFilter All(std::initializer_list<FComponentTypeID> InComponentTypes)
	{
		return FComplexInclusivityFilter(FComponentMask(InComponentTypes), EComplexInclusivityFilterMode::AllOf);
	}

	static FComplexInclusivityFilter Any(std::initializer_list<FComponentTypeID> InComponentTypes)
	{
		return FComplexInclusivityFilter(FComponentMask(InComponentTypes), EComplexInclusivityFilterMode::AnyOf);
	}

	bool Match(FComponentMask Input) const
	{
		switch (Mode)
		{
			case EComplexInclusivityFilterMode::AllOf:
				{
					FComponentMask Temp = Mask;
					Temp.CombineWithBitwiseAND(Input, EBitwiseOperatorFlags::MaintainSize);
					return Temp == Mask;
				}
				break;
			case EComplexInclusivityFilterMode::AnyOf:
				{
					FComponentMask Temp = Mask;
					Temp.CombineWithBitwiseAND(Input, EBitwiseOperatorFlags::MaintainSize);
					return Temp.Find(true) != INDEX_NONE;
				}
				break;
			default:
				checkf(false, TEXT("Not implemented"));
				return false;
		}
	}
};


struct FComplexInclusivity
{
	FComplexInclusivityFilter Filter;
	FComponentMask ComponentsToInclude;
};

/**
 * A class that contains all the component factory relationships.
 *
 * A source component (imported from an entity provider) can trigger the creation of other components on
 * the same entity or on children entities of its entity.
 */
struct FEntityFactories
{
	/**
	 * Defines a component as something that should always be created on every child entity.
	 */
	void DefineChildComponent(FComponentTypeID InChildComponent)
	{
		ParentToChildComponentTypes.AddUnique(FComponentTypeID::Invalid(), InChildComponent);
	}

	/**
	 * Specifies that if a component is present on a parent entity, the given child component should
	 * be created on any child entity.
	 */
	void DefineChildComponent(FComponentTypeID InParentComponent, FComponentTypeID InChildComponent)
	{
		ParentToChildComponentTypes.AddUnique(InParentComponent, InChildComponent);
	}

	/**
	 * Makes the given component automatically copied from a parent entity to all its children entities.
	 * @note: include "EntitySystem/MovieSceneEntityFactoryTemplates.h" for definition
	 */
	template<typename ComponentType>
	void DuplicateChildComponent(TComponentTypeID<ComponentType> InComponent);

	/**
	 * Specifies that if a component is present on a parent entity, the given child component should
	 * be created on any child entity, and initialized with the given initializer.
	 * @note: include "EntitySystem/MovieSceneEntityFactoryTemplates.h" for definition
	 */
	template<typename ParentComponent, typename ChildComponent, typename InitializerCallback>
	void DefineChildComponent(TComponentTypeID<ParentComponent> InParentType, TComponentTypeID<ChildComponent> InChildType, InitializerCallback&& InInitializer);

	/**
	 * Adds the definition for a child component. The helper methods above are easier and preferrable.
	 */
	MOVIESCENE_API void DefineChildComponent(TInlineValue<FChildEntityInitializer>&& InInitializer);

	/**
	 * Indicates that if the first component exists on an entity, the second component should be created on
	 * that entity too.
	 *
	 * @note: the inverse is not implied (ie B can still exist without A)
     */
	MOVIESCENE_API void DefineMutuallyInclusiveComponent(FComponentTypeID InComponentA, FComponentTypeID InComponentB);

	/**
	 * Specifies a mutual inclusivity relationship. The helper method above is easier and preferrable.
	 */
	MOVIESCENE_API void DefineMutuallyInclusiveComponent(TInlineValue<FMutualEntityInitializer>&& InInitializer);

	/**
	 * Specifies that if an entity matches the given filter, the specified components should be created on it.
	 * @note: include "EntitySystem/MovieSceneEntityFactoryTemplates.h" for definition
	 */
	template<typename... ComponentTypes>
	void DefineComplexInclusiveComponents(const FComplexInclusivityFilter& InFilter, ComponentTypes... InComponents);

	/**
	 * Specifies that if an entity matches the given filter, the specified component should be created on it.
	 */
	MOVIESCENE_API void DefineComplexInclusiveComponents(const FComplexInclusivityFilter& InFilter, FComponentTypeID InComponent);

	/**
	 * Defines a new complex inclusivity relationship. The helper methods above are easier and preferrable.
	 */
	MOVIESCENE_API void DefineComplexInclusiveComponents(const FComplexInclusivity& InInclusivity);

	/**
	 * Given a set of components on a parent entity, compute what components should exist on a child entity.
	 *
	 * This resolves all the parent-to-child relationships.
	 */
	MOVIESCENE_API int32 ComputeChildComponents(const FComponentMask& ParentComponentMask, FComponentMask& ChildComponentMask);

	/**
	 * Given a set of components on an entity, computes what other components should also exist on this entity.
	 *
	 * This resolves all the mutual and complex inclusivity relationships.
	 */
	MOVIESCENE_API int32 ComputeMutuallyInclusiveComponents(FComponentMask& ComponentMask);

	void RunInitializers(const FComponentMask& ParentType, const FComponentMask& ChildType, const FEntityAllocation* ParentAllocation, TArrayView<const int32> ParentAllocationOffsets, const FEntityRange& InChildEntityRange);

	TArray<TInlineValue<FChildEntityInitializer>> ChildInitializers;
	TArray<TInlineValue<FMutualEntityInitializer>> MutualInitializers;

	TMultiMap<FComponentTypeID, FComponentTypeID> ParentToChildComponentTypes;
	FMovieSceneEntitySystemDirectedGraph MutualInclusivityGraph;
	TArray<FComplexInclusivity> ComplexInclusivity;

	struct
	{
		FComponentMask AllMutualFirsts;
		FComponentMask AllComplexFirsts;
	}
	Masks;
};


}	// using namespace MovieScene
}	// using namespace UE
