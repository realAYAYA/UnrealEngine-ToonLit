// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationMassConfigNode.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationMassConfigNode)

FChaosClothAssetSimulationMassConfigNode::FChaosClothAssetSimulationMassConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationMassConfigNode::AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
{
	float MassValue;
	switch (MassMode)
	{
	default:
	case EClothMassMode::UniformMass: MassValue = UniformMass; break;
	case EClothMassMode::TotalMass: MassValue = TotalMass; break;
	case EClothMassMode::Density: MassValue = Density; break;
	}

	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYENUM(MassMode);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTY(MassValue);
}
