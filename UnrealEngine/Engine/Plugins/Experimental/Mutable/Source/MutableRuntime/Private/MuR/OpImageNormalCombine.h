// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/OpImageLayer.h"

namespace mu
{
	// Combine layer oprations get packed color.
	inline unsigned CombineNormal(unsigned base, unsigned blended)
	{
		// See /Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals.BlendAngleCorrectedNormals
		// for the source of the effect.

		const float baseRf = (static_cast<float>((base >> 0)  & 0xFF) / 255.0f) * 2.0f - 1.0f;
		const float baseGf = (static_cast<float>((base >> 8)  & 0xFF) / 255.0f) * 2.0f - 1.0f;
		const float baseBf = (static_cast<float>((base >> 16) & 0xFF) / 255.0f) * 2.0f; //One added to the b channel.

		const float blendedRf = (static_cast<float>((blended >> 0)  & 0xFF) / 255.0f) * 2.0f - 1.0f;
		const float blendedGf = (static_cast<float>((blended >> 8)  & 0xFF) / 255.0f) * 2.0f - 1.0f;
		const float blendedBf = (static_cast<float>((blended >> 16) & 0xFF) / 255.0f) * 2.0f - 1.0f;

		FVector3f a = FVector3f(baseRf, baseGf, baseBf);
		FVector3f b = FVector3f(-blendedRf, -blendedGf, blendedBf);

		// The original does not normalize but if not done results don't look good due to signal clipping.
		FVector3f n = (a* FVector3f::DotProduct(a, b) - b*a.Z);
		n.Normalize();

		return 
			(FMath::Clamp(static_cast<unsigned>((n.X + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 0) |
			(FMath::Clamp(static_cast<unsigned>((n.Y + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 8) |
			(FMath::Clamp(static_cast<unsigned>((n.Z + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 16);
	}

	inline unsigned CombineNormalMasked(unsigned base, unsigned blended, unsigned mask)
	{
		// See /Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals.BlendAngleCorrectedNormals
		// for the source of the effect.

		const float baseRf = (static_cast<float>((base >> 0)  & 0xFF) / 255.0f) * 2.0f - 1.0f;
		const float baseGf = (static_cast<float>((base >> 8)  & 0xFF) / 255.0f) * 2.0f - 1.0f;
		const float baseBf = (static_cast<float>((base >> 16) & 0xFF) / 255.0f) * 2.0f - 1.0f;

		const float blendedRf = (static_cast<float>((blended >> 0)  & 0xFF) / 255.0f) * 2.0f - 1.0f;
		const float blendedGf = (static_cast<float>((blended >> 8)  & 0xFF) / 255.0f) * 2.0f - 1.0f;
		const float blendedBf = (static_cast<float>((blended >> 16) & 0xFF) / 255.0f) * 2.0f - 1.0f;

		const float maskRf = (static_cast<float>(mask & 0xFF) / 255.0f);

		FVector3f a = FVector3f(baseRf, baseGf, baseBf + 1.0f);
		FVector3f b = FVector3f(-blendedRf, -blendedGf, blendedBf);

		// The original does not normalize but if not done results don't look good due to signal clipping.
		FVector3f n = ( FMath::Lerp( FVector3f(baseRf, baseGf, baseBf), a*FVector3f::DotProduct(a, b) - b*a.Z, maskRf ) );
		n.Normalize();

		return 
			(FMath::Clamp(static_cast<unsigned>((n.X + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 0) |
			(FMath::Clamp(static_cast<unsigned>((n.Y + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 8) |
			(FMath::Clamp(static_cast<unsigned>((n.Z + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 16);
	}

	inline void ImageNormalCombine(Image* pResult, const Image* pBase, FVector4f col)
	{
		ImageLayerCombineColour<CombineNormal>(pResult, pBase, col);
	}

	inline void ImageNormalCombine(Image* pResult, const Image* pBase, const Image* pBlended, bool bOnlyFirstLOD)
	{
		ImageLayerCombine<CombineNormal>(pResult, pBase, pBlended, bOnlyFirstLOD);
	}

	inline void ImageNormalCombine(Image* pResult, const Image* pBase, const Image* pMask, FVector4f col)
	{
		ImageLayerCombineColour<CombineNormal, CombineNormalMasked>(pResult, pBase, pMask, col);
	}

	inline void ImageNormalCombine(Image* pResult, const Image* pBase, const Image* pMask, const Image* pBlended, bool bOnlyFirstLOD)
	{
		ImageLayerCombine<CombineNormal, CombineNormalMasked>(pResult, pBase, pMask, pBlended, bOnlyFirstLOD);
	}
}
