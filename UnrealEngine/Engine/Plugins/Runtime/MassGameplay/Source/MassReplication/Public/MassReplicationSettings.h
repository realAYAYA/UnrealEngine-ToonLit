// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassSettings.h"
#include "MassReplicationSettings.generated.h"

UCLASS(config = Mass, defaultconfig, meta = (DisplayName = "Mass Replication"))
class MASSREPLICATION_API UMassReplicationSettings : public UMassModuleSettings
{
	GENERATED_BODY()

public:
	static const UMassReplicationSettings* Get()
	{
		return GetDefault<UMassReplicationSettings>();
	}

	float GetReplicationGridCellSize() const
	{ 
		return ReplicationGridCellSize; 
	}
	
private:

	UPROPERTY(EditAnywhere, config, Category = Replication);
	float ReplicationGridCellSize = 5000.0f; // 50 meters;
};
