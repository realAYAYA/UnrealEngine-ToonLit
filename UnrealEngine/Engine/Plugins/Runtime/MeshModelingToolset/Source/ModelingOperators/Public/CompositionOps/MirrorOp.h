// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelingOperators.h"
#include "Util/ProgressCancel.h"

namespace UE
{
namespace Geometry
{

class MODELINGOPERATORS_API FMirrorOp : public FDynamicMeshOperator
{
public:
	virtual ~FMirrorOp() {}

	// Inputs
	TSharedPtr<const FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;
	FVector3d LocalPlaneOrigin;
	FVector3d LocalPlaneNormal;

	/** Tolerance to use when bCropFirst or bWeldAlongPlane is true. */
	double PlaneTolerance = FMathf::ZeroTolerance * 10.0;;

	/** Whether to crop the result first along the plane. */
	bool bCropFirst = true;

	/** If true, the mirrored portion is appended to the original. If false, the result is just the mirrored portion. */
	bool bAppendToOriginal = true;

	/** Whether vertices on the mirror plane should be welded. Only relevant if bAppendToOriginal is true. */
	bool bWeldAlongPlane = true;

	/** Whether, when welding, new bowtie vertex creation should be allowed. */
	bool bAllowBowtieVertexCreation = false;

	void SetTransform(const FTransformSRT3d& Transform);

	//
	// FDynamicMeshOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;

};

} // end namespace UE::Geometry
} // end namespace UE
