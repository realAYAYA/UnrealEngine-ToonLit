// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "ChaosClothAsset/ImportFilePath.h"
#include "Dataflow/DataflowTerminalNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Misc/SecureHash.h"
#include "USDImportNode.generated.h"

/** Import a USD file from a third party garment construction software. */
USTRUCT(meta = (DataflowCloth))
struct FChaosClothAssetUSDImportNode : public FDataflowTerminalNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetUSDImportNode, "USDImport", "Cloth", "Cloth USD Import")

public:
	UPROPERTY(meta = (DataflowOutput))
	FManagedArrayCollection Collection;

	/** Name of the USD file to import. */
	UPROPERTY(EditAnywhere, Category = "USD Import", Meta = (DisplayName = "USD File"))
	FChaosClothAssetImportFilePath UsdFile;

	FChaosClothAssetUSDImportNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode interface
	virtual void SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const override;
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	virtual void Serialize(FArchive& Archive) override;
	//~ End FDataflowNode interface

	bool ImportFromFile(const FString& UsdPath, const FString& AssetPath, class FText& OutErrorText);
	bool ImportFromCache(const TSharedRef<FManagedArrayCollection>& ClothCollection, class FText& OutErrorText) const;

	UPROPERTY(VisibleAnywhere, Category = "USD Import")
	FString PackagePath;  // Content folder where all the USD asset are imported

	FMD5Hash FileHash;
	FManagedArrayCollection CollectionCache;  // Content cache for data that hasn't got a USD schema yet
};
