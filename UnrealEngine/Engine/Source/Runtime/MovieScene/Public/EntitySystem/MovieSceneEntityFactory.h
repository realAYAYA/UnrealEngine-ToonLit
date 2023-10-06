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
#include "EntitySystem/MovieSceneMutualComponentInclusivity.h"
#include "Misc/AssertionMacros.h"
#include "Misc/InlineValue.h"

#include <initializer_list>


namespace UE
{
namespace MovieScene
{

struct IMutualComponentInitializer;

struct FEntityRange;
struct FEntityAllocation;
struct FMutualComponentInitializers;

struct UE_DEPRECATED(5.2, "Please use DefineComplexInclusiveComponents()") FComplexInclusivity
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
	void DefineMutuallyInclusiveComponent(FComponentTypeID InComponentA, FComponentTypeID InComponentB)
	{
		DefineMutuallyInclusiveComponents(InComponentA, { InComponentB });
	}

	/**
	 * Indicates that if the first component exists on an entity, the specified components should be created on
	 * that entity too.
	 *
	 * @note: the inverse is not implied (ie B can still exist without A)
	 */
	MOVIESCENE_API void DefineMutuallyInclusiveComponents(FComponentTypeID InComponentA, std::initializer_list<FComponentTypeID> InMutualComponents);

	/**
	 * Specifies a mutual inclusivity relationship along with a custom initializer for initializing the mutual component(s)
	 */
	MOVIESCENE_API void DefineMutuallyInclusiveComponents(FComponentTypeID InComponentA, std::initializer_list<FComponentTypeID> InMutualComponents, FMutuallyInclusiveComponentParams&& Params);

	/**
	 * Specifies that if an entity matches the given filter, the specified component should be created on it.
	 */
	MOVIESCENE_API void DefineComplexInclusiveComponents(const FComplexInclusivityFilter& InFilter, FComponentTypeID InComponent);

	/**
	 * Specifies that if an entity matches the given filter, the specified components should be created on it.
	 */
	MOVIESCENE_API void DefineComplexInclusiveComponents(const FComplexInclusivityFilter& InFilter, std::initializer_list<FComponentTypeID> InComponents, FMutuallyInclusiveComponentParams&& Params);

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
	MOVIESCENE_API int32 ComputeMutuallyInclusiveComponents(EMutuallyInclusiveComponentType MutualTypes, FComponentMask& ComponentMask, FMutualComponentInitializers& OutInitializers);

	MOVIESCENE_API void RunInitializers(const FComponentMask& ParentType, const FComponentMask& ChildType, const FEntityAllocation* ParentAllocation, TArrayView<const int32> ParentAllocationOffsets, const FEntityRange& InChildEntityRange);

public:

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		using FDeprecatedComplexInclusivity = FComplexInclusivity;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	/**
	 * Specifies that if an entity matches the given filter, the specified components should be created on it.
	 */
	template<typename... ComponentTypes>
	UE_DEPRECATED(5.2, "Please use DefineComplexInclusiveComponents(const FComplexInclusivityFilter&, initializer_list<FComponentTypeID>)")
	void DefineComplexInclusiveComponents(const FComplexInclusivityFilter& InFilter, FComponentTypeID InComponent, ComponentTypes... InComponents)
	{
		DefineComplexInclusiveComponents(InFilter, std::initializer_list<FComponentTypeID>({ InComponent, InComponents... }), FMutuallyInclusiveComponentParams());
	}

	/**
	 * Defines a new complex inclusivity relationship. The helper methods above are easier and preferrable.
	 */
	UE_DEPRECATED(5.2, "Please use DefineComplexInclusiveComponents(const FComplexInclusivityFilter&, FComponentTypeID)")
	MOVIESCENE_API void DefineComplexInclusiveComponents(const FDeprecatedComplexInclusivity& InInclusivity);

private:

	TArray<TInlineValue<FChildEntityInitializer>> ChildInitializers;
	TMultiMap<FComponentTypeID, FComponentTypeID> ParentToChildComponentTypes;
	UE::MovieScene::FMutualInclusivityGraph MutualInclusivityGraph;
};


}	// using namespace MovieScene
}	// using namespace UE
