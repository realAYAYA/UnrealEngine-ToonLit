// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationDefaultConfigNode.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/ClothEngineTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Chaos/PBDLongRangeConstraints.h"  // For Tether modes
#include "Dataflow/DataflowInputOutput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationDefaultConfigNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetSimulationDefaultConfigNode"

FChaosClothAssetSimulationDefaultConfigNode::FChaosClothAssetSimulationDefaultConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
	, SimulationConfig(NewObject<UChaosClothConfig>())
	, SharedSimulationConfig(NewObject<UChaosClothSharedSimConfig>())
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
}

void FChaosClothAssetSimulationDefaultConfigNode::Serialize(FArchive& Ar)
{
	if (!SimulationConfig)
	{
		SimulationConfig = NewObject<UChaosClothConfig>();
	}
	SimulationConfig->Serialize(Ar);

	if (!SharedSimulationConfig)
	{
		SharedSimulationConfig = NewObject<UChaosClothSharedSimConfig>();
	}
	SharedSimulationConfig->Serialize(Ar);
}

void FChaosClothAssetSimulationDefaultConfigNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace ::Chaos;
	using namespace ::Chaos::Softs;
	using namespace UE::Chaos::ClothAsset;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));

		if (FCollectionClothFacade(ClothCollection).IsValid())  // Can only act on the collection if it is a valid cloth collection
		{
			if (!FCollectionPropertyConstFacade(ClothCollection).IsValid())  // Cannot already have any properties set before this node (it's using CopyTo and wouldn't warn when overriding)
			{
				FClothingSimulationConfig ClothingSimulationConfig;
				ClothingSimulationConfig.Initialize(SimulationConfig.Get(), SharedSimulationConfig.Get());

				ClothingSimulationConfig.GetPropertyCollection(0)->CopyTo(&ClothCollection.Get());

				// Generate tethers
				const FCollectionPropertyConstFacade& Properties = ClothingSimulationConfig.GetProperties(0);
				constexpr bool bUseGeodesicTethersDefault = true;
				const bool bUseGeodesicTethers = Properties.GetValue<bool>(TEXT("UseGeodesicTethers"), bUseGeodesicTethersDefault);
				// Use the "MaxDistance" weight map to generate tethers. This follows legacy behavior.
				static const FName MaxDistanceName(TEXT("MaxDistance"));

				UE::Chaos::ClothAsset::FClothEngineTools::GenerateTethers(ClothCollection, MaxDistanceName, bUseGeodesicTethers);
			}
			else
			{
				FClothDataflowTools::LogAndToastWarning(*this,
					LOCTEXT("CannotOverridePreExistingConfigHeadline", "Cannot override pre-existing config."),
					LOCTEXT("CannotOverridePreExistingConfigDetails", "The Simulation Default Config node cannot be used to override any existing properties, and must be placed ahead of any other config nodes."));
			}
		}

		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
}

void FChaosClothAssetSimulationDefaultConfigNode::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SimulationConfig);
	Collector.AddReferencedObject(SharedSimulationConfig);
}

FString FChaosClothAssetSimulationDefaultConfigNode::GetReferencerName() const
{
	return TEXT("FChaosClothAssetSimulationDefaultConfigNode");
}

#undef LOCTEXT_NAMESPACE
