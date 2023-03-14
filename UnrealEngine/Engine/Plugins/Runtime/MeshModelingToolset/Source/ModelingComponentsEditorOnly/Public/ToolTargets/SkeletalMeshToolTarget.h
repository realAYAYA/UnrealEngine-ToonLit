// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TargetInterfaces/DynamicMeshCommitter.h"
#include "TargetInterfaces/DynamicMeshProvider.h"
#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/MeshDescriptionCommitter.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/SkeletalMeshBackedTarget.h"
#include "ToolTargets/PrimitiveComponentToolTarget.h"

#include "SkeletalMeshToolTarget.generated.h"


class USkeletalMesh;

/**
 * A tool target backed by a read-only skeletal mesh.
 * 
 * This is a special tool target that refers to the underlying asset (in this case a skeletal mesh), rather than indirectly through a component.
 * This type of target is used in cases, such as opening an asset through the content browser, when there is no component available.
 */
UCLASS(Transient)
class MODELINGCOMPONENTSEDITORONLY_API USkeletalMeshReadOnlyToolTarget :
	public UToolTarget,
	public IMeshDescriptionProvider,
	public IDynamicMeshProvider,
	public IMaterialProvider,
	public ISkeletalMeshBackedTarget
{
	GENERATED_BODY()

public:
	// UToolTarget
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

	// ISkeletalMeshBackedTarget implementation
	virtual USkeletalMesh* GetSkeletalMesh() const override;

protected:
	USkeletalMesh* SkeletalMesh = nullptr;

	// So that the tool target factory can poke into Component.
	friend class USkeletalMeshReadOnlyToolTargetFactory;
	friend class USkeletalMeshComponentReadOnlyToolTarget;
	friend class USkeletalMeshComponentToolTarget;

	static bool IsValid(const USkeletalMesh* USkeletalMesh);

	static void GetMeshDescription(const USkeletalMesh* SkeletalMesh, FMeshDescription& MeshDescriptionOut);
	static void GetMaterialSet(const USkeletalMesh* SkeletalMesh, FComponentMaterialSet& MaterialSetOut,
		bool bPreferAssetMaterials);
	static bool CommitMaterialSetUpdate(USkeletalMesh* SkeletalMesh,
		const FComponentMaterialSet& MaterialSet, bool bApplyToAsset);

	// Until USkeletalMesh stores its internal representation as FMeshDescription, we need to
	// retain the storage here to cover the lifetime of the pointer returned by GetMeshDescription(). 
	TUniquePtr<FMeshDescription> CachedMeshDescription;	
};


/**
 * A tool target backed by a skeletal mesh.
 */
UCLASS(Transient)
class MODELINGCOMPONENTSEDITORONLY_API USkeletalMeshToolTarget :
	public USkeletalMeshReadOnlyToolTarget,
	public IMeshDescriptionCommitter,
	public IDynamicMeshCommitter
{
	GENERATED_BODY()

public:
	// IMeshDescriptionCommitter implementation
	virtual void CommitMeshDescription(const FCommitter& Committer, const FCommitMeshParameters& CommitMeshParams = FCommitMeshParameters()) override;
	using IMeshDescriptionCommitter::CommitMeshDescription; // unhide the other overload

	// IDynamicMeshCommitter
	virtual void CommitDynamicMesh(const UE::Geometry::FDynamicMesh3& Mesh, 
		const FDynamicMeshCommitInfo& CommitInfo) override;
	using IDynamicMeshCommitter::CommitDynamicMesh; // unhide the other overload

protected:
	// So that the tool target factory can poke into Component.
	friend class USkeletalMeshToolTargetFactory;
	friend class USkeletalMeshComponentToolTarget;

	static void CommitMeshDescription(USkeletalMesh* SkeletalMesh, 
		FMeshDescription* MeshDescription, const FCommitter& Committer);
};


/** Factory for USkeletalMeshReadOnlyToolTarget to be used by the target manager. */
UCLASS(Transient)
class MODELINGCOMPONENTSEDITORONLY_API USkeletalMeshReadOnlyToolTargetFactory : public UToolTargetFactory
{
	GENERATED_BODY()

public:

	virtual bool CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) const override;

	virtual UToolTarget* BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) override;
};


/** Factory for USkeletalMeshToolTarget to be used by the target manager. */
UCLASS(Transient)
class MODELINGCOMPONENTSEDITORONLY_API USkeletalMeshToolTargetFactory : public UToolTargetFactory
{
	GENERATED_BODY()

public:

	virtual bool CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) const override;

	virtual UToolTarget* BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) override;
};
