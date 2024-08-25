// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/GeometryCollectionExternalRenderInterface.h"

#include "GeometryCollectionISMPoolRenderer.generated.h"

class AGeometryCollectionISMPoolActor;
class UGeometryCollectionISMPoolComponent;
class UGeometryCollectionComponent;
class ULevel;

/** Implementation of a geometry collection custom renderer that pushes AutoInstanceMeshes to an ISMPool. */
UCLASS()
class UGeometryCollectionISMPoolRenderer : public UObject, public IGeometryCollectionExternalRenderInterface
{
	GENERATED_BODY()

public:
	//~ Begin IGeometryCollectionExternalRenderInterface Interface.
	virtual void OnRegisterGeometryCollection(UGeometryCollectionComponent const& InComponent) override;
	virtual void OnUnregisterGeometryCollection() override;
	virtual void UpdateState(UGeometryCollection const& InGeometryCollection, FTransform const& InComponentTransform, uint32 InStateFlags) override;
	virtual void UpdateRootTransform(UGeometryCollection const& InGeometryCollection, FTransform const& InRootTransform) override;
	virtual void UpdateRootTransforms(UGeometryCollection const& InGeometryCollection, FTransform const& InRootTransform, TArrayView<const FTransform3f> InRootLocalTransforms) override;
	virtual void UpdateTransforms(UGeometryCollection const& InGeometryCollection, TArrayView<const FTransform3f> InTransforms) override;
	//~ End IGeometryCollectionExternalRenderInterface Interface.

	/** Description for a group of meshes that are added/updated together. */
	struct FISMPoolGroup
	{
		int32 GroupIndex = INDEX_NONE;
		TArray<int32> MeshIds;
	};

protected:
	/** Instanced Static Mesh Pool actor that is used to render our meshes. */
	UPROPERTY(Transient)
	TObjectPtr<AGeometryCollectionISMPoolActor> ISMPoolActor;

	/** Cached component transform. */
	FTransform ComponentTransform = FTransform::Identity;

	/** ISM pool groups per rendering element type. */
	FISMPoolGroup MergedMeshGroup;
	FISMPoolGroup InstancesGroup;

	/** level of the owning component of this renderer */
	ULevel* OwningLevel = nullptr;

private:
	UGeometryCollectionISMPoolComponent* GetOrCreateISMPoolComponent();
	void InitMergedMeshFromGeometryCollection(UGeometryCollection const& InGeometryCollection);
	void InitInstancesFromGeometryCollection(UGeometryCollection const& InGeometryCollection);
	void UpdateMergedMeshTransforms(FTransform const& InBaseTransform, TArrayView<const FTransform3f> LocalTransforms);
	void UpdateInstanceTransforms(UGeometryCollection const& InGeometryCollection, FTransform const& InBaseTransform, TArrayView<const FTransform3f> InTransforms);
	void ReleaseGroup(FISMPoolGroup& InOutGroup);
};
