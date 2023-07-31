// Copyright Epic Games, Inc. All Rights Reserved.


#include "Selection/DynamicMeshSelector.h"
#include "Selections/GeometrySelectionUtil.h"
#include "ToolContextInterfaces.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshIndexUtil.h"
#include "DynamicMesh/MeshNormals.h"
#include "Changes/MeshVertexChange.h"
#include "DynamicMesh/ColliderMesh.h"
#include "GroupTopology.h"

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "FDynamicMeshSelector"


FDynamicMeshSelector::~FDynamicMeshSelector()
{
	if (TargetMesh.IsValid())
	{
		ensureMsgf(false, TEXT("FDynamicMeshSelector was not properly Shutdown!"));
		FDynamicMeshSelector::Shutdown();
	}
}


void FDynamicMeshSelector::Initialize(
	FGeometryIdentifier SourceGeometryIdentifierIn,
	UDynamicMesh* TargetMeshIn, 
	TUniqueFunction<UE::Geometry::FTransformSRT3d()> GetWorldTransformFuncIn)
{
	check(TargetMeshIn != nullptr);

	SourceGeometryIdentifier = SourceGeometryIdentifierIn;
	TargetMesh = TargetMeshIn;
	GetWorldTransformFunc = MoveTemp(GetWorldTransformFuncIn);

	RegisterMeshChangedHandler();
}

void FDynamicMeshSelector::RegisterMeshChangedHandler()
{
	TargetMesh_OnMeshChangedHandle = TargetMesh->OnMeshChanged().AddLambda([this](UDynamicMesh* Mesh, FDynamicMeshChangeInfo ChangeInfo)
	{
		ColliderMesh.Reset();

		if (ChangeInfo.Type != EDynamicMeshChangeType::MeshVertexChange)
		{
			GroupTopology.Reset();
		}

		// publish geometry-modified event
		NotifyGeometryModified();
	});
}


void FDynamicMeshSelector::Shutdown()
{
	if (TargetMesh.IsValid())
	{
		TargetMesh->OnMeshChanged().Remove(TargetMesh_OnMeshChangedHandle);
		TargetMesh_OnMeshChangedHandle.Reset();
		
		ColliderMesh.Reset();
	}
	TargetMesh = nullptr;
}

bool FDynamicMeshSelector::Sleep()
{
	check(TargetMesh.IsValid());

	TargetMesh->OnMeshChanged().Remove(TargetMesh_OnMeshChangedHandle);
	TargetMesh_OnMeshChangedHandle.Reset();

	SleepingTargetMesh = TargetMesh;
	TargetMesh = nullptr;

	ColliderMesh.Reset();
	GroupTopology.Reset();

	return true;
}

bool FDynamicMeshSelector::Restore()
{
	check(SleepingTargetMesh.IsValid());

	TargetMesh = SleepingTargetMesh.Get();
	SleepingTargetMesh = nullptr;

	RegisterMeshChangedHandler();

	return true;
}


bool FDynamicMeshSelector::RayHitTest(
	const FRay3d& WorldRay,
	FInputRayHit& HitResultOut )
{
	FTransformSRT3d WorldTransform = GetWorldTransformFunc();
	HitResultOut = FInputRayHit();
	FRay3d LocalRay = WorldTransform.InverseTransformRay(WorldRay);
	double RayHitT; int32 HitTriangleID; FVector3d HitBaryCoords;
	if (GetColliderMesh()->FindNearestHitTriangle(LocalRay, RayHitT, HitTriangleID, HitBaryCoords))
	{
		HitResultOut.bHit = true;
		HitResultOut.HitIdentifier = HitTriangleID;
		HitResultOut.HitOwner = TargetMesh.Get();
		FVector3d WorldPosition = WorldTransform.TransformPosition(LocalRay.PointAt(RayHitT));
		HitResultOut.HitDepth = WorldRay.GetParameter(WorldPosition);
	}
	return HitResultOut.bHit;
}


void FDynamicMeshSelector::UpdateSelectionViaRaycast(
	const FRay3d& WorldRay,
	FGeometrySelectionEditor& SelectionEditor,
	const FGeometrySelectionUpdateConfig& UpdateConfig,
	FGeometrySelectionUpdateResult& ResultOut)
{
	FRay3d LocalRay = GetWorldTransformFunc().InverseTransformRay(WorldRay);

	if (SelectionEditor.GetTopologyType() == EGeometryTopologyType::Polygroup)
	{
		UE::Geometry::UpdateGroupSelectionViaRaycast(
			GetColliderMesh(), GetGroupTopology(), &SelectionEditor,
			LocalRay, UpdateConfig, ResultOut);
	}
	else
	{
		UE::Geometry::UpdateTriangleSelectionViaRaycast(
			GetColliderMesh(), &SelectionEditor,
			LocalRay, UpdateConfig, ResultOut);
	}
}


void FDynamicMeshSelector::GetSelectionFrame(const FGeometrySelection& Selection, FFrame3d& SelectionFrame, bool bTransformToWorld)
{
	if (Selection.TopologyType == EGeometryTopologyType::Polygroup)
	{
		const FGroupTopology* Topology = GetGroupTopology();
		FGroupTopologySelection TopoSelection;
		TargetMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& SourceMesh)
		{
			UE::Geometry::ConvertPolygroupSelectionToTopologySelection(Selection, SourceMesh, Topology, TopoSelection);
		});

		SelectionFrame = Topology->GetSelectionFrame(TopoSelection);
	}
	else
	{
		TargetMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& SourceMesh)
		{
			UE::Geometry::GetTriangleSelectionFrame(Selection, SourceMesh, SelectionFrame);
		});
	}

	if (bTransformToWorld)
	{
		SelectionFrame.Transform(GetLocalToWorldTransform());
	}
}

void FDynamicMeshSelector::AccumulateSelectionBounds(const FGeometrySelection& Selection, FGeometrySelectionBounds& BoundsInOut, bool bTransformToWorld)
{
	FTransform UseTransform = (bTransformToWorld) ? GetLocalToWorldTransform() : FTransform::Identity;
	UE::Geometry::FAxisAlignedBox3d TargetWorldBounds = UE::Geometry::FAxisAlignedBox3d::Empty();
	int32 ElementCount = 0;

	if (Selection.TopologyType == EGeometryTopologyType::Polygroup)
	{
		const FGroupTopology* Topology = GetGroupTopology();
		TargetMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& SourceMesh)
		{
			UE::Geometry::EnumeratePolygroupSelectionVertices(Selection, SourceMesh, GetGroupTopology(), UseTransform,
				[&](uint32 VertexID, const FVector3d& Position) { 
					TargetWorldBounds.Contain(Position); 
					ElementCount++;
				}
			);
		});
	}
	else
	{
		TargetMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& SourceMesh)
		{
			UE::Geometry::EnumerateTriangleSelectionVertices(Selection, SourceMesh, UseTransform,
				[&](uint32 VertexID, const FVector3d& Position) { 
					TargetWorldBounds.Contain(Position); 
					ElementCount++;
				}
			);
		});
	}

	if (ElementCount > 0)		// relying on this because in (eg) single-vertex case, the box will still be "empty"
	{
		BoundsInOut.WorldBounds.Contain(TargetWorldBounds);
	}
}



void FDynamicMeshSelector::AccumulateSelectionElements(const FGeometrySelection& Selection, FGeometrySelectionElements& Elements, bool bTransformToWorld)
{
	FTransform UseWorldTransform = GetLocalToWorldTransform();
	const FTransform* ApplyTransform = (bTransformToWorld) ? &UseWorldTransform : nullptr;

	if (Selection.TopologyType == EGeometryTopologyType::Polygroup)
	{
		const FGroupTopology* Topology = GetGroupTopology();
		TargetMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& SourceMesh)
		{
			UE::Geometry::EnumeratePolygroupSelectionElements(Selection, SourceMesh, Topology,
				[&](uint32 VertexID, const FVector3d& Point) { Elements.Points.Add(Point); },
				[&](uint32 EdgeID, const FSegment3d& Segment) { Elements.Segments.Add(Segment); },
				[&](uint32 TriangleID, const FTriangle3d& Triangle) { Elements.Triangles.Add(Triangle); },
				ApplyTransform
			);
		});
	}
	else
	{
		TargetMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& SourceMesh)
		{
			UE::Geometry::EnumerateTriangleSelectionElements(Selection, SourceMesh, 
				[&](uint32 VertexID, const FVector3d& Point) { Elements.Points.Add(Point); },
				[&](uint32 EdgeID, const FSegment3d& Segment) { Elements.Segments.Add(Segment); },
				[&](uint32 TriangleID, const FTriangle3d& Triangle) { Elements.Triangles.Add(Triangle); },
				ApplyTransform
			);
		});
	}
}


void FDynamicMeshSelector::UpdateColliderMesh()
{
	// should we transform to world??
	TargetMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& SourceMesh)
	{
		FColliderMesh::FBuildOptions BuildOptions;
		BuildOptions.bBuildAABBTree = BuildOptions.bBuildVertexMap = BuildOptions.bBuildTriangleMap = true;
		ColliderMesh = MakePimpl<FColliderMesh>(SourceMesh, BuildOptions);
	});
}

const FColliderMesh* FDynamicMeshSelector::GetColliderMesh()
{
	if (!ColliderMesh.IsValid())
	{
		UpdateColliderMesh();
	}
	return ColliderMesh.Get();
}


void FDynamicMeshSelector::UpdateGroupTopology()
{
	// TODO would be preferable to not have to use the raw mesh pointer here, however
	// FGroupTopology currently needs the pointer to do mesh queries

	GroupTopology = MakePimpl<FGroupTopology>(TargetMesh->GetMeshPtr(), true);
}

const FGroupTopology* FDynamicMeshSelector::GetGroupTopology()
{
	if (!GroupTopology.IsValid())
	{
		UpdateGroupTopology();
	}
	return GroupTopology.Get();
}



bool FDynamicMeshComponentSelectorFactory::CanBuildForTarget(FGeometryIdentifier TargetIdentifier) const
{
	if (TargetIdentifier.TargetType == FGeometryIdentifier::ETargetType::PrimitiveComponent)
	{
		UDynamicMeshComponent* Component = TargetIdentifier.GetAsComponentType<UDynamicMeshComponent>();
		if (Component)
		{
			return true;
		}
	}
	return false;
}

TUniquePtr<IGeometrySelector> FDynamicMeshComponentSelectorFactory::BuildForTarget(FGeometryIdentifier TargetIdentifier) const
{
	UDynamicMeshComponent* Component = TargetIdentifier.GetAsComponentType<UDynamicMeshComponent>();
	check(Component != nullptr);

	TUniquePtr<FDynamicMeshSelector> Selector = MakeUnique<FDynamicMeshSelector>();
	Selector->Initialize(TargetIdentifier, Component->GetDynamicMesh(),
		[Component]() { return IsValid(Component) ? (UE::Geometry::FTransformSRT3d)Component->GetComponentTransform() : FTransformSRT3d::Identity(); }
	);
	return Selector;
}







/**
* FDynamicMeshSelectionTransformer is a basic Transformer implementation for a
* FDynamicMeshSelector. 
*/
class FDynamicMeshSelectionTransformer : public IGeometrySelectionTransformer
{
public:
	FDynamicMeshSelector* Selector;

	TArray<int32> MeshVertices;
	TArray<FVector3d> InitialPositions;
	TSet<int32> TriangleROI;
	TSet<int32> OverlayNormals;

	TArray<FVector3d> UpdatedPositions;

	TPimplPtr<FMeshVertexChangeBuilder> ActiveVertexChange;

	virtual IGeometrySelector* GetSelector() const override
	{
		return Selector;
	}

	virtual void BeginTransform(const FGeometrySelection& Selection) override;

	virtual void UpdateTransform( TFunctionRef<FVector3d(int32 VertexID, const FVector3d& InitialPosition, const FTransform& WorldTransform)> PositionTransformFunc ) override;
	void UpdatePendingVertexChange(bool bFinal);

	virtual void EndTransform(IToolsContextTransactionsAPI* TransactionsAPI) override;
};



void FDynamicMeshSelectionTransformer::BeginTransform(const FGeometrySelection& Selection)
{
	const FGroupTopology* UseTopology = nullptr;
	if (Selection.TopologyType == EGeometryTopologyType::Polygroup)
	{
		UseTopology = Selector->GetGroupTopology();
	}

	TSet<int32> VertexIDs;
	Selector->GetDynamicMesh()->ProcessMesh([&](const FDynamicMesh3& SourceMesh)
	{
		// get set of selected vertex IDs, and then vector (only converting to vector for VertexToTriangleOneRing...)
		if (Selection.TopologyType == EGeometryTopologyType::Polygroup)
		{
			UE::Geometry::EnumeratePolygroupSelectionVertices(Selection, SourceMesh, UseTopology, FTransform::Identity,
				[&](uint32 VertexID, const FVector3d& Position) { VertexIDs.Add((int32)VertexID); }
			);
		}
		else
		{
			UE::Geometry::EnumerateTriangleSelectionVertices(Selection, SourceMesh, FTransform::Identity,
				[&](uint32 VertexID, const FVector3d& Position) { VertexIDs.Add((int32)VertexID); }
			);
		}
		MeshVertices = VertexIDs.Array();

		// save initial positions
		InitialPositions.Reserve(MeshVertices.Num());
		for (int32 vid : MeshVertices)
		{
			InitialPositions.Add(SourceMesh.GetVertex(vid));
		}
		UpdatedPositions.SetNum(MeshVertices.Num());

		// get triangle ROI
		UE::Geometry::VertexToTriangleOneRing(&SourceMesh, MeshVertices, TriangleROI);

		// save overlay normals
		if (SourceMesh.HasAttributes() && SourceMesh.Attributes()->PrimaryNormals() != nullptr)
		{
			const FDynamicMeshNormalOverlay* Normals = SourceMesh.Attributes()->PrimaryNormals();
			UE::Geometry::TrianglesToOverlayElements(Normals, TriangleROI, OverlayNormals);
		}
	});

	ActiveVertexChange = MakePimpl<FMeshVertexChangeBuilder>(EMeshVertexChangeComponents::VertexPositions | EMeshVertexChangeComponents::OverlayNormals);
	UpdatePendingVertexChange(false);
}

void FDynamicMeshSelectionTransformer::UpdateTransform( 
	TFunctionRef<FVector3d(int32 VertexID, const FVector3d& InitialPosition, const FTransform& WorldTransform)> PositionTransformFunc 
)
{
	int32 N = MeshVertices.Num();
	FTransform WorldTransform = Selector->GetLocalToWorldTransform();

	for (int32 k = 0; k < N; ++k)
	{
		UpdatedPositions[k] = PositionTransformFunc(MeshVertices[k], InitialPositions[k], WorldTransform);
	}

	Selector->GetDynamicMesh()->EditMesh([&](FDynamicMesh3& EditMesh)
	{
		for (int32 k = 0; k < N; ++k)
		{
			EditMesh.SetVertex(MeshVertices[k], UpdatedPositions[k]);
		}

		// TODO: the right thing to do here would be to only recompute the modified overlay normals...but we have no utility for that??
		//FMeshNormals::QuickRecomputeOverlayNormals(EditMesh);

	}, EDynamicMeshChangeType::DeformationEdit,
		//EDynamicMeshAttributeChangeFlags::VertexPositions, false);
	   EDynamicMeshAttributeChangeFlags::VertexPositions | EDynamicMeshAttributeChangeFlags::NormalsTangents, false);

	UpdatePendingVertexChange(false);
}

void FDynamicMeshSelectionTransformer::UpdatePendingVertexChange(bool bFinal)
{
	// update the vertex change
	Selector->GetDynamicMesh()->ProcessMesh([&](const FDynamicMesh3& SourceMesh)
	{
		ActiveVertexChange->SaveVertices(&SourceMesh, MeshVertices, !bFinal);
		ActiveVertexChange->SaveOverlayNormals(&SourceMesh, OverlayNormals, !bFinal);
	});
}

void FDynamicMeshSelectionTransformer::EndTransform(IToolsContextTransactionsAPI* TransactionsAPI)
{
	UpdatePendingVertexChange(true);

	if (TransactionsAPI != nullptr)
	{
		TUniquePtr<FToolCommandChange> Result = MoveTemp(ActiveVertexChange->Change);
		TransactionsAPI->AppendChange(Selector->GetDynamicMesh(), MoveTemp(Result), LOCTEXT("DynamicMeshTransformChange", "Transform"));
	}

	ActiveVertexChange.Reset();
}







IGeometrySelectionTransformer* FDynamicMeshSelector::InitializeTransformation(const FGeometrySelection& Selection)
{
	check(!ActiveTransformer);

	// If we are transforming a DynamicMeshComponent, we want to defer collision updates, otherwise 
	// complex collision will be rebuilt every frame
	FGeometryIdentifier ParentIdentifier = GetSourceGeometryIdentifier();
	if (ParentIdentifier.ObjectType == FGeometryIdentifier::EObjectType::DynamicMeshComponent)
	{
		ParentIdentifier.GetAsComponentType<UDynamicMeshComponent>()->SetTransientDeferCollisionUpdates(true);
	}

	ActiveTransformer = MakePimpl<FDynamicMeshSelectionTransformer>();
	ActiveTransformer->Selector = this;

	return ActiveTransformer.Get();
}

void FDynamicMeshSelector::ShutdownTransformation(IGeometrySelectionTransformer* Transformer)
{
	ActiveTransformer.Reset();

	FGeometryIdentifier ParentIdentifier = GetSourceGeometryIdentifier();
	if (ParentIdentifier.ObjectType == FGeometryIdentifier::EObjectType::DynamicMeshComponent)
	{
		ParentIdentifier.GetAsComponentType<UDynamicMeshComponent>()->SetTransientDeferCollisionUpdates(false);
		ParentIdentifier.GetAsComponentType<UDynamicMeshComponent>()->UpdateCollision(true);
	}
}




#undef LOCTEXT_NAMESPACE 