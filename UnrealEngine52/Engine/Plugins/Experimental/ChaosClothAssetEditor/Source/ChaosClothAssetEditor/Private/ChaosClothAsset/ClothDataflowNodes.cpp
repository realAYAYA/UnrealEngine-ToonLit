// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothDataflowNodes.h"
#include "Logging/LogMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothDataflowNodes)

#define LOCTEXT_NAMESPACE "ClothDataflowNodes"

namespace Dataflow
{
	void RegisterClothDataflowNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FClothAssetTerminalDataflowNode);
	}
}

#undef LOCTEXT_NAMESPACE
