// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "NopNetObjectFilter.generated.h"

UCLASS(transient, MinimalAPI)
class UNopNetObjectFilterConfig final : public UNetObjectFilterConfig
{
	GENERATED_BODY()
};

UCLASS()
class UNopNetObjectFilter final : public UNetObjectFilter
{
	GENERATED_BODY()

protected:
	// UNetObjectFilter interface
	IRISCORE_API virtual void OnInit(FNetObjectFilterInitParams&) override;
	IRISCORE_API virtual bool AddObject(uint32 ObjectIndex, FNetObjectFilterAddObjectParams&) override;
	IRISCORE_API virtual void RemoveObject(uint32 ObjectIndex, const FNetObjectFilteringInfo&) override;
	IRISCORE_API virtual void UpdateObjects(FNetObjectFilterUpdateParams&) override;
	IRISCORE_API virtual void Filter(FNetObjectFilteringParams&) override;
};
