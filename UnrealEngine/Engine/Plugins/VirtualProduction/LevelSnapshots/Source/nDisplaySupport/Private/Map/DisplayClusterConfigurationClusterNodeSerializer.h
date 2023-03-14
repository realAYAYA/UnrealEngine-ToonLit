// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Helpers/MapSubobjectSerializer.h"

namespace UE::LevelSnapshots
{
	class ILevelSnapshotsModule;
}

namespace UE::LevelSnapshots::nDisplay::Private
{
	
	class FDisplayClusterConfigurationClusterNodeSerializer : public TMapSubobjectSerializer<FDisplayClusterConfigurationClusterNodeSerializer>
	{
		static UClass* GetSupportedClass();
		static void MarkPropertiesAsExplicitlyUnsupported(ILevelSnapshotsModule& Module);

	public:
	
		static void Register(ILevelSnapshotsModule& Module);
	
		//~ Begin TMapSubobjectSerializer Interface
		static const FMapProperty* GetMapProperty();
		//~ End TMapSubobjectSerializer Interface
	};
}