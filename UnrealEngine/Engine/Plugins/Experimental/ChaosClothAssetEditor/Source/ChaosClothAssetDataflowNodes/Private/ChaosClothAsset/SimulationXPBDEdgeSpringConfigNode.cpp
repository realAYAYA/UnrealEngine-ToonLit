// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationXPBDEdgeSpringConfigNode.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationXPBDEdgeSpringConfigNode)

FChaosClothAssetSimulationXPBDEdgeSpringConfigNode::FChaosClothAssetSimulationXPBDEdgeSpringConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&XPBDEdgeSpringStiffness.WeightMap);
	RegisterInputConnection(&XPBDEdgeSpringDamping.WeightMap);
}

void FChaosClothAssetSimulationXPBDEdgeSpringConfigNode::AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
{
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		XPBDEdgeSpringStiffness,
		EdgeSpringStiffness,             // Existing properties to warn against
		XPBDAnisoStretchStiffnessWarp);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED1(
		XPBDEdgeSpringDamping,
		XPBDAnisoStretchDamping);  // Existing properties to warn against
}
