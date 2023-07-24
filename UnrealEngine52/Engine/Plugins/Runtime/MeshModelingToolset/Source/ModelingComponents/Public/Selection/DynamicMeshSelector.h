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


class FBasicDynamicMeshSelectionTransformer;
class FMeshVertexChangeBuilder;
PREDECLARE_GEOMETRY(class FGroupTopology);
PREDECLARE_GEOMETRY(class FColliderMesh);
PREDECLARE_GEOMETRY(class FSegmentTree3);

/**
 * FBaseDynamicMeshSelector is an implementation of IGeometrySelector for a UDynamicMesh.
 * Note that the Selector itself does *not* require that the target object be a UDynamicMeshComponent,
 * and subclasses of FBaseDynamicMeshSelector are used for both Volumes and StaticMeshComponents.
 * Access to the World transform is provided by a TUniqueFunction set up in the Factory.
 */
class MODELINGCOMPONENTS_API FBaseDynamicMeshSelector : public IGeometrySelector
{
public:
	virtual ~FBaseDynamicMeshSelector();

	/**
	 * Initialize the FBaseDynamicMeshSelector for a given source/target UDynamicMesh.
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

	virtual void InitializeSelectionFromPredicate(	
		FGeometrySelection& SelectionInOut,
		TFunctionRef<bool(UE::Geometry::FGeoSelectionID)> SelectionIDPredicate,
		EInitializeSelectionMode InitializeMode = EInitializeSelectionMode::All,
		const FGeometrySelection* ReferenceSelection = nullptr) override;



	virtual void UpdateSelectionFromSelection(	
		const FGeometrySelection& FromSelection,
		bool bAllowConversion,
		FGeometrySelectionEditor& SelectionEditor,
		const FGeometrySelectionUpdateConfig& UpdateConfig,
		UE::Geometry::FGeometrySelectionDelta* SelectionDelta = nullptr ) override;


	virtual bool RayHitTest(
		const FWorldRayQueryInfo& RayInfo,
		UE::Geometry::FGeometrySelectionHitQueryConfig QueryConfig,
		FInputRayHit& HitResultOut) override;


	virtual void UpdateSelectionViaRaycast(
		const FWorldRayQueryInfo& RayInfo,
		FGeometrySelectionEditor& SelectionEditor,
		const FGeometrySelectionUpdateConfig& UpdateConfig,
		FGeometrySelectionUpdateResult& ResultOut) override;

	virtual void GetSelectionPreviewForRaycast(
		const FWorldRayQueryInfo& RayInfo,
		FGeometrySelectionEditor& PreviewEditor) override;



	virtual void UpdateSelectionViaShape(	
		const FWorldShapeQueryInfo& ShapeInfo,
		FGeometrySelectionEditor& SelectionEditor,
		const FGeometrySelectionUpdateConfig& UpdateConfig,
		FGeometrySelectionUpdateResult& ResultOut 
	) override;


	virtual FTransform GetLocalToWorldTransform() const
	{
		return (FTransform)GetWorldTransformFunc();
	}

	virtual void GetSelectionFrame(const FGeometrySelection& Selection, UE::Geometry::FFrame3d& SelectionFrame, bool bTransformToWorld) override;
	virtual void AccumulateSelectionBounds(const FGeometrySelection& Selection, FGeometrySelectionBounds& BoundsInOut, bool bTransformToWorld) override;
	virtual void AccumulateSelectionElements(const FGeometrySelection& Selection, FGeometrySelectionElements& Elements, bool bTransformToWorld, bool bIsForPreview) override;

public:
	// these need to be public for the Transformers...can we do it another way?
	UDynamicMesh* GetDynamicMesh() const { return TargetMesh.Get(); }
	const UE::Geometry::FGroupTopology* GetGroupTopology();

protected:
	FGeometryIdentifier SourceGeometryIdentifier;
	TUniqueFunction<UE::Geometry::FTransformSRT3d()> GetWorldTransformFunc;

	TWeakObjectPtr<UDynamicMesh> TargetMesh;
	//UDynamicMesh* GetDynamicMesh() const { return TargetMesh.Get(); }		// temporarily public above

	FDelegateHandle TargetMesh_OnMeshChangedHandle;
	void RegisterMeshChangedHandler();
	void InvalidateOnMeshChange(FDynamicMeshChangeInfo ChangeInfo);

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
	//const UE::Geometry::FGroupTopology* GetGroupTopology();		// temporarily public above

	//
	// GroupEdgeSegmentTree stores a hit-testable AABBTree for the polygroup edges (depends on GroupTopology)
	//
	TPimplPtr<UE::Geometry::FSegmentTree3> GroupEdgeSegmentTree;
	void UpdateGroupEdgeSegmentTree();
	const UE::Geometry::FSegmentTree3* GetGroupEdgeSpatial();

	// support for sleep/restore
	TWeakObjectPtr<UDynamicMesh> SleepingTargetMesh = nullptr;


	virtual void UpdateSelectionViaRaycast_GroupEdges(
		const FWorldRayQueryInfo& RayInfo,
		FGeometrySelectionEditor& SelectionEditor,
		const FGeometrySelectionUpdateConfig& UpdateConfig,
		FGeometrySelectionUpdateResult& ResultOut);

	virtual void UpdateSelectionViaRaycast_MeshTopology(
		const FWorldRayQueryInfo& RayInfo,
		FGeometrySelectionEditor& SelectionEditor,
		const FGeometrySelectionUpdateConfig& UpdateConfig,
		FGeometrySelectionUpdateResult& ResultOut);

};


/**
 * FDynamicMeshSelector is an implementation of FBaseDynamicMeshSelector meant to be used
 * with UDynamicMeshComponents.
 */
class MODELINGCOMPONENTS_API FDynamicMeshSelector : public FBaseDynamicMeshSelector
{
public:

	virtual IGeometrySelectionTransformer* InitializeTransformation(const FGeometrySelection& Selection) override;
	virtual void ShutdownTransformation(IGeometrySelectionTransformer* Transformer) override;

protected:
	TSharedPtr<FBasicDynamicMeshSelectionTransformer> ActiveTransformer;

	// give Transformer access to internals 
	friend class FBasicDynamicMeshSelectionTransformer;
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


/**
* BasicDynamicMeshSelectionTransformer is a basic Transformer implementation that can be
* used with a FBaseDynamicMeshSelector. This Transformer moves the selected vertices and 
* nothing else (ie no polygroup-based soft deformation)
*/
class MODELINGCOMPONENTS_API FBasicDynamicMeshSelectionTransformer : public IGeometrySelectionTransformer
{
public:
	virtual void Initialize(FBaseDynamicMeshSelector* Selector);

	virtual IGeometrySelector* GetSelector() const override
	{
		return Selector;
	}

	virtual void BeginTransform(const UE::Geometry::FGeometrySelection& Selection) override;

	virtual void UpdateTransform( TFunctionRef<FVector3d(int32 VertexID, const FVector3d& InitialPosition, const FTransform& WorldTransform)> PositionTransformFunc ) override;
	void UpdatePendingVertexChange(bool bFinal);

	virtual void EndTransform(IToolsContextTransactionsAPI* TransactionsAPI) override;

	TFunction<void(IToolsContextTransactionsAPI* TransactionsAPI)> OnEndTransformFunc;

protected:
	FBaseDynamicMeshSelector* Selector;

	TArray<int32> MeshVertices;
	TArray<FVector3d> InitialPositions;
	TSet<int32> TriangleROI;
	TSet<int32> OverlayNormals;

	TArray<FVector3d> UpdatedPositions;

	TPimplPtr<FMeshVertexChangeBuilder> ActiveVertexChange;

};




