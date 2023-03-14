// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "IDataSourceFilterInterface.h"
#include "DataSourceFiltering.h"
#include "IDataSourceFilterSetInterface.generated.h"

UINTERFACE(Blueprintable)
class SOURCEFILTERINGCORE_API UDataSourceFilterSetInterface : public UInterface
{
	GENERATED_BODY()
};

/** Interface used for implementing Engine and UnrealInsights versions respectively UDataSourceFilterSet and UTraceDataSourceFilterSet */
class SOURCEFILTERINGCORE_API IDataSourceFilterSetInterface
{
	GENERATED_BODY()

public:
	/** Return the current Filter Set operation mode */
	virtual EFilterSetMode GetFilterSetMode() const = 0;
};

