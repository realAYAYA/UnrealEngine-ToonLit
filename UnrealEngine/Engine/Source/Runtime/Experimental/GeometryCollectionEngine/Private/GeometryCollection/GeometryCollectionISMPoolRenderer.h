// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/GeometryCollectionExternalRenderInterface.h"

#include "GeometryCollectionISMPoolRenderer.generated.h"

class AGeometryCollectionISMPoolActor;
class UGeometryCollectionISMPoolComponent;

/** Implementation of a geometry collection custom renderer that pushes AutoInstanceMeshes to an ISMPool. */
UCLASS()
class UGeometryCollectionISMPoolRenderer : public UObject, public IGeometryCollectionExternalRenderInterface
{
	GENERATED_BODY()

public:
	//~ Begin IGeometryCollectionExternalRenderInterface Interface.
	virtual void OnRegisterGeometryCollection(UGeometryCollectionComponent const& InComponent) override;
	virtual void OnUnregisterGeometryCollection() override;
	virtual void UpdateState(UGeometryCollection const& InGeometryCollection, FTransform const& InComponentTransform, bool bInIsBroken, bool bInIsVisible) override;
	virtual void UpdateRootTransform(UGeometryCollection const& InGeometryCollection, FTransform const& InRootTransform) override;
	virtual void UpdateTransforms(UGeometryCollection const& InGeometryCollection, TArrayView<const FTransform> InTransforms) override;
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

private:
	UGeometryCollectionISMPoolComponent* GetOrCreateISMPoolComponent();
	void InitMergedMeshFromGeometryCollection(UGeometryCollection const& InGeometryCollection);
	void InitInstancesFromGeometryCollection(UGeometryCollection const& InGeometryCollection);
	void UpdateMergedMeshTransforms(FTransform const& InBaseTransform);
	void UpdateInstanceTransforms(UGeometryCollection const& InGeometryCollection, FTransform const& InBaseTransform, TArrayView<const FTransform> InTransforms);
	void ReleaseGroup(FISMPoolGroup& InOutGroup);
};
