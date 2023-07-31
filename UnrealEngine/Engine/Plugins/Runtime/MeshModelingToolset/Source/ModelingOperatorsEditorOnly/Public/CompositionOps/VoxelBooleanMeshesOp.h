// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/ProgressCancel.h"
#include "ModelingOperators.h"

struct FMeshDescription;

namespace UE
{
namespace Geometry
{


class MODELINGOPERATORSEDITORONLY_API FVoxelBooleanMeshesOp : public FDynamicMeshOperator
{
public:
	virtual ~FVoxelBooleanMeshesOp() {}

	enum class EBooleanOperation
	{
		DifferenceAB = 0,
		DifferenceBA = 1,
		Intersect = 2,
		Union = 3
	};

	struct FInputMesh
	{
		const FMeshDescription* Mesh;
		FTransform Transform;
	};

	// inputs
	TArray<FInputMesh> InputMeshArray;
	int32 VoxelCount = 128;
	double VoxelSizeD = 1.0;
	double AdaptivityD = 0;
	double IsoSurfaceD = 0;
	bool   bAutoSimplify = false;
	EBooleanOperation Operation = EBooleanOperation::DifferenceBA;

	//
	// FDynamicMeshOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;

private:

	// compute the voxel size based on the voxel count and the input geometry
	float ComputeVoxelSize() const;

};

} // end namespace UE::Geometry
} // end namespace UE

