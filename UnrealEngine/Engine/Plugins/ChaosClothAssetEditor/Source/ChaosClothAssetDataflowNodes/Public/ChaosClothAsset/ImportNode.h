// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "ImportNode.generated.h"

class UChaosClothAsset;

/** Refresh structure for push button customization. */
USTRUCT()
struct FChaosClothAssetImportNodeRefreshAsset
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Import Node Refresh Asset")
	bool bRefreshAsset = false;
};

/** Import an existing Cloth Asset into the graph. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetImportNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetImportNode, "ClothAssetImport", "Cloth", "Cloth Asset Import")

public:
	UPROPERTY(Meta = (DataflowOutput))
	FManagedArrayCollection Collection;

	/** The Cloth Asset to import into a collection. */
	UPROPERTY(EditAnywhere, Category = "Cloth Asset Import")
	TObjectPtr<const UChaosClothAsset> ClothAsset;

	/** The LOD to import into the collection. Only one LOD can be imported at a time. */
	UPROPERTY(EditAnywhere, Category = "Cloth Asset Import", Meta = (DisplayName = "Import LOD", ClampMin = "0"))
	int32 ImportLod = 0;

	/**
	 * Reimport the imported asset. 
	 */
	UPROPERTY(EditAnywhere, Category = "Cloth Asset Import")
	mutable FChaosClothAssetImportNodeRefreshAsset ReimportAsset;

	FChaosClothAssetImportNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};
