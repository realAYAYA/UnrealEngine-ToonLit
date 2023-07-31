// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FDMXDisplayClusterReplicator;


class DMXDISPLAYCLUSTER_API FDMXDisplayClusterModule 
	: public IModuleInterface
{

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
	/** Setup the DisplayCluster replicator */
	void CreateDMXDisplayClusterReplicator();

	/** The replicator instance */
	TSharedPtr<FDMXDisplayClusterReplicator> DMXDisplayClusterReplicator;
};
