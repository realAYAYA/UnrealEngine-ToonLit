// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "IDataSourceFilterSetInterface.generated.h"

enum class EFilterSetMode : uint8;

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


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "DataSourceFiltering.h"
#include "IDataSourceFilterInterface.h"
#endif
