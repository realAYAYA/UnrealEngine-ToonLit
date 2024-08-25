// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "NavEdgeProviderInterface.generated.h"

struct FNavEdgeSegment
{
	FVector P0, P1;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UNavEdgeProviderInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class INavEdgeProviderInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual bool GetEdges(const FVector& Center, float Range, TArray<FNavEdgeSegment>& Edges)
	{
		return false;
	}
};
