// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sampling/MeshMapEvaluator.h"
#include "Sampling/MeshBakerCommon.h"
#include "Image/ImageBuilder.h"

namespace UE
{
namespace Geometry
{

class IMeshBakerDetailSampler;	

enum class EMeshPropertyMapType
{
	Position = 1,
	Normal = 2,
	FacetNormal = 3,
	UVPosition = 4,
	MaterialID = 5,
	VertexColor = 6
};

/**
 * A mesh evaluator for mesh properties as color data.
 */
class DYNAMICMESH_API FMeshPropertyMapEvaluator : public FMeshMapEvaluator
{
public:
	EMeshPropertyMapType Property = EMeshPropertyMapType::Normal;

public:
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMeshPropertyMapEvaluator() = default;
	FMeshPropertyMapEvaluator(const FMeshPropertyMapEvaluator&) = default;
	FMeshPropertyMapEvaluator& operator=(const FMeshPropertyMapEvaluator&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Begin FMeshMapEvaluator interface
	virtual void Setup(const FMeshBaseBaker& Baker, FEvaluationContext& Context) override;

	virtual const TArray<EComponents>& DataLayout() const override;

	virtual EMeshMapEvaluatorType Type() const override { return EMeshMapEvaluatorType::Property; }
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
	FAxisAlignedBox3d Bounds;
	FVector3f DefaultValue = FVector3f::Zero();

private:
	static FVector3f NormalToColor(const FVector3d Normal) 
	{
		return (FVector3f)((Normal + FVector3d::One()) * 0.5);
	}

	static FVector3f UVToColor(const FVector2f UV)
	{
		const float X = FMathf::Clamp(UV.X, 0.0, 1.0);
		const float Y = FMathf::Clamp(UV.Y, 0.0, 1.0);
		return FVector3f(X, Y, 0);
	}

	static FVector3f PositionToColor(const FVector3d Position, const FAxisAlignedBox3d SafeBounds)
	{
		double X = (Position.X - SafeBounds.Min.X) / SafeBounds.Width();
		double Y = (Position.Y - SafeBounds.Min.Y) / SafeBounds.Height();
		double Z = (Position.Z - SafeBounds.Min.Z) / SafeBounds.Depth();
		return (FVector3f)FVector3d(X, Y, Z);
	}

	template <bool bUseDetailNormalMap>
	FVector3f SampleFunction(const FCorrespondenceSample& SampleData) const;
};

} // end namespace UE::Geometry
} // end namespace UE

