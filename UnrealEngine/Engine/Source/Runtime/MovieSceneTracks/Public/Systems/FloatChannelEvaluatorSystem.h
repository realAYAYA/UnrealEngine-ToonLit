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

#include "FloatChannelEvaluatorSystem.generated.h"

class UMovieSceneEntitySystemLinker;
class UObject;

namespace UE
{
namespace MovieScene
{

	struct FSourceFloatChannel;
	struct FSourceFloatChannelFlags;

} // namespace MovieScene
} // namespace UE

/**
 * System that is responsible for evaluating float channels.
 */
UCLASS()
class MOVIESCENETRACKS_API UFloatChannelEvaluatorSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UFloatChannelEvaluatorSystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;

	static void RegisterChannelType(TComponentTypeID<UE::MovieScene::FSourceFloatChannel> SourceChannelType, TComponentTypeID<UE::MovieScene::FSourceFloatChannelFlags> ChannelFlagsType, TComponentTypeID<double> ResultType);

private:

	struct FChannelType
	{
		TComponentTypeID<UE::MovieScene::FSourceFloatChannel> ChannelType;
		TComponentTypeID<UE::MovieScene::FSourceFloatChannelFlags> ChannelFlagsType;
		TComponentTypeID<double> ResultType;
	};

	static TArray<FChannelType, TInlineAllocator<16>> StaticChannelTypes;
};