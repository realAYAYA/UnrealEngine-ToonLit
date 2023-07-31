// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
template <typename ActorType, TEMPLATE_REQUIRES(TIsDerivedFrom<ActorType, AActor>::IsDerived)>
struct FWorldPartitionActorDescType
{};

#define DEFINE_ACTORDESC_TYPE(ActorType, ActorDescType) \
template <>												\
struct FWorldPartitionActorDescType<ActorType>			\
{														\
	typedef class ActorDescType Type;					\
};
#else
#define DEFINE_ACTORDESC_TYPE(ActorType, ActorDescType)
#endif
