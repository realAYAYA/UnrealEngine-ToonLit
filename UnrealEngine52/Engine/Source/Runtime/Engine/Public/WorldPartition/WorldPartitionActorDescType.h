// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/UnrealTypeTraits.h"

class AActor;

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
