// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "NavFilters/NavigationQueryFilter.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "RecastFilter_UseDefaultArea.generated.h"

class ANavigationData;
class UObject;
struct FNavigationQueryFilter;

/** Regular navigation area, applied to entire navigation data by default */
UCLASS(MinimalAPI)
class URecastFilter_UseDefaultArea : public UNavigationQueryFilter
{
	GENERATED_UCLASS_BODY()

	virtual void InitializeFilter(const ANavigationData& NavData, const UObject* Querier, FNavigationQueryFilter& Filter) const override;
};
