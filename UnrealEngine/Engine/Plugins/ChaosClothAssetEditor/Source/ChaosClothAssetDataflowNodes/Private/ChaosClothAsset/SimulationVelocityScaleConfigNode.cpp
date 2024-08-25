// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationVelocityScaleConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationVelocityScaleConfigNode)

FChaosClothAssetSimulationVelocityScaleConfigNode::FChaosClothAssetSimulationVelocityScaleConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationVelocityScaleConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	
	PropertyHelper.SetProperty(this, &LinearVelocityScale);
	PropertyHelper.SetProperty(this, &AngularVelocityScale);
	PropertyHelper.SetProperty(this, &MaxVelocityScale);
	PropertyHelper.SetProperty(this, &FictitiousAngularScale);
}
