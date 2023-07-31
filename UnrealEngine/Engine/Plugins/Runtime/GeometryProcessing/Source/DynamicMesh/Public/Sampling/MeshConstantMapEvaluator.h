// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sampling/MeshMapEvaluator.h"
#include "DynamicMesh/MeshTangents.h"

namespace UE
{
namespace Geometry
{

/**
 * A mesh evaluator for constant data. This evaluator can be useful as a filler
 * when computing per channel color data.
 */
class DYNAMICMESH_API FMeshConstantMapEvaluator : public FMeshMapEvaluator
{
public:
	FMeshConstantMapEvaluator() = default;
	explicit FMeshConstantMapEvaluator(float ValueIn);
	
	// Begin FMeshMapEvaluator interface
	virtual void Setup(const FMeshBaseBaker& Baker, FEvaluationContext& Context) override;

	virtual const TArray<EComponents>& DataLayout() const override;

	virtual EMeshMapEvaluatorType Type() const override { return EMeshMapEvaluatorType::Constant; }
	// End FMeshMapEvaluator interface

	static void EvaluateSample(float*& Out, const FCorrespondenceSample& Sample, void* EvalData);

	static void EvaluateDefault(float*& Out, void* EvalData);

	static void EvaluateColor(const int DataIdx, float*& In, FVector4f& Out, void* EvalData);

public:
	float Value = 0.0f;
};

} // end namespace UE::Geometry
} // end namespace UE

