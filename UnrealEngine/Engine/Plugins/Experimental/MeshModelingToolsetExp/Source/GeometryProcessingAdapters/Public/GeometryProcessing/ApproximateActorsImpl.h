// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryProcessingInterfaces/ApproximateActors.h"

class UMaterialInterface;

namespace UE
{
namespace Geometry
{

class FDynamicMesh3;

/**
 * Implementation of IGeometryProcessing_ApproximateActors
 */
class GEOMETRYPROCESSINGADAPTERS_API FApproximateActorsImpl : public IGeometryProcessing_ApproximateActors
{
public:
	virtual FOptions ConstructOptions(const FMeshApproximationSettings& MeshApproximationSettings) override;

	virtual void ApproximateActors(const TArray<AActor*>& Actors, const FOptions& Options, FResults& ResultsOut) override;
	


protected:

	virtual void GenerateApproximationForActorSet(const TArray<AActor*>& Actors, const FOptions& Options, FResults& ResultsOut);

	virtual UStaticMesh* EmitGeneratedMeshAsset(
		const TArray<AActor*>& Actors,
		const FOptions& Options,
		FResults& ResultsOut,
		FDynamicMesh3* FinalMesh,
		UMaterialInterface* Material = nullptr,
		FDynamicMesh3* DebugMesh = nullptr);
};


}
}
