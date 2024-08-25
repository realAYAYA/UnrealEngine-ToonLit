// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Math/IntVector.h"

namespace mu
{
	class Image;
	class Mesh;
	struct FProjector;
	enum class ESamplingMethod : uint8;
	template <int NUM_INTERPOLATORS> class RasterVertex;

	struct FScratchImageProject
	{
		TArray<RasterVertex<4>> Vertices;
		TArray<uint8> CulledVertex;
	};

    extern void ImageRasterProjectedPlanar( const Mesh* pMesh, Image* pTargetImage,
		const Image* pSource, const Image* pMask,
		bool bIsRGBFadingEnabled, bool bIsAlphaFadingEnabled,
		ESamplingMethod SamplingMethod,
		float FadeStart, float FadeEnd, float MipInterpolationFactor,
		int Layout, int32 Block,
		UE::Math::TIntVector2<uint16> CropMin, UE::Math::TIntVector2<uint16> UncroppedSize,
		FScratchImageProject* scratch, bool bUseVectorImplementation = false);

    extern void ImageRasterProjectedCylindrical( const Mesh* pMesh, Image* pTargetImage,
		const Image* pSource, const Image* pMask,
		bool bIsRGBFadingEnabled, bool bIsAlphaFadingEnabled,
		ESamplingMethod SamplingMethod,
		float FadeStart, float FadeEnd, float MipInterpolationFactor,
		int32 Layout,
		float ProjectionAngle,
		UE::Math::TIntVector2<uint16> CropMin, UE::Math::TIntVector2<uint16> UncroppedSize,
		FScratchImageProject* Scratch, bool bUseVectorImplementation = false);

    extern void ImageRasterProjectedWrapping( const Mesh* pMesh, Image* pTargetImage,
		const Image* pSource, const Image* pMask,
		bool bIsRGBFadingEnabled, bool bIsAlphaFadingEnabled,
		ESamplingMethod SamplingMethod,
		float FadeStart, float FadeEnd, float MipInterpolationFactor,
		int32 Layout, int32 Block,
		UE::Math::TIntVector2<uint16> CropMin, UE::Math::TIntVector2<uint16> UncroppedSize,
		FScratchImageProject* Scratch, bool bUseVectorImplementation = false);

	extern float ComputeProjectedFootprintBestMip(
			const Mesh* pMesh, const FProjector& Projector, const FVector2f& TargetSize, const FVector2f& SourceSize);

    extern void MeshProject(Mesh* Result, const Mesh* pMesh, const FProjector& Projector, bool& bOutSuccess);

	MUTABLERUNTIME_API extern void CreateMeshOptimisedForProjection(Mesh* Result, int32 Layout);
	MUTABLERUNTIME_API extern void CreateMeshOptimisedForWrappingProjection(Mesh* Result, int32 Layout);

}
