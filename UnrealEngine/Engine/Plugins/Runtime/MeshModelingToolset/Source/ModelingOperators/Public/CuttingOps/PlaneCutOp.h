// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/ProgressCancel.h"
#include "ModelingOperators.h"


namespace UE
{
namespace Geometry
{


class MODELINGOPERATORS_API FPlaneCutOp : public FDynamicMeshOperator
{
public:
	virtual ~FPlaneCutOp() {}

	// inputs
	FVector3d LocalPlaneOrigin, LocalPlaneNormal;
	bool bFillCutHole = true;
	bool bFillSpans = false;
	bool bKeepBothHalves = false;
	double CutPlaneLocalThickness = 0; // plane thickness in the local space of the mesh
	double UVScaleFactor = 0;
	static const FName ObjectIndexAttribute;
	TSharedPtr<const FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;

	void SetTransform(const FTransformSRT3d& Transform);

	//
	// FDynamicMeshOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;
};

} // end namespace UE::Geometry
} // end namespace UE

