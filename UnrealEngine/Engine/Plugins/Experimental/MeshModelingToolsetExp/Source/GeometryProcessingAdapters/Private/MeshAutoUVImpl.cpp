// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryProcessing/MeshAutoUVImpl.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "DynamicMeshToMeshDescription.h"
#include "ParameterizationOps/ParameterizeMeshOp.h"


using namespace UE::Geometry;


IGeometryProcessing_MeshAutoUV::FOptions FMeshAutoUVImpl::ConstructDefaultOptions()
{
	return IGeometryProcessing_MeshAutoUV::FOptions();
}

void FMeshAutoUVImpl::GenerateUVs(FMeshDescription& InOutMesh, const IGeometryProcessing_MeshAutoUV::FOptions& Options, IGeometryProcessing_MeshAutoUV::FResults& ResultsOut)
{
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> DynamicMesh = MakeShared<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe>();
	FMeshDescriptionToDynamicMesh MeshDescriptionToDynamicMesh;
	MeshDescriptionToDynamicMesh.Convert(&InOutMesh, *DynamicMesh);

	UE::Geometry::FParameterizeMeshOp ParameterizeMeshOp;
	ParameterizeMeshOp.InputMesh = DynamicMesh;
	ParameterizeMeshOp.Stretch = Options.UVAtlasStretch;
	ParameterizeMeshOp.NumCharts = Options.UVAtlasNumCharts;
	ParameterizeMeshOp.XAtlasMaxIterations = Options.XAtlasMaxIterations;

	ParameterizeMeshOp.InitialPatchCount = Options.NumInitialPatches;
	ParameterizeMeshOp.PatchCurvatureAlignmentWeight = Options.CurvatureAlignment;
	ParameterizeMeshOp.PatchMergingMetricThresh = Options.MergingThreshold;
	ParameterizeMeshOp.PatchMergingAngleThresh = Options.MaxAngleDeviationDeg;
	ParameterizeMeshOp.ExpMapNormalSmoothingSteps = Options.SmoothingSteps;
	ParameterizeMeshOp.ExpMapNormalSmoothingAlpha = Options.SmoothingAlpha;

	ParameterizeMeshOp.bEnablePacking = Options.bAutoPack;
	ParameterizeMeshOp.Width = ParameterizeMeshOp.Height = Options.PackingTargetWidth;

	if (Options.Method == IGeometryProcessing_MeshAutoUV::EAutoUVMethod::UVAtlas)
	{
		ParameterizeMeshOp.Method = UE::Geometry::EParamOpBackend::UVAtlas;
	}
	else if (Options.Method == IGeometryProcessing_MeshAutoUV::EAutoUVMethod::XAtlas)
	{
		ParameterizeMeshOp.Method = UE::Geometry::EParamOpBackend::XAtlas;
	}
	else
	{
		ParameterizeMeshOp.Method = UE::Geometry::EParamOpBackend::PatchBuilder;
	}

	ParameterizeMeshOp.CalculateResult(nullptr);

	TUniquePtr<UE::Geometry::FDynamicMesh3> ResultMesh = ParameterizeMeshOp.ExtractResult();

	ResultsOut.ResultCode = IGeometryProcessing_MeshAutoUV::EResultCode::Success;

	FConversionToMeshDescriptionOptions ConversionOptions;
	ConversionOptions.bSetPolyGroups = false;
	ConversionOptions.bUpdatePositions = false;
	ConversionOptions.bUpdateNormals = false;
	ConversionOptions.bUpdateTangents = false;
	ConversionOptions.bUpdateUVs = true;
	ConversionOptions.bUpdateVtxColors = false;
	ConversionOptions.bTransformVtxColorsSRGBToLinear = false;

	FDynamicMeshToMeshDescription Converter(ConversionOptions);
	Converter.UpdateUsingConversionOptions(ResultMesh.Get(), InOutMesh);
}