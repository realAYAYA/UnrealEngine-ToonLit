// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshBakingNodes/BakeMeshTextureImageNode.h"
#include "GeometryFlowNodeUtil.h"

#include "Sampling/MeshResampleImageBaker.h"

using namespace UE::Geometry;
using namespace UE::GeometryFlow;

void FBakeMeshTextureImageNode::Evaluate(
	const FNamedDataMap& DatasIn,
	FNamedDataMap& DatasOut,
	TUniquePtr<FEvaluationInfo>& EvaluationInfo)
{
	if (ensure(DatasOut.Contains(OutParamTextureImage())))
	{
		bool bAllInputsValid = true;
		bool bRecomputeRequired = (IsOutputAvailable(OutParamTextureImage()) == false);
		TSafeSharedPtr<IData> BakeCacheArg = FindAndUpdateInputForEvaluate(InParamBakeCache(), DatasIn, bRecomputeRequired, bAllInputsValid);
		TSafeSharedPtr<IData> ImageArg = FindAndUpdateInputForEvaluate(InParamImage(), DatasIn, bRecomputeRequired, bAllInputsValid);
		TSafeSharedPtr<IData> SettingsArg = FindAndUpdateInputForEvaluate(InParamSettings(), DatasIn, bRecomputeRequired, bAllInputsValid);
		if (bAllInputsValid)
		{
			if (bRecomputeRequired)
			{
				// always make a copy of settings
				FBakeMeshTextureImageSettings Settings;
				SettingsArg->GetDataCopy(Settings, FBakeMeshTextureImageSettings::DataTypeIdentifier);

				const FMeshBakingCache& BakeCacheContainer = BakeCacheArg->GetDataConstRef<FMeshBakingCache>((int)EMeshProcessingDataTypes::BakingCache);
				const FDynamicMesh3& DetailMesh = BakeCacheContainer.DetailMesh;
				check(DetailMesh.HasAttributes() && DetailMesh.Attributes()->NumUVLayers() > Settings.DetailUVLayer);
				const FDynamicMeshUVOverlay* DetailUVOverlay = DetailMesh.Attributes()->GetUVLayer(Settings.DetailUVLayer);

				const FTextureImage& SourceImage = ImageArg->GetDataConstRef<FTextureImage>(FTextureImage::DataTypeIdentifier);

				FMeshResampleImageBaker Baker;
				Baker.SetCache(&BakeCacheContainer.BakeCache);
				Baker.DetailUVOverlay = DetailUVOverlay;
				Baker.SampleFunction = [&](FVector2d UVCoord) {
					return SourceImage.Image.BilinearSampleUV<float>(UVCoord, FVector4f(0, 0, 0, 1));
				};
				Baker.Bake();

				FTextureImage ResultImage;
				ResultImage.Image = MoveTemp(*Baker.TakeResult());

				SetOutput(OutParamTextureImage(), MakeMovableData<FTextureImage>(MoveTemp(ResultImage)) );

				EvaluationInfo->CountCompute(this);
			}
			DatasOut.SetData(OutParamTextureImage(), GetOutput(OutParamTextureImage()));
		}
	}
}