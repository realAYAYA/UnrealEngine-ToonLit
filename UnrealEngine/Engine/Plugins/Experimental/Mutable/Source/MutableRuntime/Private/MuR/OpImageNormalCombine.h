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

		vec3f a = vec3f(baseRf, baseGf, baseBf);
		vec3f b = vec3f(-blendedRf, -blendedGf, blendedBf);

		// The original does not normalize but if not done results don't look good due to signal clipping.
		vec3f n = normalise_approx(a*dot(a, b) - b*a.z());

		return 
			(mu::clamp(static_cast<unsigned>((n.x() + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 0) |
			(mu::clamp(static_cast<unsigned>((n.y() + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 8) |
			(mu::clamp(static_cast<unsigned>((n.z() + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 16);
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

		vec3f a = vec3f(baseRf, baseGf, baseBf + 1.0f);
		vec3f b = vec3f(-blendedRf, -blendedGf, blendedBf);

		// The original does not normalize but if not done results don't look good due to signal clipping.
		vec3f n = mu::normalise_approx( 
					mu::lerp( vec3f(baseRf, baseGf, baseBf), a*dot(a, b) - b*a.z(), maskRf ) );

		return 
			(mu::clamp(static_cast<unsigned>((n.x() + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 0) |
			(mu::clamp(static_cast<unsigned>((n.y() + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 8) |
			(mu::clamp(static_cast<unsigned>((n.z() + 1.0f) * 0.5f * 255.0f), 0u, 255u) << 16);
	}

	inline void ImageNormalCombine(Image* pResult, const Image* pBase, vec3<float> col)
	{
		ImageLayerCombineColour<CombineNormal>(pResult, pBase, col);
	}

	inline void ImageNormalCombine(Image* pResult, const Image* pBase, const Image* pBlended)
	{
		ImageLayerCombine<CombineNormal>(pResult, pBase, pBlended);
	}

	inline void ImageNormalCombine(Image* pResult, const Image* pBase, const Image* pMask, vec3<float> col)
	{
		ImageLayerCombineColour<CombineNormal, CombineNormalMasked>(pResult, pBase, pMask, col);
	}

	inline void ImageNormalCombine(Image* pResult, const Image* pBase, const Image* pMask, const Image* pBlended)
	{
		ImageLayerCombine<CombineNormal, CombineNormalMasked>(pResult, pBase, pMask, pBlended);
	}
}
