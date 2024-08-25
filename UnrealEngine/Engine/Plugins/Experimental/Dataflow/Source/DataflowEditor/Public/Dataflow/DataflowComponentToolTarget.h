// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TargetInterfaces/DynamicMeshCommitter.h"
#include "TargetInterfaces/DynamicMeshProvider.h"
#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/MeshDescriptionCommitter.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "ToolTargets/PrimitiveComponentToolTarget.h"

#include "DataflowComponentToolTarget.generated.h"

/**
 * A tool target backed by a read-only dataflow component that can provide and take a mesh
 * description.
 */
UCLASS(Transient)
class DATAFLOWEDITOR_API UDataflowComponentReadOnlyToolTarget :
	public UPrimitiveComponentToolTarget,
	public IMeshDescriptionProvider,
	public IDynamicMeshProvider, 
	public IMaterialProvider
{
	GENERATED_BODY()

public:

	// UToolTarget implementation
	virtual bool IsValid() const override;

	// IMeshDescriptionProvider implementation
	virtual const FMeshDescription* GetMeshDescription(const FGetMeshParameters& GetMeshParams = FGetMeshParameters()) override;
	virtual FMeshDescription GetEmptyMeshDescription() override;

	// IMaterialProvider implementation
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	virtual void GetMaterialSet(FComponentMaterialSet& MaterialSetOut, bool bPreferAssetMaterials) const override;
	virtual bool CommitMaterialSetUpdate(const FComponentMaterialSet& MaterialSet, bool bApplyToAsset) override;

	// IDynamicMeshProvider
	virtual UE::Geometry::FDynamicMesh3 GetDynamicMesh() override;
	virtual UE::Geometry::FDynamicMesh3 GetDynamicMesh(bool bRequestTangents) override;

protected:
	// So that the tool target factory can poke into Component.
	friend class UDataflowComponentReadOnlyToolTargetFactory;

	// Until UDataflow stores its internal representation as FMeshDescription, we need to
	// retain the storage here to cover the lifetime of the pointer returned by GetMeshDescription(). 
	TUniquePtr<FMeshDescription> DataflowMeshDescription;
};

/**
 * A tool target backed by a dataflow component that can provide and take a mesh
 * description.
 */
UCLASS(Transient)
class DATAFLOWEDITOR_API UDataflowComponentToolTarget :
	public UDataflowComponentReadOnlyToolTarget,
	public IMeshDescriptionCommitter,
	public IDynamicMeshCommitter
{
	GENERATED_BODY()

public:
	// IMeshDescriptionCommitter implementation
	virtual void CommitMeshDescription(const FCommitter& Committer, const FCommitMeshParameters& CommitParams = FCommitMeshParameters()) override;
	using IMeshDescriptionCommitter::CommitMeshDescription; // unhide the other overload

	// IDynamicMeshCommitter
	virtual void CommitDynamicMesh(const UE::Geometry::FDynamicMesh3& Mesh, const FDynamicMeshCommitInfo& CommitInfo) override;
	using IDynamicMeshCommitter::CommitDynamicMesh; // unhide the other overload

protected:
	// So that the tool target factory can poke into Component.
	friend class UDataflowComponentToolTargetFactory;
};


/** Factory for UDataflowComponentReadOnlyToolTarget to be used by the target manager. */
UCLASS(Transient)
class DATAFLOWEDITOR_API UDataflowComponentReadOnlyToolTargetFactory : public UToolTargetFactory
{
	GENERATED_BODY()

public:

	// UToolTargetFactory implementation
	virtual bool CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) const override;
	virtual UToolTarget* BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) override;
};


/** Factory for UDataflowComponentToolTarget to be used by the target manager. */
UCLASS(Transient)
class DATAFLOWEDITOR_API UDataflowComponentToolTargetFactory : public UToolTargetFactory
{
	GENERATED_BODY()

public:

	// UToolTargetFactory implementation
	virtual bool CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) const override;
	virtual UToolTarget* BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) override;
};
