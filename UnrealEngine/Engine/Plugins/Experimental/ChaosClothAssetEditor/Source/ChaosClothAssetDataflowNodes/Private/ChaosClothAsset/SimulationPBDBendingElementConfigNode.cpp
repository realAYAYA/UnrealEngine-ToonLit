// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationPBDBendingElementConfigNode.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationPBDBendingElementConfigNode)

FChaosClothAssetSimulationPBDBendingElementConfigNode::FChaosClothAssetSimulationPBDBendingElementConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&FlatnessRatio.WeightMap);
	RegisterInputConnection(&RestAngle.WeightMap);
	RegisterInputConnection(&BendingElementStiffness.WeightMap);
	RegisterInputConnection(&BucklingStiffness.WeightMap);
}

void FChaosClothAssetSimulationPBDBendingElementConfigNode::AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
{
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYENUMCHECKED2(
		RestAngleType,
		XPBDRestAngleType,        // Existing properties to warn against
		XPBDAnisoRestAngleType);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		FlatnessRatio,
		XPBDFlatnessRatio,        // Existing properties to warn against
		XPBDAnisoFlatnessRatio);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		RestAngle,
		XPBDRestAngle,        // Existing properties to warn against
		XPBDAnisoRestAngle);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED4(
		BendingElementStiffness,
		BendingSpringStiffness,          // Existing properties to warn against
		XPBDBendingSpringStiffness,      //
		XPBDBendingElementStiffness,     //
		XPBDAnisoBendingStiffnessWarp);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYCHECKED2(
		BucklingRatio,
		XPBDBucklingRatio,        // Existing properties to warn against
		XPBDAnisoBucklingRatio);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		BucklingStiffness,
		XPBDBucklingStiffness,            // Existing properties to warn against
		XPBDAnisoBucklingStiffnessWarp);  //
}
