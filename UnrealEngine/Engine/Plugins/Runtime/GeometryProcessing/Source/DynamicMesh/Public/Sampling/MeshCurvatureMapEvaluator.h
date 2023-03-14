// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sampling/MeshMapEvaluator.h"

namespace UE
{
namespace Geometry
{

class IMeshBakerDetailSampler;
class FMeshVertexCurvatureCache;

/**
 * A mesh evaluator for mesh curvatures.
 */
class DYNAMICMESH_API FMeshCurvatureMapEvaluator : public FMeshMapEvaluator
{
public:
	enum class ECurvatureType
	{
		Mean = 0,
		Gaussian = 1,
		MaxPrincipal = 2,
		MinPrincipal = 3
	};
	ECurvatureType UseCurvatureType = ECurvatureType::Mean;

	enum class EColorMode
	{
		BlackGrayWhite = 0,
		RedGreenBlue = 1,
		RedBlue = 2
	};
	EColorMode UseColorMode = EColorMode::RedGreenBlue;

	enum class EClampMode
	{
		FullRange = 0,
		Positive = 1,
		Negative = 2
	};
	EClampMode UseClampMode = EClampMode::FullRange;

	double RangeScale = 1.0;
	double MinRangeScale = 0.0;

	// Allows override of the max curvature; if false, range is set based on [-(avg+stddev), avg+stddev]
	bool bOverrideCurvatureRange = false;
	double OverrideRangeMax = 0.1;

	// Required input data, can be provided, will be computed otherwise
	TSharedPtr<FMeshVertexCurvatureCache> Curvatures;

public:
	// Begin FMeshMapEvaluator interface
	virtual void Setup(const FMeshBaseBaker& Baker, FEvaluationContext& Context) override;

	virtual const TArray<EComponents>& DataLayout() const override;

	virtual EMeshMapEvaluatorType Type() const override { return EMeshMapEvaluatorType::Curvature; }
	// End FMeshMapEvaluator interface

	static void EvaluateSample(float*& Out, const FCorrespondenceSample& Sample, void* EvalData);

	static void EvaluateDefault(float*& Out, void* EvalData);

	static void EvaluateColor(const int DataIdx, float*& In, FVector4f& Out, void* EvalData);

	/** Populate Curvatures member if valid data has not been provided */
	void CacheDetailCurvatures();

protected:
	// Cached data
	const IMeshBakerDetailSampler* DetailSampler = nullptr;
	double MinPreClamp = -TNumericLimits<double>::Max();
	double MaxPreClamp = TNumericLimits<double>::Max();
	FInterval1d ClampRange;
	FVector3f NegativeColor;
	FVector3f ZeroColor;
	FVector3f PositiveColor;

protected:
	double GetCurvature(int32 vid) const;
	void GetColorMapRange(FVector3f& NegativeColorOut, FVector3f& ZeroColorOut, FVector3f& PositiveColorOut) const;

private:
	double SampleFunction(const FCorrespondenceSample& SampleData) const;
};

} // end namespace UE::Geometry
} // end namespace UE

