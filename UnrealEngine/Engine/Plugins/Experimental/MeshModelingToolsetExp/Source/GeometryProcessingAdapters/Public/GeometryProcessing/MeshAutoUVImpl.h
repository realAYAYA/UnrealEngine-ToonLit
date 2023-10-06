// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryProcessingInterfaces/MeshAutoUV.h"


namespace UE
{
namespace Geometry
{

/**
 * Implementation of IGeometryProcessing_MeshAutoUV
 */
class GEOMETRYPROCESSINGADAPTERS_API FMeshAutoUVImpl : public IGeometryProcessing_MeshAutoUV
{
public:
	virtual FOptions ConstructDefaultOptions() override;

	virtual void GenerateUVs(FMeshDescription& InOutMesh, const FOptions& Options, FResults& ResultsOut) override;
};


}
}
