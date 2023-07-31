// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TargetInterfaces/DynamicMeshCommitter.h"
#include "TargetInterfaces/DynamicMeshProvider.h"
#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/MeshDescriptionCommitter.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/DynamicMeshSource.h"
#include "TargetInterfaces/PhysicsDataSource.h"
#include "ToolTargets/PrimitiveComponentToolTarget.h"

#include "DynamicMeshComponentToolTarget.generated.h"

class UDynamicMesh;

/**
 * A ToolTarget backed by a DynamicMeshComponent
 */
UCLASS(Transient)
class MODELINGCOMPONENTSEDITORONLY_API UDynamicMeshComponentToolTarget : 
	public UPrimitiveComponentToolTarget,
	public IMeshDescriptionCommitter, 
	public IMeshDescriptionProvider, 
	public IDynamicMeshProvider,
	public IDynamicMeshCommitter,
	public IMaterialProvider,
	public IPersistentDynamicMeshSource,
	public IPhysicsDataSource
{
	GENERATED_BODY()

public:
	virtual bool IsValid() const override;


public:
	// IMeshDescriptionProvider implementation
	const FMeshDescription* GetMeshDescription(const FGetMeshParameters& GetMeshParams = FGetMeshParameters()) override;
	virtual FMeshDescription GetEmptyMeshDescription() override;

	// IMeshDescritpionCommitter implementation
	virtual void CommitMeshDescription(const FCommitter& Committer, const FCommitMeshParameters& CommitMeshParams = FCommitMeshParameters()) override;
	using IMeshDescriptionCommitter::CommitMeshDescription; // unhide the other overload

	// IDynamicMeshProvider implementation
	virtual UE::Geometry::FDynamicMesh3 GetDynamicMesh() override;

	// IDynamicMeshCommitter implementation
	virtual void CommitDynamicMesh(const UE::Geometry::FDynamicMesh3& Mesh, const FDynamicMeshCommitInfo& CommitInfo) override;
	using IDynamicMeshCommitter::CommitDynamicMesh; // unhide the other overload

	// IMaterialProvider implementation
	int32 GetNumMaterials() const override;
	UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	void GetMaterialSet(FComponentMaterialSet& MaterialSetOut, bool bPreferAssetMaterials) const override;
	virtual bool CommitMaterialSetUpdate(const FComponentMaterialSet& MaterialSet, bool bApplyToAsset) override;	

	// IPersistentDynamicMeshSource implementation
	virtual UDynamicMesh* GetDynamicMeshContainer() override;
	virtual void CommitDynamicMeshChange(TUniquePtr<FToolCommandChange> Change, const FText& ChangeMessage) override;
	virtual bool HasDynamicMeshComponent() const override;
	virtual UDynamicMeshComponent* GetDynamicMeshComponent() override;

	// IPhysicsDataSource implementation
	virtual UBodySetup* GetBodySetup() const override;
	virtual IInterface_CollisionDataProvider* GetComplexCollisionProvider() const override;

	// Rest provided by parent class

protected:
	TUniquePtr<FMeshDescription> ConvertedMeshDescription;
	bool bHaveMeshDescription = false;

protected:
	friend class UDynamicMeshComponentToolTargetFactory;
};


/** Factory for UDynamicMeshComponentToolTarget to be used by the target manager. */
UCLASS(Transient)
class MODELINGCOMPONENTSEDITORONLY_API UDynamicMeshComponentToolTargetFactory : public UToolTargetFactory
{
	GENERATED_BODY()

public:

	virtual bool CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) const override;

	virtual UToolTarget* BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) override;
};