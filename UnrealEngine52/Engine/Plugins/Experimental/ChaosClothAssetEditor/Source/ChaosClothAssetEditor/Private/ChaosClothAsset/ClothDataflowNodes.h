// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowCore.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothCollection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "ClothDataflowNodes.generated.h"

USTRUCT()
struct FClothAssetTerminalDataflowNode : public FDataflowTerminalNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FClothAssetTerminalDataflowNode, "ClothAssetTerminal", "Cloth", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:

	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;		// TODO: Replace with FClothCollection

	FClothAssetTerminalDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowTerminalNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const override
	{
		if (UChaosClothAsset* ClothAsset = Cast<UChaosClothAsset>(Asset.Get()))
		{
			// TODO: Modify ClothAsset directly
		}
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue<FManagedArrayCollection>(Context, InCollection, &Collection);
	}
};

namespace Dataflow
{
	void RegisterClothDataflowNodes();
}

