// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSystemTestPlugin/ReplicationSystemTestPlugin.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"

class FReplicationSystemTestPlugin: public IReplicationSystemTestPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FReplicationSystemTestPlugin, ReplicationSystemTestPlugin )

void FReplicationSystemTestPlugin::StartupModule()
{
}

void FReplicationSystemTestPlugin::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

