// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmoothingOpBase.h"
#include "CoreMinimal.h"

namespace UE
{
namespace Geometry
{

class MODELINGOPERATORS_API  FIterativeSmoothingOp : public FSmoothingOpBase
{
public:
	FIterativeSmoothingOp(const FDynamicMesh3* Mesh, const FSmoothingOpBase::FOptions& OptionsIn);

	~FIterativeSmoothingOp() override {};

	// Apply smoothing. results in an updated ResultMesh
	void CalculateResult(FProgressCancel* Progress) override;

private:

	double GetSmoothAlpha(int32 VertexID, bool bIsBoundary);

	// uniform iterative smoothing
	void Smooth_Forward(bool bUniform);

	// cotan smoothing iterations
	void Smooth_Implicit_Cotan();

	// mean value smoothing iterations
	void Smooth_MeanValue();
};

} // end namespace UE::Geometry
} // end namespace UE
