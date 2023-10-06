// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationPBDBendingSpringConfigNode.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationPBDBendingSpringConfigNode)

FChaosClothAssetSimulationPBDBendingSpringConfigNode::FChaosClothAssetSimulationPBDBendingSpringConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&BendingSpringStiffness.WeightMap);
}

void FChaosClothAssetSimulationPBDBendingSpringConfigNode::AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
{
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED4(
		BendingSpringStiffness,
		BendingElementStiffness,         // Existing properties to warn against
		XPBDBendingSpringStiffness,      //
		XPBDBendingElementStiffness,     //
		XPBDAnisoBendingStiffnessWarp);  //
}
