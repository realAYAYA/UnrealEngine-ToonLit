// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Tuple.h"
#include "Containers/Map.h"
#include "Sampling/MeshMapEvaluator.h"
#include "Sampling/MeshBakerCommon.h"
#include "DynamicMesh/MeshTangents.h"

namespace UE
{
namespace Geometry
{

/**
 * A mesh evaluator for tangent space normals.
 */
class DYNAMICMESH_API FMeshNormalMapEvaluator : public FMeshMapEvaluator
{
public:
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMeshNormalMapEvaluator() = default;
	FMeshNormalMapEvaluator(const FMeshNormalMapEvaluator&) = default;
	FMeshNormalMapEvaluator& operator=( const FMeshNormalMapEvaluator& ) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Begin FMeshMapEvaluator interface
	virtual void Setup(const FMeshBaseBaker& Baker, FEvaluationContext& Context) override;

	virtual const TArray<EComponents>& DataLayout() const override;

	virtual EMeshMapEvaluatorType Type() const override { return EMeshMapEvaluatorType::Normal; }
	// End FMeshMapEvaluator interface

	template <bool bUseDetailNormalMap>
	static void EvaluateSample(float*& Out, const FCorrespondenceSample& Sample, void* EvalData);

	static void EvaluateDefault(float*& Out, void* EvalData);

	static void EvaluateColor(const int DataIdx, float*& In, FVector4f& Out, void* EvalData);

protected:
	// Cached data
	const IMeshBakerDetailSampler* DetailSampler = nullptr;
	using FDetailNormalTexture UE_DEPRECATED(5.1, "Use FNormalTexture instead.") = IMeshBakerDetailSampler::FBakeDetailTexture;
	using FDetailNormalTextureMap UE_DEPRECATED(5.1, "Use FNormalTextureMap instead.") = TMap<const void*, IMeshBakerDetailSampler::FBakeDetailTexture>;
	UE_DEPRECATED(5.1, "Use DetailNormalMaps instead.")
	TMap<const void*, IMeshBakerDetailSampler::FBakeDetailTexture> DetailNormalTextures;

	using FNormalTexture = IMeshBakerDetailSampler::FBakeDetailNormalTexture;
	using FNormalTextureMap = TMap<const void*, FNormalTexture>;
	FNormalTextureMap DetailNormalMaps;
	
	bool bHasDetailNormalTextures = false;
	const TMeshTangents<double>* BaseMeshTangents = nullptr;

	FVector3f DefaultNormal = FVector3f::UnitZ();

private:
	template <bool bUseDetailNormalMap>
	FVector3f SampleFunction(const FCorrespondenceSample& SampleData) const;
};

} // end namespace UE::Geometry
} // end namespace UE

