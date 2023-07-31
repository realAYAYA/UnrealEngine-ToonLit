// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Change.h"
#include "Templates/PimplPtr.h"
#include "Selections/GeometrySelection.h"
#include "Selection/GeometrySelector.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "TransformTypes.h"


class FDynamicMeshSelectionTransformer;
class FMeshVertexChangeBuilder;
PREDECLARE_GEOMETRY(class FGroupTopology);
PREDECLARE_GEOMETRY(class FColliderMesh);


/**
 * FDynamicMeshSelector is an implementation of IGeometrySelector for a UDynamicMesh.
 * Note that the Selector itself does *not* require that the target object be a UDynamicMeshComponent.
 * Access to the World transform is provided by a TUniqueFunction set up in the Factory.
 */
class MODELINGCOMPONENTS_API FDynamicMeshSelector : public IGeometrySelector
{
public:
	virtual ~FDynamicMeshSelector();

	/**
	 * Initialize the FDynamicMeshSelector for a given source/target UDynamicMesh.
	 * @param SourceGeometryIdentifier identifier for the object that the TargetMesh came from (eg DynamicMeshComponent or other UDynamicMesh source)
	 * @param TargetMesh the target UDynamicMesh
	 * @param GetWorldTransformFunc function that provides the Local to World Transform
	 */
	virtual void Initialize(
		FGeometryIdentifier SourceGeometryIdentifier,
		UDynamicMesh* TargetMesh,
		TUniqueFunction<UE::Geometry::FTransformSRT3d()> GetWorldTransformFunc);

	/**
	 * @return FGeometryIdentifier for the parent of this Selector (eg a UDynamicMeshComponent in the common case)
	 */
	virtual FGeometryIdentifier GetSourceGeometryIdentifier() const
	{
		return SourceGeometryIdentifier;
	}

	//
	// IGeometrySelector API implementation
	//

	virtual void Shutdown() override;
	virtual bool SupportsSleep() const { return true; }
	virtual bool Sleep();
	virtual bool Restore();


	virtual FGeometryIdentifier GetIdentifier() const override
	{
		FGeometryIdentifier Identifier;
		Identifier.TargetType = FGeometryIdentifier::ETargetType::MeshContainer;
		Identifier.ObjectType = FGeometryIdentifier::EObjectType::DynamicMesh;
		Identifier.TargetObject = TargetMesh;
		return Identifier;
	}

	virtual bool RayHitTest(
		const FRay3d& WorldRay,
		FInputRayHit& HitResultOut) override;


	virtual void UpdateSelectionViaRaycast(
		const FRay3d& WorldRay,
		FGeometrySelectionEditor& SelectionEditor,
		const FGeometrySelectionUpdateConfig& UpdateConfig,
		FGeometrySelectionUpdateResult& ResultOut) override;


	virtual FTransform GetLocalToWorldTransform() const
	{
		return (FTransform)GetWorldTransformFunc();
	}

	virtual void GetSelectionFrame(const FGeometrySelection& Selection, UE::Geometry::FFrame3d& SelectionFrame, bool bTransformToWorld) override;
	virtual void AccumulateSelectionBounds(const FGeometrySelection& Selection, FGeometrySelectionBounds& BoundsInOut, bool bTransformToWorld) override;
	virtual void AccumulateSelectionElements(const FGeometrySelection& Selection, FGeometrySelectionElements& Elements, bool bTransformToWorld) override;

	virtual IGeometrySelectionTransformer* InitializeTransformation(const FGeometrySelection& Selection) override;
	virtual void ShutdownTransformation(IGeometrySelectionTransformer* Transformer) override;


protected:
	FGeometryIdentifier SourceGeometryIdentifier;
	TUniqueFunction<UE::Geometry::FTransformSRT3d()> GetWorldTransformFunc;

	TWeakObjectPtr<UDynamicMesh> TargetMesh;
	UDynamicMesh* GetDynamicMesh() const { return TargetMesh.Get(); }

	FDelegateHandle TargetMesh_OnMeshChangedHandle;
	void RegisterMeshChangedHandler();

	//
	// FColliderMesh is used to store a hit-testable AABBTree independent of the UDynamicMesh 
	//
	TPimplPtr<UE::Geometry::FColliderMesh> ColliderMesh;
	void UpdateColliderMesh();
	const UE::Geometry::FColliderMesh* GetColliderMesh();

	//
	// GroupTopology will be built on-demand if polygroup selection queries are made
	//
	TPimplPtr<UE::Geometry::FGroupTopology> GroupTopology;
	void UpdateGroupTopology();
	const UE::Geometry::FGroupTopology* GetGroupTopology();

	// support for sleep/restore
	TWeakObjectPtr<UDynamicMesh> SleepingTargetMesh = nullptr;


	TPimplPtr<FDynamicMeshSelectionTransformer> ActiveTransformer;

	// give Transformer access to internals 
	friend class FDynamicMeshSelectionTransformer;
};


/**
 * FDynamicMeshComponentSelectorFactory constructs FDynamicMeshSelector instances 
 * for UDynamicMeshComponents
 */
class MODELINGCOMPONENTS_API FDynamicMeshComponentSelectorFactory : public IGeometrySelectorFactory
{
public:
	virtual bool CanBuildForTarget(FGeometryIdentifier TargetIdentifier) const;

	virtual TUniquePtr<IGeometrySelector> BuildForTarget(FGeometryIdentifier TargetIdentifier) const;
};





