// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationCollisionConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationCollisionConfigNode)

FChaosClothAssetSimulationCollisionConfigNode::FChaosClothAssetSimulationCollisionConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationCollisionConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyBool(this, &bUseCCD);
	PropertyHelper.SetProperty(this, &ProximityStiffness);
	
	PropertyHelper.SetFabricProperty(FName(TEXT("CollisionThickness")), CollisionThicknessImported,
		[](UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		{
			return FabricFacade.GetCollisionThickness();
		}, {});

	PropertyHelper.SetFabricProperty(FName(TEXT("FrictionCoefficient")), FrictionCoefficientImported,
		[](UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		{
			return FabricFacade.GetFriction();
		}, {});
}

void FChaosClothAssetSimulationCollisionConfigNode::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading())
	{
#if WITH_EDITORONLY_DATA
		if (FrictionCoefficient_DEPRECATED != UE::Chaos::ClothAsset::FDefaultFabric::Friction)
		{
			FrictionCoefficientImported.ImportedValue = FrictionCoefficient_DEPRECATED;
			FrictionCoefficient_DEPRECATED = UE::Chaos::ClothAsset::FDefaultFabric::Friction;
		}
		if (CollisionThickness_DEPRECATED != UE::Chaos::ClothAsset::FDefaultFabric::CollisionThickness)
		{
			CollisionThicknessImported.ImportedValue = CollisionThickness_DEPRECATED;
			CollisionThickness_DEPRECATED = UE::Chaos::ClothAsset::FDefaultFabric::CollisionThickness;
		}
#endif
	}
}

