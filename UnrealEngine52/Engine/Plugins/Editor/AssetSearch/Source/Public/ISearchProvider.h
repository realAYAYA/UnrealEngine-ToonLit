// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SearchQuery.h"

class ISearchProvider
{
public:
	virtual ~ISearchProvider() { }
	virtual void Search(FSearchQueryPtr SearchQuery) = 0;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
