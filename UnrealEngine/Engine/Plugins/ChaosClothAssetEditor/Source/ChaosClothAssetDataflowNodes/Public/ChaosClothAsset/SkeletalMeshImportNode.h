// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "SkeletalMeshImportNode.generated.h"

class USkeletalMesh;

/** Import a skeletal mesh asset into the cloth collection simulation and/or render mesh containers. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSkeletalMeshImportNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSkeletalMeshImportNode, "SkeletalMeshImport", "Cloth", "Cloth Skeletal Mesh Import")

public:
	UPROPERTY(Meta = (DataflowOutput))
	FManagedArrayCollection Collection;

	/** The skeletal mesh to import. */
	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh Import")
	TObjectPtr<const USkeletalMesh> SkeletalMesh;

	/** The skeletal mesh LOD to import. */
	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh Import", Meta = (ClampMin = "0", DisplayName = "LOD Index"))
	int32 LODIndex = 0;

	/** The skeletal mesh LOD section to import. */
	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh Import", Meta = (ClampMin = "0"))
	int32 SectionIndex = 0;

	/** Whether to import the simulation mesh from the specified skeletal mesh. */
	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh Import")
	bool bImportSimMesh = true;

	/** Whether to import the render mesh from the specified skeletal mesh. */
	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh Import")
	bool bImportRenderMesh = true;

	/**
	 * UV channel of the skeletal mesh to import the 2D simulation mesh patterns from.
	 * If set to -1, then the import will unwrap the 3D simulation mesh into 2D simulation mesh patterns.
	 */
	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh Import", Meta = (ClampMin = "-1", EditCondition = "bImportSimMesh"))
	int32 UVChannel = 0;

	/* Apply this scale to the UVs when populating Sim Mesh positions. */
	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh Import", Meta = (AllowPreserveRatio, EditCondition = "bImportSimMesh && UVChannel != INDEX_NONE"))
	FVector2f UVScale = { 1.f, 1.f };

	FChaosClothAssetSkeletalMeshImportNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};
