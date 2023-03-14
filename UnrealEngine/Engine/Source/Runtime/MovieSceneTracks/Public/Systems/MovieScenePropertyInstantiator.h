// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "EntitySystem/MovieSceneDecompositionQuery.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieScenePropertyRegistry.h"
#include "EntitySystem/MovieScenePropertyComponentHandler.h"

#include "Misc/TVariant.h"
#include "Misc/Optional.h"
#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/SortedMap.h"

#include "MovieScenePropertyInstantiator.generated.h"

namespace UE
{
namespace MovieScene
{

struct FPropertyStats;
struct FPropertyDefinition;
class FEntityManager;

} // namespace MovieScene
} // namespace UE

class UMovieSceneBlenderSystem;

/** Class responsible for resolving all property types registered with FBuiltInComponentTypes::PropertyRegistry */
UCLASS()
class MOVIESCENETRACKS_API UMovieScenePropertyInstantiatorSystem
	: public UMovieSceneEntityInstantiatorSystem
{
public:

	using FMovieSceneEntityID = UE::MovieScene::FMovieSceneEntityID;

	GENERATED_BODY()

	UMovieScenePropertyInstantiatorSystem(const FObjectInitializer& ObjInit);

	/**
	 * Retrieve the stats for a specific property
	 */
	UE::MovieScene::FPropertyStats GetStatsForProperty(UE::MovieScene::FCompositePropertyTypeID PropertyID) const;


	/**
	 * Recompose a value from the constituent parts specified in InQuery, taking into account the weightings of each channel.
	 * For instance, if a property comprises 3 additive values (a:1, b:2, c:3), and we recompose 'a' with an InCurrentValue of 10, the result for a would be 5.
	 * @note: ValueType must be either copy assignable to/from the storage type of the property, or must have a ConvertOperationalProperty overload
	 *
	 * @param InComponents   The components that define the property to decompose
	 * @param InQuery        The query defining the entities and object to recompose
	 * @param InCurrentValue The value of the property to recompose
	 * @return A result matching the storage type of the components, containing recomposed values for each of the entities specified in InQuery
	 */
	template<typename PropertyTraits, typename ValueType>
	UE::MovieScene::TRecompositionResult<ValueType> RecomposeBlend(const UE::MovieScene::TPropertyComponents<PropertyTraits>& InComponents, const UE::MovieScene::FDecompositionQuery& InQuery, const ValueType& InCurrentValue)
	{
		using namespace UE::MovieScene;

		using StorageType = typename PropertyTraits::StorageType;

		StorageType TmpStorage{};
		ConvertOperationalProperty(InCurrentValue, TmpStorage);

		UE::MovieScene::TRecompositionResult<StorageType> Result = RecomposeBlendOperational(InComponents, InQuery, TmpStorage);

		UE::MovieScene::TRecompositionResult<ValueType> ConversionResult(InCurrentValue, Result.Values.Num());
		for (int32 Index = 0; Index < Result.Values.Num(); ++Index)
		{
			ConvertOperationalProperty(Result.Values[Index], ConversionResult.Values[Index]);
		}
		return ConversionResult;
	}


	/**
	 * Recompose a value from the constituent parts specified in InQuery, taking into account the weightings of each channel.
	 * For instance, if a property comprises 3 additive values (a:1, b:2, c:3), and we recompose 'a' with an InCurrentValue of 10, the result for a would be 5.
	 *
	 * @param InComponents   The components that define the property to decompose
	 * @param InQuery        The query defining the entities and object to recompose
	 * @param InCurrentValue The value of the property to recompose
	 * @return A result matching the storage type of the components, containing recomposed values for each of the entities specified in InQuery
	 */
	template<typename PropertyTraits>
	UE::MovieScene::TRecompositionResult<typename PropertyTraits::StorageType> RecomposeBlendOperational(const UE::MovieScene::TPropertyComponents<PropertyTraits>& InComponents, const UE::MovieScene::FDecompositionQuery& InQuery, const typename PropertyTraits::StorageType& InCurrentValue);

private:

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	virtual void OnLink() override;
	virtual void OnUnlink() override;

private:

	using FChannelMask     = TBitArray<TFixedAllocator< 1 >>;
	using FSlowPropertyPtr = TSharedPtr<FTrackInstancePropertyBindings>;

	struct FObjectPropertyInfo
	{
		FObjectPropertyInfo(UE::MovieScene::FResolvedProperty&& InProperty)
			: Property(MoveTemp(InProperty))
			, BoundObject(nullptr)
			, BlendChannel(INVALID_BLEND_CHANNEL)
			, bWantsRestoreState(false)
		{}

		/** Variant of the property itself as either a pointer offset, a custom property index, or slow track instance bindings object */
		UE::MovieScene::FResolvedProperty Property;

		/** POinter to the blender system to use for this property, if its blended */
		TWeakObjectPtr<UMovieSceneBlenderSystem> Blender;
		/** The object being animated */
		UObject* BoundObject;
		/** The path of the property being animated */
		FMovieScenePropertyBinding PropertyBinding;
		/** Mask of composite channels that are not animated (set bits indicate an unanimated channel) */
		FChannelMask EmptyChannels;
		/** The entity that contains the property component itself. For fast path properties this is the actual child entity produced from the bound object instantiators. */
		UE::MovieScene::FMovieSceneEntityID PropertyEntityID;
		/** Blend channel allocated from Blender, or INVALID_BLEND_CHANNEL if unblended. */
		uint16 BlendChannel;
		/** The index of this property within FPropertyRegistry::GetProperties. */
		int32 PropertyDefinitionIndex;
		/** Index of a float-based property if this property has been set for float-to-double conversion */
		TOptional<int32> ConvertedFromPropertyDefinitionIndex;
		/** true if any of the contributors to this property need restore state. */
		bool bWantsRestoreState;
	};

private:

	/* Parameter structure passed around when instantiating a specific instance of a property */
	struct FPropertyParameters
	{
		/** Pointer to the property instance to be animated */
		FObjectPropertyInfo* PropertyInfo;
		/** Pointer to the property type definition from FPropertyRegistry */
		const UE::MovieScene::FPropertyDefinition* PropertyDefinition;
		/** The index of the PropertyInfo member within UMovieScenePropertyInstantiatorSystem::ResolvedProperties */
		int32 PropertyInfoIndex;
	};
	void DiscoverInvalidatedProperties(TBitArray<>& OutInvalidatedProperties);
	void UpgradeFloatToDoubleProperties(const TBitArray<>& InvalidatedProperties);
	void ProcessInvalidatedProperties(const TBitArray<>& InvalidatedProperties);
	void UpdatePropertyInfo(const FPropertyParameters& Params);
	bool PropertySupportsFastPath(const FPropertyParameters& Params) const;
	void InitializeFastPath(const FPropertyParameters& Params);
	void InitializeBlendPath(const FPropertyParameters& Params);
	int32 ResolveProperty(UE::MovieScene::FCustomAccessorView CustomAccessors, UObject* Object, const FMovieScenePropertyBinding& PropertyBinding, int32 PropertyDefinitionIndex);

	UE::MovieScene::FPropertyRecomposerPropertyInfo FindPropertyFromSource(FMovieSceneEntityID EntityID, UObject* Object) const;

	void InitializePropertyMetaData(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents);

	void CleanTaggedGarbage(UMovieSceneEntitySystemLinker*);

private:

	static constexpr uint16 INVALID_BLEND_CHANNEL = uint16(-1);

	TSparseArray<FObjectPropertyInfo> ResolvedProperties;
	TMultiMap<int32, FMovieSceneEntityID> Contributors;
	TMultiMap<int32, FMovieSceneEntityID> NewContributors;

	/** Reverse lookup from an entity to the index within ResolvedProperties that it animates.
	 * @note: can contain INDEX_NONE for properties that have not resolved. */
	TMap<FMovieSceneEntityID, int32> EntityToProperty;
	TMap< TTuple<UObject*, FName>, int32 > ObjectPropertyToResolvedIndex;

	TArray<UE::MovieScene::FPropertyStats> PropertyStats;

	UE::MovieScene::FComponentMask CleanFastPathMask;

	TBitArray<> InitializePropertyMetaDataTasks;
	TBitArray<> SaveGlobalStateTasks;

	UE::MovieScene::FBuiltInComponentTypes* BuiltInComponents;
	 
	UE::MovieScene::FPropertyRecomposerImpl RecomposerImpl;
};


template<typename PropertyTraits>
UE::MovieScene::TRecompositionResult<typename PropertyTraits::StorageType> UMovieScenePropertyInstantiatorSystem::RecomposeBlendOperational(const UE::MovieScene::TPropertyComponents<PropertyTraits>& Components, const UE::MovieScene::FDecompositionQuery& InQuery, const typename PropertyTraits::StorageType& InCurrentValue)
{
	return RecomposerImpl.RecomposeBlendOperational<PropertyTraits>(Components, InQuery, InCurrentValue);
}
