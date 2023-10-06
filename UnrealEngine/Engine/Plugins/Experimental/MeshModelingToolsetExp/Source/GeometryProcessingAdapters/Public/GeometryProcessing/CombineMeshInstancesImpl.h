// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryProcessingInterfaces/CombineMeshInstances.h"

class UMaterialInterface;

namespace UE
{
namespace Geometry
{

class FDynamicMesh3;

/**
 * Implementation of IGeometryProcessing_CombineMeshInstances
 */
class GEOMETRYPROCESSINGADAPTERS_API FCombineMeshInstancesImpl : public IGeometryProcessing_CombineMeshInstances
{
public:
	virtual FOptions ConstructDefaultOptions() override;

	virtual void CombineMeshInstances(const FSourceInstanceList& MeshInstances, const FOptions& Options, FResults& ResultsOut) override;

protected:

};


}
}
