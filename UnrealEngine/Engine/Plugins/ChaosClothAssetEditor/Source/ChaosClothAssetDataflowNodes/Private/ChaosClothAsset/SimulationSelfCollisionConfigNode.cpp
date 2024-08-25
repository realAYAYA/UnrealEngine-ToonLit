// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationSelfCollisionConfigNode.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationSelfCollisionConfigNode)

FChaosClothAssetSimulationSelfCollisionConfigNode::FChaosClothAssetSimulationSelfCollisionConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&SelfCollisionLayers.StringValue, GET_MEMBER_NAME_CHECKED(FChaosClothAssetConnectableIStringValue, StringValue));
	RegisterInputConnection(&SelfCollisionDisabledFaces.StringValue, GET_MEMBER_NAME_CHECKED(FChaosClothAssetConnectableIStringValue, StringValue));
	RegisterInputConnection(&SelfCollisionEnabledKinematicFaces.StringValue, GET_MEMBER_NAME_CHECKED(FChaosClothAssetConnectableIStringValue, StringValue));
	RegisterInputConnection(&SelfCollisionKinematicColliderFrictionWeighted.WeightMap);
	RegisterInputConnection(&SelfCollisionThicknessWeighted.WeightMap);
}

void FChaosClothAssetSimulationSelfCollisionConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyBool(FName("UseSelfCollisions"), true);
	PropertyHelper.SetProperty(this, &SelfCollisionStiffness);
	PropertyHelper.SetProperty(this, &SelfCollisionDisableNeighborDistance, {}, ECollectionPropertyFlags::None); // Non animatable
	PropertyHelper.SetPropertyString(this, &SelfCollisionDisabledFaces);
	PropertyHelper.SetPropertyBool(this, &bSelfCollideAgainstKinematicCollidersOnly);
	PropertyHelper.SetPropertyBool(this, &bSelfCollideAgainstAllKinematicVertices);
	PropertyHelper.SetPropertyString(this, &SelfCollisionEnabledKinematicFaces);
	PropertyHelper.SetProperty(this, &SelfCollisionKinematicColliderThickness);
	PropertyHelper.SetProperty(this, &SelfCollisionKinematicColliderStiffness);
	PropertyHelper.SetPropertyWeighted(TEXT("SelfCollisionKinematicColliderFriction"), SelfCollisionKinematicColliderFrictionWeighted);

	PropertyHelper.SetPropertyBool(this, &bUseSelfIntersections);
	PropertyHelper.SetPropertyBool(this, &bUseGlobalIntersectionAnalysis);
	PropertyHelper.SetPropertyBool(this, &bUseContourMinimization);
	PropertyHelper.SetProperty(this, &NumContourMinimizationPostSteps);
	PropertyHelper.SetPropertyBool(this, &bUseGlobalPostStepContours);
	PropertyHelper.SetProperty(this, &SelfCollisionProximityStiffness);

	PropertyHelper.SetFabricPropertyString<int32,FChaosClothAssetConnectableIStringValue>(FName(TEXT("SelfCollisionLayers")), SelfCollisionLayers,
		[](const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		{
			return FabricFacade.GetLayer();
		}, {}, ECollectionPropertyFlags::None, UE::Chaos::ClothAsset::ClothCollectionGroup::SimFaces);
	
	PropertyHelper.SetFabricPropertyWeighted(FName(TEXT("SelfCollisionThickness")), SelfCollisionThicknessWeighted,
		[](const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		{
			return FabricFacade.GetCollisionThickness();
		}, {});

	PropertyHelper.SetFabricProperty(FName(TEXT("SelfCollisionFriction")), SelfCollisionFrictionImported,
		[](UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		{
			return FabricFacade.GetFriction();
		}, {});
}

void FChaosClothAssetSimulationSelfCollisionConfigNode::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading())
	{
#if WITH_EDITORONLY_DATA
		if (SelfCollisionFriction_DEPRECATED != UE::Chaos::ClothAsset::FDefaultFabric::SelfFriction)
		{
			SelfCollisionFrictionImported.ImportedValue = SelfCollisionFriction_DEPRECATED;
			SelfCollisionFriction_DEPRECATED = UE::Chaos::ClothAsset::FDefaultFabric::SelfFriction;
		}
		if (SelfCollisionKinematicColliderFriction_DEPRECATED != FrictionDeprecatedValue)
		{
			SelfCollisionKinematicColliderFrictionWeighted.Low = SelfCollisionKinematicColliderFrictionWeighted.High = SelfCollisionKinematicColliderFriction_DEPRECATED;
			SelfCollisionKinematicColliderFriction_DEPRECATED = FrictionDeprecatedValue;
		}
		if (SelfCollisionThickness_DEPRECATED != SelfCollisionThicknessDeprecatedValue)
		{
			SelfCollisionThicknessWeighted.Low = SelfCollisionThicknessWeighted.High = SelfCollisionThickness_DEPRECATED;
			SelfCollisionThickness_DEPRECATED = SelfCollisionThicknessDeprecatedValue;
		}
#endif
	}
}