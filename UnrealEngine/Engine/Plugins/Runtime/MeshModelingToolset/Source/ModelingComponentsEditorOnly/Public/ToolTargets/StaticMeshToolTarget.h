// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TargetInterfaces/DynamicMeshCommitter.h"
#include "TargetInterfaces/DynamicMeshProvider.h"
#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/MeshDescriptionCommitter.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/StaticMeshBackedTarget.h"
#include "ToolTargets/PrimitiveComponentToolTarget.h"
#include "ComponentSourceInterfaces.h"  // for EMeshLODIdentifier

#include "StaticMeshToolTarget.generated.h"

class UStaticMesh;

/**
 * A tool target backed by a static mesh asset that can provide and take a mesh
 * description.
 * 
 * This is a special tool target that refers to the underlying asset (in this case a static mesh), rather than indirectly through a component.
 * This type of target is used in cases, such as opening an asset through the content browser, when there is no component available.
 */
UCLASS(Transient)
class MODELINGCOMPONENTSEDITORONLY_API UStaticMeshToolTarget : 
	public UToolTarget,
	public IMeshDescriptionCommitter, 
	public IMeshDescriptionProvider, 
	public IMaterialProvider, 
	public IStaticMeshBackedTarget,
	public IDynamicMeshProvider, 
	public IDynamicMeshCommitter
{
	GENERATED_BODY()

public:
	/**
	 * Configure active LOD to edit. Can only call this after Component is configured in base UPrimitiveComponentToolTarget.
	 * If requested LOD does not exist, fall back to one that does.
	 */
	virtual void SetEditingLOD(EMeshLODIdentifier RequestedEditingLOD);

	/** @return current editing LOD */
	virtual EMeshLODIdentifier GetEditingLOD() const { return EditingLOD; }

	// UToolTarget
	virtual bool IsValid() const override;

	// IMeshDescriptionProvider implementation
	virtual const FMeshDescription* GetMeshDescription(const FGetMeshParameters& GetMeshParams = FGetMeshParameters()) override;
	virtual FMeshDescription GetEmptyMeshDescription() override;

	// IMeshDescritpionCommitter implementation
	virtual void CommitMeshDescription(const FCommitter& Committer, const FCommitMeshParameters& CommitParams = FCommitMeshParameters()) override;
	using IMeshDescriptionCommitter::CommitMeshDescription; // unhide the other overload

	// IDynamicMeshProvider
	virtual UE::Geometry::FDynamicMesh3 GetDynamicMesh() override;

	// IDynamicMeshCommitter
	virtual void CommitDynamicMesh(const UE::Geometry::FDynamicMesh3& Mesh, const FDynamicMeshCommitInfo& CommitInfo) override;
	using IDynamicMeshCommitter::CommitDynamicMesh; // unhide the other overload

	// IMaterialProvider implementation
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	virtual void GetMaterialSet(FComponentMaterialSet& MaterialSetOut, bool bPreferAssetMaterials) const override;
	virtual bool CommitMaterialSetUpdate(const FComponentMaterialSet& MaterialSet, bool bApplyToAsset) override;	

	// IStaticMeshBackedTarget
	virtual UStaticMesh* GetStaticMesh() const override;

	// Rest provided by parent class

protected:
	UStaticMesh* StaticMesh = nullptr;

	EMeshLODIdentifier EditingLOD = EMeshLODIdentifier::LOD0;

	friend class UStaticMeshToolTargetFactory;

	friend class UStaticMeshComponentToolTarget;

	static bool IsValid(const UStaticMesh* StaticMesh, EMeshLODIdentifier EditingLOD);
	static EMeshLODIdentifier GetValidEditingLOD(const UStaticMesh* StaticMesh, 
		EMeshLODIdentifier RequestedEditingLOD);
	static void CommitMeshDescription(UStaticMesh* StaticMesh, 
		const FCommitter& Committer, EMeshLODIdentifier EditingLODIn);
	static void GetMaterialSet(const UStaticMesh* StaticMesh, 
		FComponentMaterialSet& MaterialSetOut, bool bPreferAssetMaterials);
	static bool CommitMaterialSetUpdate(UStaticMesh* SkeletalMesh, 
		const FComponentMaterialSet& MaterialSet, bool bApplyToAsset);
};


/** Factory for UStaticMeshToolTarget to be used by the target manager. */
UCLASS(Transient)
class MODELINGCOMPONENTSEDITORONLY_API UStaticMeshToolTargetFactory : public UToolTargetFactory
{
	GENERATED_BODY()

public:

	virtual bool CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) const override;

	virtual UToolTarget* BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) override;



public:
	virtual EMeshLODIdentifier GetActiveEditingLOD() const { return EditingLOD; }
	virtual void SetActiveEditingLOD(EMeshLODIdentifier NewEditingLOD);

protected:
	// LOD to edit, default is to edit LOD0
	EMeshLODIdentifier EditingLOD = EMeshLODIdentifier::LOD0;
};