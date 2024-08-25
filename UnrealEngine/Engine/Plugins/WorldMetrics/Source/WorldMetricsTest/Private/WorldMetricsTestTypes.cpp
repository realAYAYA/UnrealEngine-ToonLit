// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldMetricsTestTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldMetricsTestTypes)

//---------------------------------------------------------------------------------------------------------------------
// UMockWorldMetricBase
//---------------------------------------------------------------------------------------------------------------------

void UMockWorldMetricBase::Initialize()
{
	++InitializeCount;
}

void UMockWorldMetricBase::Deinitialize()
{
	++DeinitializeCount;
}

void UMockWorldMetricBase::Update(float /*DeltaTimeInSeconds*/)
{
	++UpdateCount;
}

//---------------------------------------------------------------------------------------------------------------------
// UMockWorldMetricsExtensionBase
//---------------------------------------------------------------------------------------------------------------------

void UMockWorldMetricsExtensionBase::Initialize()
{
	++InitializeCount;
}

void UMockWorldMetricsExtensionBase::Deinitialize()
{
	++DeinitializeCount;
}

void UMockWorldMetricsExtensionBase::OnAcquire(UObject* InOwner)
{
	++OnAcquireCount;
}

void UMockWorldMetricsExtensionBase::OnRelease(UObject* InOwner)
{
	++OnReleaseCount;
}
