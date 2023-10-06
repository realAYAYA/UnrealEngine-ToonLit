// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationXPBDAnisoBendingConfigNode.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationXPBDAnisoBendingConfigNode)

FChaosClothAssetSimulationXPBDAnisoBendingConfigNode::FChaosClothAssetSimulationXPBDAnisoBendingConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&XPBDAnisoFlatnessRatio.WeightMap);
	RegisterInputConnection(&XPBDAnisoRestAngle.WeightMap);
	RegisterInputConnection(&XPBDAnisoBendingStiffnessWarp.WeightMap);
	RegisterInputConnection(&XPBDAnisoBendingStiffnessWeft.WeightMap);
	RegisterInputConnection(&XPBDAnisoBendingStiffnessBias.WeightMap);
	RegisterInputConnection(&XPBDAnisoBendingDamping.WeightMap);
	RegisterInputConnection(&XPBDAnisoBucklingStiffnessWarp.WeightMap);
	RegisterInputConnection(&XPBDAnisoBucklingStiffnessWeft.WeightMap);
	RegisterInputConnection(&XPBDAnisoBucklingStiffnessBias.WeightMap);
}

void FChaosClothAssetSimulationXPBDAnisoBendingConfigNode::AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
{
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYENUMCHECKED2(
		XPBDAnisoRestAngleType,
		XPBDRestAngleType,  // Existing properties to warn against
		RestAngleType);     //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		XPBDAnisoFlatnessRatio,
		XPBDFlatnessRatio,  // Existing properties to warn against
		FlatnessRatio);     //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		XPBDAnisoRestAngle,
		XPBDRestAngle,  // Existing properties to warn against
		RestAngle);     //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED4(
		XPBDAnisoBendingStiffnessWarp,
		BendingSpringStiffness,        // Existing properties to warn against
		BendingElementStiffness,       //
		XPBDBendingSpringStiffness,    //
		XPBDBendingElementStiffness);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoBendingStiffnessWeft);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoBendingStiffnessBias);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		XPBDAnisoBendingDamping,
		XPBDBendingSpringDamping,    // Existing properties to warn against
		XPBDBendingElementDamping);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYCHECKED2(
		XPBDAnisoBucklingRatio,
		BucklingRatio,       // Existing properties to warn against
		XPBDBucklingRatio);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		XPBDAnisoBucklingStiffnessWarp,
		BucklingStiffness,       // Existing properties to warn against
		XPBDBucklingStiffness);  //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoBucklingStiffnessWeft);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoBucklingStiffnessBias);
}
