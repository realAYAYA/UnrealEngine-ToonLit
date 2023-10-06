// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class ILiveLinkClient;

/** References the live link client */
struct LIVELINK_API FLiveLinkClientReference
{
public:
	ILiveLinkClient* GetClient() const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
