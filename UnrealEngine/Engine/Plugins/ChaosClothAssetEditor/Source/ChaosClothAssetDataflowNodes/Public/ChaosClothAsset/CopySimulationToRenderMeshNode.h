// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "CopySimulationToRenderMeshNode.generated.h"

class UMaterialInterface;

/** Copy the simulation mesh to the render mesh to be able to render the simulation mesh, or when not using a different mesh for rendering. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetCopySimulationToRenderMeshNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetCopySimulationToRenderMeshNode, "CopySimulationToRenderMesh", "Cloth", "Cloth Simulation Render Mesh")

public:

	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** New material for the render mesh. */
	UPROPERTY(EditAnywhere, Category = "Copy Simulation To Render Mesh")
	TObjectPtr<const UMaterialInterface> Material;

	/** Generate a single render pattern rather than a render pattern per sim pattern. */
	UPROPERTY(EditAnywhere, Category = "Copy Simulation To Render Mesh")
	bool bGenerateSingleRenderPattern = true;

	FChaosClothAssetCopySimulationToRenderMeshNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};
