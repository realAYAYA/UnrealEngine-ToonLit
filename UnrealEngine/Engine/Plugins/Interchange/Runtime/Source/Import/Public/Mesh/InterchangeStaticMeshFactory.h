// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeFactoryBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Mesh/InterchangeStaticMeshPayload.h"

#include "InterchangeStaticMeshFactory.generated.h"


class UBodySetup;
class UStaticMesh;
class UInterchangeStaticMeshFactoryNode;
class UInterchangeStaticMeshLodDataNode;
struct FMeshDescription;
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
	virtual UObject* CreateEmptyAsset(const FCreateAssetParams& Arguments) override;
	virtual UObject* CreateAsset(const FCreateAssetParams& Arguments) override;
	virtual void PreImportPreCompletedCallback(const FImportPreCompletedCallbackParams& Arguments) override;
	virtual bool GetSourceFilenames(const UObject* Object, TArray<FString>& OutSourceFilenames) const override;
	virtual bool SetSourceFilename(const UObject* Object, const FString& SourceFilename, int32 SourceIndex) const override;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////

private:

	struct FMeshPayload
	{
		FString MeshName;
		TFuture<TOptional<UE::Interchange::FStaticMeshPayloadData>> PayloadData;
		FTransform Transform = FTransform::Identity;
	};

	TArray<FMeshPayload> GetMeshPayloads(const FCreateAssetParams& Arguments, const TArray<FString>& MeshUids) const;

	bool AddConvexGeomFromVertices(const FCreateAssetParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom);
	bool DecomposeConvexMesh(const FCreateAssetParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, UBodySetup* BodySetup);
	bool AddBoxGeomFromTris(const FCreateAssetParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom);
	bool AddSphereGeomFromVertices(const FCreateAssetParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom);
	bool AddCapsuleGeomFromVertices(const FCreateAssetParams& Arguments, const FMeshDescription& MeshDescription, const FTransform& Transform, FKAggregateGeom& AggGeom);

	bool ImportBoxCollision(const FCreateAssetParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode);
	bool ImportCapsuleCollision(const FCreateAssetParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode);
	bool ImportSphereCollision(const FCreateAssetParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode);
	bool ImportConvexCollision(const FCreateAssetParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshLodDataNode* LodDataNode);
	bool GenerateKDopCollision(const FCreateAssetParams& Arguments, UStaticMesh* StaticMesh);
	bool ImportSockets(const FCreateAssetParams& Arguments, UStaticMesh* StaticMesh, const UInterchangeStaticMeshFactoryNode* FactoryNode);

	void CommitMeshDescriptions(UStaticMesh& StaticMesh, TArray<FMeshDescription>&& LodMeshDescriptions);

#if WITH_EDITORONLY_DATA
	void SetupSourceModelsSettings(UStaticMesh& StaticMesh, const TArray<FMeshDescription>& LodMeshDescriptions, int32 PreviousLodCount, int32 FinalLodCount, bool bIsAReimport);
#endif
};
