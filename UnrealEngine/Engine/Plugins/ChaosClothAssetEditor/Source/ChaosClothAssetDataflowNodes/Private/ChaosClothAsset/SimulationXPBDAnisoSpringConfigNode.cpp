// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationXPBDAnisoSpringConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationXPBDAnisoSpringConfigNode)

FChaosClothAssetSimulationXPBDAnisoSpringConfigNode::FChaosClothAssetSimulationXPBDAnisoSpringConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&XPBDAnisoSpringStiffnessWarp.WeightMap);
	RegisterInputConnection(&XPBDAnisoSpringStiffnessWeft.WeightMap);
	RegisterInputConnection(&XPBDAnisoSpringStiffnessBias.WeightMap);
	RegisterInputConnection(&XPBDAnisoSpringDamping.WeightMap);
	RegisterInputConnection(&XPBDAnisoSpringWarpScale.WeightMap);
	RegisterInputConnection(&XPBDAnisoSpringWeftScale.WeightMap);
}

void FChaosClothAssetSimulationXPBDAnisoSpringConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyBool(this, &bXPBDAnisoSpringUse3dRestLengths, {}, ECollectionPropertyFlags::None);  // Non animatable
	PropertyHelper.SetPropertyWeighted(this, &XPBDAnisoSpringStiffnessWarp);
	PropertyHelper.SetPropertyWeighted(this, &XPBDAnisoSpringStiffnessWeft);
	PropertyHelper.SetPropertyWeighted(this, &XPBDAnisoSpringStiffnessBias);
	PropertyHelper.SetPropertyWeighted(this, &XPBDAnisoSpringDamping);
	PropertyHelper.SetPropertyWeighted(this, &XPBDAnisoSpringWarpScale);
	PropertyHelper.SetPropertyWeighted(this, &XPBDAnisoSpringWeftScale);
}
