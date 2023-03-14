// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Logging/LogMacros.h"
#include "OverlappingCorners.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMeshBuilder, Log, All);

class UObject;
struct FMeshDescription;
struct FMeshBuildSettings;
struct FVertexInstanceID;
struct FMeshReductionSettings;

class MESHBUILDER_API FMeshDescriptionHelper
{
public:

	FMeshDescriptionHelper(FMeshBuildSettings* InBuildSettings);

	//Build a render mesh description with the BuildSettings. This will update the RenderMeshDescription in place
	void SetupRenderMeshDescription(UObject* Owner, FMeshDescription& RenderMeshDescription, bool bForNanite, bool bNeedTangents);

	void ReduceLOD(const FMeshDescription& BaseMesh, FMeshDescription& DestMesh, const struct FMeshReductionSettings& ReductionSettings, const FOverlappingCorners& InOverlappingCorners, float &OutMaxDeviation);

	void FindOverlappingCorners(const FMeshDescription& MeshDescription, float ComparisonThreshold);

	const FOverlappingCorners& GetOverlappingCorners() const { return OverlappingCorners; }

private:

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE function declarations

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE class members

	FMeshBuildSettings* BuildSettings;
	FOverlappingCorners OverlappingCorners;

	
	//////////////////////////////////////////////////////////////////////////
	//INLINE small helper use to optimize search and compare

	/**
	* Smoothing group interpretation helper structure.
	*/
	struct FFanFace
	{
		int32 FaceIndex;
		int32 LinkedVertexIndex;
		bool bFilled;
		bool bBlendTangents;
		bool bBlendNormals;
	};

};
