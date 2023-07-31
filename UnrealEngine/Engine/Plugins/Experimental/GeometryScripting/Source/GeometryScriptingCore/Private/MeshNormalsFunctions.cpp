// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/MeshNormalsFunctions.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTangents.h"
#include "Operations/RepairOrientation.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Polygroups/PolygroupSet.h"
#include "UDynamicMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshNormalsFunctions)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_MeshNormalsFunctions"


UDynamicMesh* UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals( UDynamicMesh* TargetMesh, UGeometryScriptDebug* Debug )
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("FlipNormals_InvalidInput", "FlipNormals: TargetMesh is Null"));
		return TargetMesh;
	}

	// todo: publish correct change types
	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		EditMesh.ReverseOrientation(true);

		if (EditMesh.HasAttributes() && EditMesh.Attributes()->PrimaryNormals() != nullptr)
		{
			FDynamicMeshNormalOverlay* Normals = EditMesh.Attributes()->PrimaryNormals();
			for (int elemid : Normals->ElementIndicesItr())
			{
				FVector3f Normal = Normals->GetElement(elemid);
				Normals->SetElement(elemid, -Normal);
			}
		}

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshNormalsFunctions::AutoRepairNormals(UDynamicMesh* TargetMesh, UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("AutoRepairNormals_InvalidInput", "AutoRepairNormals: TargetMesh is Null"));
		return TargetMesh;
	}

	// todo: publish correct change types
	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		FMeshRepairOrientation Repair(&EditMesh);
		Repair.OrientComponents();
		FDynamicMeshAABBTree3 Tree(&EditMesh, true);
		Repair.SolveGlobalOrientation(&Tree);

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(UDynamicMesh* TargetMesh, UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("SetPerVertexNormals_InvalidInput", "SetPerVertexNormals: TargetMesh is Null"));
		return TargetMesh;
	}

	// todo: publish correct change types
	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		if (EditMesh.HasAttributes() == false)
		{
			EditMesh.EnableAttributes();
		}
		FMeshNormals::InitializeOverlayToPerVertexNormals(EditMesh.Attributes()->PrimaryNormals(), false);

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshNormalsFunctions::SetPerFaceNormals(UDynamicMesh* TargetMesh, UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("SetPerFaceNormals_InvalidInput", "SetPerFaceNormals: TargetMesh is Null"));
		return TargetMesh;
	}

	// todo: publish correct change types
	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		FMeshNormals::InitializeMeshToPerTriangleNormals(&EditMesh);

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(
	UDynamicMesh* TargetMesh,
	FGeometryScriptCalculateNormalsOptions CalculateOptions,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("RecomputeNormals_InvalidInput", "RecomputeNormals: TargetMesh is Null"));
		return TargetMesh;
	}

	// todo: publish correct change types
	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		if (EditMesh.HasAttributes() == false)
		{
			EditMesh.EnableAttributes();
		}
		FMeshNormals MeshNormals(&EditMesh);
		MeshNormals.RecomputeOverlayNormals(EditMesh.Attributes()->PrimaryNormals(), CalculateOptions.bAreaWeighted, CalculateOptions.bAngleWeighted);
		MeshNormals.CopyToOverlay(EditMesh.Attributes()->PrimaryNormals(), false);

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals( 
	UDynamicMesh* TargetMesh, 
	FGeometryScriptSplitNormalsOptions SplitOptions, 
	FGeometryScriptCalculateNormalsOptions CalculateOptions,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ComputeSplitNormals_InvalidInput", "ComputeSplitNormals: TargetMesh is Null"));
		return TargetMesh;
	}

	// todo: publish correct change types
	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		if (EditMesh.HasAttributes() == false)
		{
			EditMesh.EnableAttributes();
		}

		float NormalDotProdThreshold = FMathf::Cos(SplitOptions.OpeningAngleDeg * FMathf::DegToRad);
		FMeshNormals FaceNormals(&EditMesh);
		FaceNormals.ComputeTriangleNormals();
		const TArray<FVector3d>& Normals = FaceNormals.GetNormals();

		TUniquePtr<FPolygroupSet> SplitByGroups;
		if (SplitOptions.bSplitByFaceGroup)
		{
			FPolygroupLayer InputGroupLayer{ SplitOptions.GroupLayer.bDefaultLayer, SplitOptions.GroupLayer.ExtendedLayerIndex };
			if (InputGroupLayer.CheckExists(&EditMesh) == false)
			{
				UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ComputeSplitNormals_MissingGroups", "ComputeSplitNormals: Polygroup Layer does not exist"));
				return;
			}
			SplitByGroups = MakeUnique<FPolygroupSet>(&EditMesh, InputGroupLayer);
		}

		// create new normal overlay topology based on splitting criteria
		EditMesh.Attributes()->PrimaryNormals()->CreateFromPredicate([&](int VID, int TA, int TB)
		{
			if (SplitOptions.bSplitByOpeningAngle && Normals[TA].Dot(Normals[TB]) < NormalDotProdThreshold)
			{
				return false;
			}
			if (SplitOptions.bSplitByFaceGroup && SplitByGroups->GetTriangleGroup(TA) != SplitByGroups->GetTriangleGroup(TB))
			{
				return false;
			}
			return true;

		}, 0);

		// recompute the normals in the overlay
		FMeshNormals RecomputeMeshNormals(&EditMesh);
		RecomputeMeshNormals.RecomputeOverlayNormals(EditMesh.Attributes()->PrimaryNormals(), CalculateOptions.bAreaWeighted, CalculateOptions.bAngleWeighted);
		RecomputeMeshNormals.CopyToOverlay(EditMesh.Attributes()->PrimaryNormals(), false);

		// todo: bAllowSharpVertices from EditNormalsOp?

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshNormalsFunctions::ComputeTangents( 
	UDynamicMesh* TargetMesh, 
	FGeometryScriptTangentsOptions Options,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ComputeTangents_InvalidInput", "ComputeTangents: TargetMesh is Null"));
		return TargetMesh;
	}

	// todo: publish correct change types
	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		if (EditMesh.HasAttributes() == false || !(Options.UVLayer < EditMesh.Attributes()->NumUVLayers()) || EditMesh.Attributes()->NumNormalLayers() == 0 )
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ComputeTangents_NoUVLayers", "ComputeTangents: TargetMesh is missing UV Set or Normals required to compute Tangents"));
			return;
		}
		if (EditMesh.Attributes()->HasTangentSpace() == false)
		{
			EditMesh.Attributes()->EnableTangents();
		}

		FComputeTangentsOptions TangentOptions;
		TangentOptions.bAveraged = (Options.Type == EGeometryScriptTangentTypes::FastMikkT);

		FMeshTangentsd Tangents(&EditMesh);
		Tangents.ComputeTriVertexTangents(
			EditMesh.Attributes()->PrimaryNormals(),
			EditMesh.Attributes()->GetUVLayer(Options.UVLayer),
			TangentOptions);
		Tangents.CopyToOverlays(EditMesh);

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}






UDynamicMesh* UGeometryScriptLibrary_MeshNormalsFunctions::SetMeshTriangleNormals(
	UDynamicMesh* TargetMesh,
	int TriangleID,
	FGeometryScriptTriangle Normals,
	bool& bIsValidTriangle,
	bool bDeferChangeNotifications)
{
	bIsValidTriangle = false;
	if (TargetMesh)
	{
		TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
		{
			if (EditMesh.IsTriangle(TriangleID) && EditMesh.HasAttributes() && EditMesh.Attributes()->PrimaryNormals() != nullptr)
			{
				bIsValidTriangle = true;
				FDynamicMeshNormalOverlay* NormalOverlay = EditMesh.Attributes()->PrimaryNormals();
				int32 Elem0 = NormalOverlay->AppendElement((FVector3f)Normals.Vector0);
				int32 Elem1 = NormalOverlay->AppendElement((FVector3f)Normals.Vector1);
				int32 Elem2 = NormalOverlay->AppendElement((FVector3f)Normals.Vector2);
				NormalOverlay->SetTriangle(TriangleID, FIndex3i(Elem0, Elem1, Elem2), true);
			}
		}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, bDeferChangeNotifications);
	}
	return TargetMesh;	
}






UDynamicMesh* UGeometryScriptLibrary_MeshNormalsFunctions::GetMeshPerVertexNormals(
	UDynamicMesh* TargetMesh, 
	FGeometryScriptVectorList& NormalList, 
	bool& bIsValidNormalSet,
	bool& bHasVertexIDGaps,
	bool bAverageSplitVertexValues)
{
	NormalList.Reset();
	TArray<FVector>& Normals = *NormalList.List;
	bHasVertexIDGaps = false;
	bIsValidNormalSet = false;
	if (TargetMesh)
	{
		TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			bHasVertexIDGaps = ! ReadMesh.IsCompactV();

			if (ReadMesh.HasAttributes() && ReadMesh.Attributes()->NumNormalLayers() > 0 )
			{
				const FDynamicMeshNormalOverlay* NormalOverlay = ReadMesh.Attributes()->PrimaryNormals();

				if (bAverageSplitVertexValues)
				{
					Normals.Init(FVector::Zero(), ReadMesh.MaxVertexID());
					for (int32 tid : ReadMesh.TriangleIndicesItr())
					{
						if (NormalOverlay->IsSetTriangle(tid))
						{
							FIndex3i TriV = ReadMesh.GetTriangle(tid);
							FVector3f A, B, C;
							NormalOverlay->GetTriElements(tid, A, B, C);
							Normals[TriV.A] += (FVector)A;
							Normals[TriV.B] += (FVector)B;
							Normals[TriV.C] += (FVector)C;
						}
					}

					for (int32 k = 0; k < Normals.Num(); ++k)
					{
						if (Normals[k].SquaredLength() > 0)
						{
							Normalize(Normals[k]);
						}
					}
				}
				else
				{
					Normals.Init(FVector::UnitZ(), ReadMesh.MaxVertexID());
					for (int32 tid : ReadMesh.TriangleIndicesItr())
					{
						if (NormalOverlay->IsSetTriangle(tid))
						{
							FIndex3i TriV = ReadMesh.GetTriangle(tid);
							FVector3f A, B, C;
							NormalOverlay->GetTriElements(tid, A, B, C);
							Normals[TriV.A] = (FVector)A;
							Normals[TriV.B] = (FVector)B;
							Normals[TriV.C] = (FVector)C;
						}
					}
				}

				bIsValidNormalSet = true;
			}
		});
	}

	return TargetMesh;
}




#undef LOCTEXT_NAMESPACE

