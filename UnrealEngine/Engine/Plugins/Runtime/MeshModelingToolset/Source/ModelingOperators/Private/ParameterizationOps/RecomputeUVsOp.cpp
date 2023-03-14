// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParameterizationOps/RecomputeUVsOp.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Selections/MeshConnectedComponents.h"
#include "Parameterization/MeshLocalParam.h"
#include "Parameterization/DynamicMeshUVEditor.h"
#include "Parameterization/PatchBasedMeshUVGenerator.h"
#include "Properties/RecomputeUVsProperties.h"

#include "DynamicSubmesh3.h"

#include "Async/ParallelFor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RecomputeUVsOp)

using namespace UE::Geometry;


#define LOCTEXT_NAMESPACE "RecomputeUVsOp"


void FRecomputeUVsOp::NormalizeUVAreas(const FDynamicMesh3& Mesh, FDynamicMeshUVOverlay* Overlay, float GlobalScale)
{
	FMeshConnectedComponents UVComponents(&Mesh);
	UVComponents.FindConnectedTriangles([&](int32 Triangle0, int32 Triangle1) {
		return Overlay->AreTrianglesConnected(Triangle0, Triangle1);
	});

	// TODO ParallelFor
	for (FMeshConnectedComponents::FComponent& Component : UVComponents)
	{
		const TArray<int>& Triangles = Component.Indices;
		TSet<int> UVElements;
		UVElements.Reserve(Triangles.Num() * 3);
		double AreaUV = 0;
		double Area3D = 0;
		FVector3d Triangle3D[3];
		FVector2d TriangleUV[3];
		FAxisAlignedBox2d BoundsUV = FAxisAlignedBox2d::Empty();

		for (int tid : Triangles)
		{
			FIndex3i TriElements = Overlay->GetTriangle(tid);
			if (!TriElements.Contains(FDynamicMesh3::InvalidID))
			{
				for (int j = 0; j < 3; ++j)
				{
					TriangleUV[j] = FVector2d(Overlay->GetElement(TriElements[j]));
					BoundsUV.Contain(TriangleUV[j]);
					Triangle3D[j] = Mesh.GetVertex(Overlay->GetParentVertex(TriElements[j]));
					UVElements.Add(TriElements[j]);
				}
				AreaUV += VectorUtil::Area(TriangleUV[0], TriangleUV[1], TriangleUV[2]);
				Area3D += VectorUtil::Area(Triangle3D[0], Triangle3D[1], Triangle3D[2]);
			}
		}
		
		double LinearScale = (AreaUV > 0.00001) ? ( FMathd::Sqrt(Area3D) / FMathd::Sqrt(AreaUV)) : 1.0;
		LinearScale = LinearScale * GlobalScale;
		FVector2d ComponentOrigin = BoundsUV.Center();

		for (int elemid : UVElements)
		{
			FVector2d UV = FVector2d(Overlay->GetElement(elemid));
			UV = (UV - ComponentOrigin) * LinearScale;
			Overlay->SetElement(elemid, FVector2f(UV));
		}
	}
}


void FRecomputeUVsOp::CalculateResult(FProgressCancel* Progress)
{
	NewResultInfo = FGeometryResult(EGeometryResultType::InProgress);

	bool bOK;
	if (bMergingOptimization)
	{
		bOK = CalculateResult_RegionOptimization(Progress);
	}
	else
	{
		bOK = CalculateResult_Basic(Progress);
	}

	NewResultInfo.SetSuccess(bOK, Progress);
	SetResultInfo(NewResultInfo);

}




bool FRecomputeUVsOp::CalculateResult_Basic(FProgressCancel* Progress)
{
	if (!InputMesh.IsValid())
	{
		return false;
	}

	ResultMesh = MakeUnique<FDynamicMesh3>(*InputMesh);

	FDynamicMesh3* BaseMesh = ResultMesh.Get();
	FDynamicMeshUVEditor UVEditor(BaseMesh, UVLayer, true);
	FDynamicMeshUVOverlay* UseOverlay = UVEditor.GetOverlay();
	if (ensure(UseOverlay != nullptr) == false)
	{
		return false;
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	// find group-connected-components
	bool bUseExisingUVTopology = false;
	FMeshConnectedComponents ConnectedComponents(ResultMesh.Get());
	if (IslandMode == ERecomputeUVsIslandMode::PolyGroups)
	{
		if (InputGroups != nullptr)
		{
			ConnectedComponents.FindConnectedTriangles([this](int32 CurTri, int32 NbrTri) {
				return InputGroups->GetTriangleGroup(CurTri) == InputGroups->GetTriangleGroup(NbrTri);
			});
		}
		else
		{
			ConnectedComponents.FindConnectedTriangles([this](int32 CurTri, int32 NbrTri) {
				return ResultMesh->GetTriangleGroup(CurTri) == ResultMesh->GetTriangleGroup(NbrTri);
			});
		}
	}
	else
	{
		ConnectedComponents.FindConnectedTriangles([&](int32 Triangle0, int32 Triangle1) {
			return UseOverlay->AreTrianglesConnected(Triangle0, Triangle1);
		});
		bUseExisingUVTopology = true;
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	// TODO: the solves here could be done in parallel if we pre-allocated the island element IDs

	int32 NumComponents = ConnectedComponents.Num();
	TArray<bool> bComponentSolved;
	bComponentSolved.Init(false, NumComponents);
	int32 SuccessCount = 0;
	for (int32 k = 0; k < NumComponents; ++k)
	{
		const TArray<int32>& ComponentTris = ConnectedComponents[k].Indices;

		bComponentSolved[k] = false;
		switch (UnwrapType)
		{
			case ERecomputeUVsUnwrapType::ExpMap:
			{
				FDynamicMeshUVEditor::FExpMapOptions Options;
				Options.NormalSmoothingRounds = this->NormalSmoothingRounds;
				Options.NormalSmoothingAlpha = this->NormalSmoothingAlpha;
				bComponentSolved[k] = UVEditor.SetTriangleUVsFromExpMap(ComponentTris, Options);
				break;
			}

			case ERecomputeUVsUnwrapType::ConformalFreeBoundary:
			{
				bComponentSolved[k] = UVEditor.SetTriangleUVsFromFreeBoundaryConformal(ComponentTris, bUseExisingUVTopology);
				if (bComponentSolved[k])
				{
					UVEditor.ScaleUVAreaTo3DArea(ComponentTris, true);
				}
				break;
			}
			
			case ERecomputeUVsUnwrapType::SpectralConformal:
			{
				bComponentSolved[k] = UVEditor.SetTriangleUVsFromFreeBoundarySpectralConformal(ComponentTris, bUseExisingUVTopology, bPreserveIrregularity);
				if (bComponentSolved[k])
				{
					UVEditor.ScaleUVAreaTo3DArea(ComponentTris, true);
				}
				break;
			}
		}
		

		if (bComponentSolved[k])
		{
			SuccessCount++;
		}

		if (Progress && Progress->Cancelled())
		{
			return false;
		}
	}

	if (bAutoRotate)
	{
		ParallelFor(NumComponents, [&](int32 k)
		{
			if (Progress && Progress->Cancelled())
			{
				return;
			}
			if (bComponentSolved[k])
			{
				const TArray<int32>& ComponentTris = ConnectedComponents[k].Indices;
				UVEditor.AutoOrientUVArea(ComponentTris);
			};
		});
	}
	if (Progress && Progress->Cancelled())
	{
		return false;
	}


	if (bNormalizeAreas)
	{
		// todo should be a DynamicUVEditor function?
		NormalizeUVAreas(*ResultMesh, UseOverlay, AreaScaling);
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	if (bPackUVs)
	{
		bool bPackingSuccess = UVEditor.QuickPack(PackingTextureResolution, PackingGutterWidth);
		if (!bPackingSuccess)
		{
			NewResultInfo.AddError(FGeometryError(0, LOCTEXT("IslandPackingFailed", "Failed to pack UV islands")));
			return false;
		}
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	return true;
}






bool FRecomputeUVsOp::CalculateResult_RegionOptimization(FProgressCancel* Progress)
{
	if (!InputMesh.IsValid())
	{
		return false;
	}

	ResultMesh = MakeUnique<FDynamicMesh3>(*InputMesh);

	FDynamicMesh3* BaseMesh = ResultMesh.Get();
	FDynamicMeshUVEditor UVEditor(BaseMesh, UVLayer, true);
	FDynamicMeshUVOverlay* UseOverlay = UVEditor.GetOverlay();
	if (ensure(UseOverlay != nullptr) == false)
	{
		return false;
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	// initialize UV generation with connected components
	FMeshConnectedComponents ConnectedComponents(ResultMesh.Get());
	if (IslandMode == ERecomputeUVsIslandMode::PolyGroups)
	{
		if (InputGroups != nullptr)
		{
			ConnectedComponents.FindConnectedTriangles([this](int32 CurTri, int32 NbrTri) {
				return InputGroups->GetTriangleGroup(CurTri) == InputGroups->GetTriangleGroup(NbrTri);
			});
		}
		else
		{
			ConnectedComponents.FindConnectedTriangles([this](int32 CurTri, int32 NbrTri) {
				return ResultMesh->GetTriangleGroup(CurTri) == ResultMesh->GetTriangleGroup(NbrTri);
			});
		}
	}
	else
	{
		ConnectedComponents.FindConnectedTriangles([&](int32 Triangle0, int32 Triangle1) {
			return UseOverlay->AreTrianglesConnected(Triangle0, Triangle1);
		});
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}


	FPatchBasedMeshUVGenerator UVGenerator;
	UVGenerator.MergingThreshold = this->MergingThreshold;
	UVGenerator.CompactnessThreshold = this->CompactnessThreshold;
	UVGenerator.MaxNormalDeviationDeg = this->MaxNormalDeviationDeg;
	UVGenerator.NormalSmoothingRounds = this->NormalSmoothingRounds;
	UVGenerator.NormalSmoothingAlpha = this->NormalSmoothingAlpha;

	TArray<TArray<int32>> UVIslandTriangleSets;
	bool bIslandsOK = UVGenerator.ComputeIslandsByRegionMerging(*BaseMesh, *UseOverlay, ConnectedComponents, UVIslandTriangleSets, Progress);

	if (bIslandsOK == false)
	{
		NewResultInfo.AddError(FGeometryError(0, LOCTEXT("IslandMergingFailed", "Failed to merge UV islands")));
		return false;
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	TArray<bool> bIslandUVsValid;
	int32 NumSolvesFailed = UVGenerator.ComputeUVsFromTriangleSets(*BaseMesh, *UseOverlay, UVIslandTriangleSets, bIslandUVsValid, Progress);
	if (NumSolvesFailed > 0)
	{
		NewResultInfo.AddWarning(FGeometryWarning(0, LOCTEXT("PartialSolvesFailed", "Failed to compute UVs for some UV islands")));
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	if (bAutoRotate)
	{
		ParallelFor(UVIslandTriangleSets.Num(), [&](int32 k)
		{
			if (Progress && Progress->Cancelled())
			{
				return;
			}
			if (bIslandUVsValid[k])
			{
				UVEditor.AutoOrientUVArea(UVIslandTriangleSets[k]);
			}
		});
	}
	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	if (bNormalizeAreas)
	{
		// todo should be a DynamicUVEditor function?
		NormalizeUVAreas(*ResultMesh, UseOverlay, AreaScaling);
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	if (bPackUVs)
	{
		bool bPackingSuccess = UVEditor.QuickPack(PackingTextureResolution, PackingGutterWidth);
		if (!bPackingSuccess)
		{
			NewResultInfo.AddError(FGeometryError(0, LOCTEXT("IslandPackingFailed", "Failed to pack UV islands")));
			return false;
		}
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	return true;
}


/**
 * Factory
 */


TUniquePtr<FDynamicMeshOperator> URecomputeUVsOpFactory::MakeNewOperator()
{
	TUniquePtr<FRecomputeUVsOp> RecomputeUVsOp = MakeUnique<FRecomputeUVsOp>();

	RecomputeUVsOp->InputMesh = OriginalMesh;
	RecomputeUVsOp->InputGroups = InputGroups;
	RecomputeUVsOp->UVLayer = GetSelectedUVChannel();

	RecomputeUVsOp->IslandMode = static_cast<ERecomputeUVsIslandMode>(Settings->IslandGeneration);

	switch (Settings->UnwrapType)
	{
	case ERecomputeUVsPropertiesUnwrapType::ExpMap:
		RecomputeUVsOp->UnwrapType = ERecomputeUVsUnwrapType::ExpMap;
		RecomputeUVsOp->bMergingOptimization = false;
		break;
	case ERecomputeUVsPropertiesUnwrapType::Conformal:
		RecomputeUVsOp->UnwrapType = ERecomputeUVsUnwrapType::ConformalFreeBoundary;
		RecomputeUVsOp->bMergingOptimization = false;
		break;
	case ERecomputeUVsPropertiesUnwrapType::SpectralConformal:
		RecomputeUVsOp->UnwrapType = ERecomputeUVsUnwrapType::SpectralConformal;
		RecomputeUVsOp->bMergingOptimization = false;
		break;
	case ERecomputeUVsPropertiesUnwrapType::IslandMerging:
		RecomputeUVsOp->UnwrapType = ERecomputeUVsUnwrapType::ExpMap;
		RecomputeUVsOp->bMergingOptimization = true;
	}

	RecomputeUVsOp->bAutoRotate = (Settings->AutoRotation == ERecomputeUVsToolOrientationMode::MinBounds);

	RecomputeUVsOp->bPreserveIrregularity = Settings->bPreserveIrregularity;

	RecomputeUVsOp->NormalSmoothingRounds = Settings->SmoothingSteps;
	RecomputeUVsOp->NormalSmoothingAlpha = Settings->SmoothingAlpha;

	RecomputeUVsOp->MergingThreshold = Settings->MergingDistortionThreshold;
	RecomputeUVsOp->MaxNormalDeviationDeg = Settings->MergingAngleThreshold;

	RecomputeUVsOp->bPackUVs = false;
	RecomputeUVsOp->bNormalizeAreas = false;

	switch (Settings->LayoutType)
	{
	case ERecomputeUVsPropertiesLayoutType::None:
		break;
	case ERecomputeUVsPropertiesLayoutType::Repack:
		RecomputeUVsOp->bPackUVs = true;
		RecomputeUVsOp->PackingTextureResolution = Settings->TextureResolution;
		break;
	case ERecomputeUVsPropertiesLayoutType::NormalizeToBounds:
		RecomputeUVsOp->bNormalizeAreas = true;
		RecomputeUVsOp->AreaScaling = Settings->NormalizeScale / OriginalMesh->GetBounds().MaxDim();
		break;
	case ERecomputeUVsPropertiesLayoutType::NormalizeToWorld:
		RecomputeUVsOp->bNormalizeAreas = true;
		RecomputeUVsOp->AreaScaling = Settings->NormalizeScale;
	}

	RecomputeUVsOp->SetTransform(TargetTransform);

	return RecomputeUVsOp;
}


#undef LOCTEXT_NAMESPACE

