// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationXPBDAnisoStretchConfigNode.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationXPBDAnisoStretchConfigNode)

FChaosClothAssetSimulationXPBDAnisoStretchConfigNode::FChaosClothAssetSimulationXPBDAnisoStretchConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&XPBDAnisoStretchStiffnessWarp.WeightMap);
	RegisterInputConnection(&XPBDAnisoStretchStiffnessWeft.WeightMap);
	RegisterInputConnection(&XPBDAnisoStretchStiffnessBias.WeightMap);
	RegisterInputConnection(&XPBDAnisoStretchDamping.WeightMap);
	RegisterInputConnection(&XPBDAnisoStretchWarpScale.WeightMap);
	RegisterInputConnection(&XPBDAnisoStretchWeftScale.WeightMap);
}

void FChaosClothAssetSimulationXPBDAnisoStretchConfigNode::AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
{
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYBOOL(XPBDAnisoStretchUse3dRestLengths);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED2(
		XPBDAnisoStretchStiffnessWarp,
		EdgeSpringStiffness,           // Existing properties to warn against
		XPBDEdgeSpringStiffness);      //
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoStretchStiffnessWeft);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoStretchStiffnessBias);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTEDCHECKED1(
		XPBDAnisoStretchDamping,
		XPBDEdgeSpringDamping);  // Existing properties to warn against
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoStretchWarpScale);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoStretchWeftScale);
}
