// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/ProgressCancel.h"
#include "ModelingOperators.h"

#include "BaseOps/VoxelBaseOp.h"

namespace UE
{
namespace Geometry
{

class MODELINGOPERATORS_API FVoxelBlendMeshesOp : public FVoxelBaseOp
{
public:
	virtual ~FVoxelBlendMeshesOp() {}

	// inputs
	TArray<TSharedPtr<const FDynamicMesh3, ESPMode::ThreadSafe>> Meshes;
	TArray<FTransform> Transforms; // 1:1 with Meshes

	double BlendFalloff = 10;
	double BlendPower = 2;

	bool bVoxWrap = false;
	bool bRemoveInternalsAfterVoxWrap = false;
	double ThickenShells = 0.0;

	bool bSubtract = false;

	void SetTransform(const FTransformSRT3d& Transform);

	//
	// FDynamicMeshOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;

};


} // end namespace UE::Geometry
} // end namespace UE
