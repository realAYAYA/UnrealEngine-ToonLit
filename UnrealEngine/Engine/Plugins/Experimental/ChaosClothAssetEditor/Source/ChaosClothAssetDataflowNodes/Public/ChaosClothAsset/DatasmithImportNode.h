// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Misc/SecureHash.h"
#include "DatasmithImportNode.generated.h"

/** Import a file from a third party garment construction package compatible with the Datasmith scene format. */
USTRUCT(meta = (DataflowCloth))
struct FChaosClothAssetDatasmithImportNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetDatasmithImportNode, "DatasmithImport", "Cloth", "Cloth Datasmith Import")

public:
	UPROPERTY(meta = (DataflowOutput))
	FManagedArrayCollection Collection;

	/** Path of the file to import using any available Datasmith cloth translator. */
	UPROPERTY(EditAnywhere, Category = "Datasmith Import")
	FFilePath ImportFile;

	FChaosClothAssetDatasmithImportNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode interface
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	virtual void Serialize(FArchive& Archive) override;
	//~ End FDataflowNode interface

	bool EvaluateImpl(Dataflow::FContext& Context, FManagedArrayCollection& OutCollection) const;

	mutable FManagedArrayCollection ImportCache;
	mutable FMD5Hash ImportHash;
};
