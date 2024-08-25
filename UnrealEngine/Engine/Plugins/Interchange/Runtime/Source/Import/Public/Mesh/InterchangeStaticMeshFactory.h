// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeFactoryBase.h"
#include "MeshDescription.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Mesh/InterchangeMeshPayload.h"

#include "InterchangeStaticMeshFactory.generated.h"


class UBodySetup;
class UStaticMesh;
class UInterchangeStaticMeshFactoryNode;
class UInterchangeStaticMeshLodDataNode;
struct FKAggregateGeom;


UCLASS(BlueprintType, Experimental)
class INTERCHANGEIMPORT_API UInterchangeStaticMeshFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()
public:

	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;
	virtual EInterchangeFactoryAssetType GetFactoryAssetType() override { return EInterchangeFactoryAssetType::Meshes; }
	virtual FImportAssetResult BeginImportAsset_GameThread(const FImportAssetObjectParams& Arguments) override;
	virtual FImportAssetResult ImportAsset_Async(const FImportAssetObjectParams& Arguments) override;
	virtual FImportAssetResult EndImportAsset_GameThread(const FImportAssetObjectParams& Arguments) override;
	virtual void SetupObject_GameThread(const FSetupObjectParams& Arguments) override;
	virtual bool GetSourceFilenames(const UObject* Object, TArray<FString>& OutSourceFilenames) const override;
	virtual bool SetSourceFilename(const UObject* Object, const FString& SourceFilename, int32 SourceIndex) const override;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////

private:

	struct FMeshPayload
	{
		FString MeshName;
		TFuture<TOptional<UE::Interchange::FMeshPayloadData>> PayloadData;
		FTransform Transform = FTransform::Identity;
	};

	TArray<FMeshPayload> GetMeshPayloads(const FImportAssetObjectParams& Arguments, const TArray<FString>& MeshUids) const;

	bool AddConvexGeomFromVertices(const FImportAssetObjectParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom);
	bool DecomposeConvexMesh(const FImportAssetObjectParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, UBodySetup* BodySetup);
	bool AddBoxGeomFromTris(const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom);
	bool AddSphereGeomFromVertices(const FImportAssetObjectParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom);
	bool AddCapsuleGeomFromVertices(const FImportAssetObjectParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom);

	bool ImportBoxCollision(const FImportAssetObjectParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode);
	bool ImportCapsuleCollision(const FImportAssetObjectParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode);
	bool ImportSphereCollision(const FImportAssetObjectParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode);
	bool ImportConvexCollision(const FImportAssetObjectParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode);
	bool GenerateKDopCollision(UStaticMesh* StaticMesh);
	bool ImportSockets(const FImportAssetObjectParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshFactoryNode* FactoryNode);

	void CommitMeshDescriptions(UStaticMesh& StaticMesh);
	void BuildFromMeshDescriptions(UStaticMesh& StaticMesh);

#if WITH_EDITORONLY_DATA
	void SetupSourceModelsSettings(UStaticMesh& StaticMesh, const TArray<FMeshDescription>& LodMeshDescriptions, int32 PreviousLodCount, int32 FinalLodCount, bool bIsAReimport);
#endif
	struct FImportAssetObjectData
	{
		bool bImportedCustomCollision = false;
		bool bImportCollision = false;
		TArray<FMeshDescription> LodMeshDescriptions;
	};
	FImportAssetObjectData ImportAssetObjectData;
};
