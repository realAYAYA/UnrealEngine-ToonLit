// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_WITH_IRIS

#include "Iris/Serialization/NetSerializer.h"

namespace UE::Net
{

UE_NET_DECLARE_SERIALIZER(FGameplayAbilityTargetDataHandleNetSerializer, GAMEPLAYABILITIES_API);

}

void InitGameplayAbilityTargetDataHandleNetSerializerTypeCache();

#endif // UE_WITH_IRIS
