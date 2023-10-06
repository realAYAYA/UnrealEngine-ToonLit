// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationVelocityScaleConfigNode.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationVelocityScaleConfigNode)

FChaosClothAssetSimulationVelocityScaleConfigNode::FChaosClothAssetSimulationVelocityScaleConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationVelocityScaleConfigNode::AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
{
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTY(LinearVelocityScale);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTY(AngularVelocityScale);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTY(FictitiousAngularScale);
}
