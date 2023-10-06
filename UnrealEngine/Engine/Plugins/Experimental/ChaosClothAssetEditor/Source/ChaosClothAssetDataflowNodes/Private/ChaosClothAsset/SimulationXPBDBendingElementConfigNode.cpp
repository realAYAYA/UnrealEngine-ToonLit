// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationXPBDBendingElementConfigNode.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationXPBDBendingElementConfigNode)

FChaosClothAssetSimulationXPBDBendingElementConfigNode::FChaosClothAssetSimulationXPBDBendingElementConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&XPBDFlatnessRatio.WeightMap);
	RegisterInputConnection(&XPBDRestAngle.WeightMap);
	RegisterInputConnection(&XPBDBendingElementStiffness.WeightMap);
	RegisterInputConnection(&XPBDBendingElementDamping.WeightMap);
	RegisterInputConnection(&XPBDBucklingStiffness.WeightMap);
}

void FChaosClothAssetSimulationXPBDBendingElementConfigNode::AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
{
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYENUMCHECKED2(
		XPBDRestAngleType,
		RestAngleType,            // Existing properties to warn against
		XPBDAnisoRestAngleType);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		XPBDFlatnessRatio,
		FlatnessRatio,            // Existing properties to warn against
		XPBDAnisoFlatnessRatio);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		XPBDRestAngle,
		RestAngle,            // Existing properties to warn against
		XPBDAnisoRestAngle);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED4(
		XPBDBendingElementStiffness,
		BendingSpringStiffness,          // Existing properties to warn against
		BendingElementStiffness,         //
		XPBDBendingSpringStiffness,      //
		XPBDAnisoBendingStiffnessWarp);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		XPBDBendingElementDamping,
		XPBDBendingSpringDamping,  // Existing properties to warn against
		XPBDAnisoBendingDamping);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYCHECKED2(
		XPBDBucklingRatio,
		BucklingRatio,            // Existing properties to warn against
		XPBDAnisoBucklingRatio);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		XPBDBucklingStiffness,
		BucklingStiffness,                // Existing properties to warn against
		XPBDAnisoBucklingStiffnessWarp);  //
}
