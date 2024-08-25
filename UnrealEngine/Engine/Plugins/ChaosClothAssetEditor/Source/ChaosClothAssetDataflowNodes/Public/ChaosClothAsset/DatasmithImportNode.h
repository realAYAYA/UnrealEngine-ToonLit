// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Misc/SecureHash.h"
#include "DatasmithCloth.h"
#include "DatasmithImportNode.generated.h"

/** Cloth asset factory providing and initializing cloth assets on behalf of the Datasmith importer. */
UCLASS()
class UChaosClothAssetDatasmithClothAssetFactory final : public UDatasmithClothAssetFactory
{
	GENERATED_BODY()

public:
	UChaosClothAssetDatasmithClothAssetFactory();
	virtual ~UChaosClothAssetDatasmithClothAssetFactory() override;

	virtual UObject* CreateClothAsset(UObject* Outer, const FName& Name, EObjectFlags ObjectFlags) const override;
	virtual UObject* DuplicateClothAsset(UObject* ClothAsset, UObject* Outer, const FName& Name) const override;
	virtual void InitializeClothAsset(UObject* ClothAsset, const FDatasmithCloth& DatasmithCloth) const override;
};


/** Cloth component factory providing and initializing cloth components on behalf of the Datasmith importer. */
UCLASS()
class UChaosClothAssetDatasmithClothComponentFactory final : public UDatasmithClothComponentFactory
{
	GENERATED_BODY()

public:
	UChaosClothAssetDatasmithClothComponentFactory();
	virtual ~UChaosClothAssetDatasmithClothComponentFactory() override;

	virtual USceneComponent* CreateClothComponent(UObject* Outer) const override;
	virtual void InitializeClothComponent(class USceneComponent* ClothComponent, UObject* ClothAsset, class USceneComponent* RootComponent) const override;
};

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

	/** Register the Datasmith cloth factory classes provider. */
	static void RegisterModularFeature();
	/** Unregister the Datasmith cloth factory classes provider. */
	static void UnregisterModularFeature();

	FChaosClothAssetDatasmithImportNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode interface
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	virtual void Serialize(FArchive& Archive) override;
	virtual bool IsDeprecated() override { return true; }
	//~ End FDataflowNode interface

	bool EvaluateImpl(Dataflow::FContext& Context, FManagedArrayCollection& OutCollection) const;

	mutable FManagedArrayCollection ImportCache;
	mutable FMD5Hash ImportHash;
};
