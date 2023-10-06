// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationXPBDBendingSpringConfigNode.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationXPBDBendingSpringConfigNode)

FChaosClothAssetSimulationXPBDBendingSpringConfigNode::FChaosClothAssetSimulationXPBDBendingSpringConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&XPBDBendingSpringStiffness.WeightMap);
	RegisterInputConnection(&XPBDBendingSpringDamping.WeightMap);
}

void FChaosClothAssetSimulationXPBDBendingSpringConfigNode::AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
{
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED4(
		XPBDBendingSpringStiffness,
		BendingSpringStiffness,          // Existing properties to warn against
		BendingElementStiffness,         //
		XPBDBendingElementStiffness,     //
		XPBDAnisoBendingStiffnessWarp);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		XPBDBendingSpringDamping,
		XPBDBendingElementDamping,  // Existing properties to warn against
		XPBDAnisoBendingDamping);   //
}
