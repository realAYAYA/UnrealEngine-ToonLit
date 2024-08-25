// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversionUtils/DynamicMeshViaMeshDescriptionUtil.h"

#include "TargetInterfaces/MeshDescriptionCommitter.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "DynamicMeshToMeshDescription.h"
#include "MeshConversionOptions.h" //FConversionToMeshDescriptionOptions
#include "MeshDescriptionToDynamicMesh.h"

FDynamicMesh3 UE::Geometry::GetDynamicMeshViaMeshDescription(
	IMeshDescriptionProvider& MeshDescriptionProvider, bool bRequestTangents)
{
	FDynamicMesh3 DynamicMesh;
	FMeshDescriptionToDynamicMesh Converter;
	Converter.bVIDsFromNonManifoldMeshDescriptionAttr = true;
	if (bRequestTangents)
	{
		FGetMeshParameters GetMeshParams;
		GetMeshParams.bWantMeshTangents = true;
		FMeshDescription MeshDescriptionCopy = MeshDescriptionProvider.GetMeshDescriptionCopy(GetMeshParams);
		Converter.Convert(&MeshDescriptionCopy, DynamicMesh, bRequestTangents);
	}
	else
	{
		Converter.Convert(MeshDescriptionProvider.GetMeshDescription(), DynamicMesh, bRequestTangents);
	}
	return DynamicMesh;
}

void UE::Geometry::CommitDynamicMeshViaMeshDescription(
	FMeshDescription&& CurrentMeshDescription,
	IMeshDescriptionCommitter& MeshDescriptionCommitter, 
	const FDynamicMesh3& Mesh, const IDynamicMeshCommitter::FDynamicMeshCommitInfo& CommitInfo)
{
	FConversionToMeshDescriptionOptions ConversionOptions;
	ConversionOptions.bSetPolyGroups = CommitInfo.bPolygroupsChanged;
	ConversionOptions.bUpdatePositions = CommitInfo.bPositionsChanged;
	ConversionOptions.bUpdateNormals = CommitInfo.bNormalsChanged;
	ConversionOptions.bUpdateTangents = CommitInfo.bTangentsChanged;
	ConversionOptions.bUpdateUVs = CommitInfo.bUVsChanged;
	ConversionOptions.bUpdateVtxColors = CommitInfo.bVertexColorsChanged;
	ConversionOptions.bTransformVtxColorsSRGBToLinear = CommitInfo.bTransformVertexColorsSRGBToLinear;

	FDynamicMeshToMeshDescription Converter(ConversionOptions);
	if (!CommitInfo.bTopologyChanged)
	{
		Converter.UpdateUsingConversionOptions(&Mesh, CurrentMeshDescription);
	}
	else
	{
		// Do a full conversion.
		Converter.Convert(&Mesh, CurrentMeshDescription);
	}

	MeshDescriptionCommitter.CommitMeshDescription(MoveTemp(CurrentMeshDescription));
}