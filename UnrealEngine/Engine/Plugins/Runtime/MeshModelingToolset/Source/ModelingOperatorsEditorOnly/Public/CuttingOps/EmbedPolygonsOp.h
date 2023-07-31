// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/ProgressCancel.h"
#include "ModelingOperators.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Polygon2.h"

#include "EmbedPolygonsOp.generated.h"


UENUM()
enum class EEmbeddedPolygonOpMethod : uint8
{
	TrimOutside,
	TrimInside,
	InsertPolygon,
	CutThrough
};

namespace UE
{
namespace Geometry
{


class MODELINGOPERATORSEDITORONLY_API FEmbedPolygonsOp : public FDynamicMeshOperator
{
public:
	virtual ~FEmbedPolygonsOp() {}

	
	// inputs
	FFrame3d PolygonFrame;
	FPolygon2d EmbedPolygon;
	bool bCutWithBoolean;
	bool bAttemptFixHolesOnBoolean;

	// TODO: switch to FGeneralPolygon2d?
	FPolygon2d GetPolygon()
	{
		return EmbedPolygon;
	}

	bool bDiscardAttributes;

	EEmbeddedPolygonOpMethod Operation;

	//float ExtrudeDistance; // TODO if we support extrude

	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;

	//
	// FDynamicMeshOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;


	// Outputs
	TArray<int> EdgesOnFailure; // edges to highlight on failure, to help visualize what happened (partial cut edges or hole edges)
	TArray<int> EmbeddedEdges;
	bool bOperationSucceeded;

private:
	void RecordEmbeddedEdges(TArray<int>& PathVertIDs);

	void BooleanPath(FProgressCancel* Progress);
};

} // end namespace UE::Geometry
} // end namespace UE

