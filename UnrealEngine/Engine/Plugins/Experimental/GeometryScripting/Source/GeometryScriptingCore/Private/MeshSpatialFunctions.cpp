// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/MeshSpatialFunctions.h"
#include "UDynamicMesh.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "MeshQueries.h"
#include "Spatial/FastWinding.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshSpatialFunctions)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_MeshSpatial"


void UGeometryScriptLibrary_MeshSpatial::ResetBVH(FGeometryScriptDynamicMeshBVH& ResetBVH)
{
	ResetBVH.Spatial.Reset();
	ResetBVH.FWNTree.Reset();
}

UDynamicMesh* UGeometryScriptLibrary_MeshSpatial::BuildBVHForMesh(
	UDynamicMesh* TargetMesh,
	FGeometryScriptDynamicMeshBVH& OutputBVH,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("BuildBVHForMesh_InvalidInput", "BuildBVHForMesh: TargetMesh is Null"));
		return TargetMesh;
	}

	if (OutputBVH.Spatial.IsValid() == false)
	{
		OutputBVH.Spatial = MakeShared<FDynamicMeshAABBTree3>();
	}

	TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		OutputBVH.Spatial->SetMesh(&ReadMesh, true);
		OutputBVH.FWNTree = MakeShared<TFastWindingTree<FDynamicMesh3>>(OutputBVH.Spatial.Get(), true);
	});

	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshSpatial::IsBVHValidForMesh(
	UDynamicMesh* TargetMesh,
	const FGeometryScriptDynamicMeshBVH& TestBVH,
	bool& bIsValid,
	UGeometryScriptDebug* Debug)
{
	bIsValid = false;
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("IsBVHValidForMesh_InvalidInput", "IsBVHValidForMesh: TargetMesh is Null"));
		return TargetMesh;
	}

	if (TestBVH.Spatial.IsValid())
	{
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			if (TestBVH.Spatial->GetMesh() == &ReadMesh)
			{
				bIsValid = TestBVH.Spatial->IsValid(false);
			}
		});
	}

	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshSpatial::RebuildBVHForMesh(
	UDynamicMesh* TargetMesh,
	FGeometryScriptDynamicMeshBVH& UpdateBVH,
	bool bOnlyIfInvalid,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("RebuildBVHForMesh_InvalidInput", "RebuildBVHForMesh: TargetMesh is Null"));
		return TargetMesh;
	}

	if (UpdateBVH.Spatial.IsValid() == false)
	{
		UpdateBVH.Spatial = MakeShared<FDynamicMeshAABBTree3>();
	}

	TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		if (bOnlyIfInvalid == false || UpdateBVH.Spatial->GetMesh() != &ReadMesh || UpdateBVH.Spatial->IsValid(false) == false)
		{
			UpdateBVH.Spatial->SetMesh(&ReadMesh, false);
			UpdateBVH.Spatial->Build();
			// FWNTree does not support rebuilding like this?
			UpdateBVH.FWNTree = MakeShared<TFastWindingTree<FDynamicMesh3>>(UpdateBVH.Spatial.Get(), true);
		}
	});

	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshSpatial::FindNearestPointOnMesh(
	UDynamicMesh* TargetMesh,
	const FGeometryScriptDynamicMeshBVH& QueryBVH,
	FVector QueryPoint,
	FGeometryScriptSpatialQueryOptions Options,
	FGeometryScriptTrianglePoint& NearestResult,
	TEnumAsByte<EGeometryScriptSearchOutcomePins>& Outcome,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptSearchOutcomePins::NotFound;
	NearestResult.bValid = false;
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("FindNearestPointOnMesh_InvalidInput", "FindNearestPointOnMesh: TargetMesh is Null"));
		return TargetMesh;
	}

	bool bHaveValidBVH = false;
	TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		if (QueryBVH.Spatial.IsValid())
		{
			TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
			{
				if (QueryBVH.Spatial->GetMesh() == &ReadMesh && QueryBVH.Spatial->IsValid(Options.bAllowUnsafeModifiedQueries))
				{
					bHaveValidBVH = true;
					IMeshSpatial::FQueryOptions QueryOptions;
					QueryOptions.MaxDistance = (Options.MaxDistance == 0) ? TNumericLimits<float>::Max() : Options.MaxDistance;
					double NearDistSqr = 0;
					int NearestTID = QueryBVH.Spatial->FindNearestTriangle( (FVector3d)QueryPoint, NearDistSqr, QueryOptions);
					if (NearestTID >= 0)
					{
						Outcome = EGeometryScriptSearchOutcomePins::Found;
						FDistPoint3Triangle3d Distance = TMeshQueries<FDynamicMesh3>::TriangleDistance(ReadMesh, NearestTID, (FVector3d)QueryPoint);
						NearestResult.bValid = true;
						NearestResult.TriangleID = NearestTID;
						NearestResult.Position = (FVector)Distance.ClosestTrianglePoint;
						NearestResult.BaryCoords = (FVector)Distance.TriangleBaryCoords;
					}
				}
			});
		}
	});

	if (!bHaveValidBVH)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("FindNearestPointOnMesh_InvalidBVH", "FindNearestPointOnMesh: QueryBVH is Invalid for this TargetMesh"));
	}

	return TargetMesh;
}




UDynamicMesh* UGeometryScriptLibrary_MeshSpatial::FindNearestRayIntersectionWithMesh(
	UDynamicMesh* TargetMesh,
	UPARAM(ref) const FGeometryScriptDynamicMeshBVH& QueryBVH,
	FVector RayOrigin,
	FVector RayDirection,
	FGeometryScriptSpatialQueryOptions Options,
	FGeometryScriptRayHitResult& HitResult,
	TEnumAsByte<EGeometryScriptSearchOutcomePins>& Outcome,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptSearchOutcomePins::NotFound;
	HitResult.bHit = false;

	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("FindNearestRayIntersectionWithMesh_InvalidInput", "FindNearestRayIntersectionWithMesh: TargetMesh is Null"));
		return TargetMesh;
	}

	bool bHaveValidBVH = false;
	TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		if (QueryBVH.Spatial.IsValid())
		{
			TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
			{
				if (QueryBVH.Spatial->GetMesh() == &ReadMesh && QueryBVH.Spatial->IsValid(Options.bAllowUnsafeModifiedQueries))
				{
					bHaveValidBVH = true;
					IMeshSpatial::FQueryOptions QueryOptions;
					QueryOptions.MaxDistance = (Options.MaxDistance == 0) ? TNumericLimits<float>::Max() : Options.MaxDistance;
					FRay3d Ray((FVector3d)RayOrigin, Normalized((FVector3d)RayDirection));
					int HitTID = QueryBVH.Spatial->FindNearestHitTriangle(Ray, QueryOptions);
					if (HitTID >= 0)
					{
						Outcome = EGeometryScriptSearchOutcomePins::Found;
						FIntrRay3Triangle3d Intersection = TMeshQueries<FDynamicMesh3>::TriangleIntersection(ReadMesh, HitTID, Ray);
						HitResult.RayParameter = Intersection.RayParameter;
						HitResult.bHit = true;
						HitResult.HitTriangleID = HitTID;
						HitResult.HitPosition = Ray.PointAt(Intersection.RayParameter);
						HitResult.HitBaryCoords = (FVector)Intersection.TriangleBaryCoords;
					}
				}
			});
		}
	});

	if (!bHaveValidBVH)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("FindNearestRayIntersectionWithMesh_InvalidBVH", "FindNearestRayIntersectionWithMesh: QueryBVH is Invalid for this TargetMesh"));
	}

	return TargetMesh;
}






UDynamicMesh* UGeometryScriptLibrary_MeshSpatial::IsPointInsideMesh(
	UDynamicMesh* TargetMesh,
	UPARAM(ref) const FGeometryScriptDynamicMeshBVH& QueryBVH,
	FVector QueryPoint,
	FGeometryScriptSpatialQueryOptions Options,
	bool& bIsInside,
	TEnumAsByte<EGeometryScriptContainmentOutcomePins>& Outcome,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptContainmentOutcomePins::Outside;
	bIsInside = false;

	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("IsPointInsideMesh_InvalidInput", "IsPointInsideMesh: TargetMesh is Null"));
		return TargetMesh;
	}

	bool bHaveValidBVH = false;
	TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		if (QueryBVH.Spatial.IsValid() && QueryBVH.FWNTree.IsValid())
		{
			TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
			{
				if (QueryBVH.Spatial->GetMesh() == &ReadMesh && QueryBVH.Spatial->IsValid(Options.bAllowUnsafeModifiedQueries))
				{
					bHaveValidBVH = true;
					bIsInside = QueryBVH.FWNTree->IsInside((FVector3d)QueryPoint, Options.WindingIsoThreshold);
					if (bIsInside)
					{
						Outcome = EGeometryScriptContainmentOutcomePins::Inside;
					}
				}
			});
		}
	});

	if (!bHaveValidBVH)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("IsPointInsideMesh_InvalidBVH", "IsPointInsideMesh: QueryBVH is Invalid for this TargetMesh"));
	}

	return TargetMesh;
}




#undef LOCTEXT_NAMESPACE

