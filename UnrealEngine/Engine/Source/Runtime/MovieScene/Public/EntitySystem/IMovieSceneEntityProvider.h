// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "EntitySystem/MovieSceneEntityBuilder.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"
#include "Evaluation/MovieSceneCompletionMode.h"
#include "HAL/Platform.h"
#include "Math/Range.h"
#include "Misc/Guid.h"
#include "Misc/InlineValue.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "IMovieSceneEntityProvider.generated.h"

class UClass;
class UMovieSceneEntitySystemLinker;
class UMovieSceneSection;
class UObject;
struct FFrameNumber;
struct FMovieSceneEntityComponentFieldBuilder;
struct FMovieSceneEvaluationFieldEntityMetaData;
struct FMovieSceneEvaluationFieldSharedEntityMetaData;
struct FMovieSceneTimeTransform;
template <typename ElementType> class TRange;


namespace UE
{
namespace MovieScene
{

class FEntityManager;
struct FEntityImportParams;
struct IEntityBuilder;


struct FImportedEntity
{
	bool IsEmpty() const
	{
		return Builders.Num() == 0;
	}

	template<typename BuilderType>
	void AddBuilder(BuilderType&& InBuilder)
	{
		Builders.Add(Forward<BuilderType>(InBuilder));
	}

	MOVIESCENE_API FMovieSceneEntityID Manufacture(const FEntityImportParams& Params, FEntityManager* EntityManager);

private:

	TArray<TInlineValue<IEntityBuilder>, TInlineAllocator<1>> Builders;
};

struct FEntityImportSequenceParams
{
	FEntityImportSequenceParams()
		: HierarchicalBias(0)
		, DefaultCompletionMode(EMovieSceneCompletionMode::KeepState)
		, bHasHierarchicalEasing(false)
		, bPreRoll(false)
		, bPostRoll(false)
	{}

	int32 HierarchicalBias;

	FInstanceHandle InstanceHandle;
	FRootInstanceHandle RootInstanceHandle;

	EMovieSceneCompletionMode DefaultCompletionMode;

	bool bHasHierarchicalEasing : 1;
	bool bPreRoll : 1;
	bool bPostRoll : 1;
};

struct FEntityImportParams
{
	const FMovieSceneEvaluationFieldEntityMetaData* EntityMetaData;
	const FMovieSceneEvaluationFieldSharedEntityMetaData* SharedMetaData;

	uint32 EntityID = 0;

	FInterrogationKey InterrogationKey;

	FEntityImportSequenceParams Sequence;

	MOVIESCENE_API FGuid GetObjectBindingID() const;
};

} // namespace MovieScene
} // namespace UE


UINTERFACE()
class MOVIESCENE_API UMovieSceneEntityProvider : public UInterface
{
public:
	GENERATED_BODY()
};

/**
 * Interface to be added to UMovieSceneSection types when they contain entity data
 */
class MOVIESCENE_API IMovieSceneEntityProvider
{
public:

	using FEntityImportParams   = UE::MovieScene::FEntityImportParams;
	using FImportedEntity       = UE::MovieScene::FImportedEntity;


	GENERATED_BODY()


	/**
	 * Populate an evaluation field with this provider's entities
	 */
	bool PopulateEvaluationField(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder)
	{
		return PopulateEvaluationFieldImpl(EffectiveRange, InMetaData, OutFieldBuilder);
	}


	void ImportEntity(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity);
	void InterrogateEntity(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity);

private:

	virtual void ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity) = 0;
	virtual void InterrogateEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity) { ImportEntityImpl(EntityLinker, Params, OutImportedEntity); }

	/** Optional user-implementation function for populating an evaluation entity field */
	virtual bool PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder) { return false; }
};
