// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Set.h"
#include "Containers/SortedMap.h"
#include "EntitySystem/EntityAllocationIterator.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "DoubleChannelEvaluatorSystem.generated.h"

class UMovieSceneEntitySystemLinker;
class UObject;

namespace UE
{
namespace MovieScene
{

	struct FSourceDoubleChannel;
	struct FSourceDoubleChannelFlags;

} // namespace MovieScene
} // namespace UE

/**
 * System that is responsible for evaluating double channels.
 */
UCLASS()
class MOVIESCENETRACKS_API UDoubleChannelEvaluatorSystem : public UMovieSceneEntitySystem
{
public:


	GENERATED_BODY()

	UDoubleChannelEvaluatorSystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;

	static void RegisterChannelType(TComponentTypeID<UE::MovieScene::FSourceDoubleChannel> SourceChannelType, TComponentTypeID<UE::MovieScene::FSourceDoubleChannelFlags> ChannelFlagsType, TComponentTypeID<double> ResultType);

private:

	struct FChannelType
	{
		TComponentTypeID<UE::MovieScene::FSourceDoubleChannel> ChannelType;
		TComponentTypeID<UE::MovieScene::FSourceDoubleChannelFlags> ChannelFlagsType;
		TComponentTypeID<double> ResultType;
	};

	static TArray<FChannelType, TInlineAllocator<4>> StaticChannelTypes;
};
