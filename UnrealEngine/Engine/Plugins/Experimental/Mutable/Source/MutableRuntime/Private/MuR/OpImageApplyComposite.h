// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"

#include "MuR/ImagePrivate.h"
#include "MuR/OpImageLayer.h"


namespace mu
{
	// Combine layer oprations get packed color.
	inline unsigned NormalComposite(
			const uint32 Base, 
			const uint32 Normal, 
			const uint8 Channel, 
			const float Power )
	{
		const int32 ChannelShift = Channel << 3;

		float ReferencValue = static_cast<float>(Base >> ChannelShift & 0xFF) / 255.0f;

		// Rougness is stored as Unorm8
		float Roughness = static_cast<float>( (Base >> ChannelShift) & 0xFF ) / 255.0f;

		FVector3f N = ( FVector3f( static_cast<float>((Normal >> 0 ) & 0xFF) / 255.0f,
							       static_cast<float>((Normal >> 8 ) & 0xFF) / 255.0f,
							       static_cast<float>((Normal >> 16) & 0xFF) / 255.0f ) * 2.0f - 1.0f );	
	
		// See TexturecCompressorModule.cpp::ApplyCompositeTexture
		// Toksvig estimation of variance
		float LengthN = FMath::Min( N.Size(), 1.0f );
		float Variance = ( 1.0f - LengthN ) / LengthN;
		Variance = FMath::Max( 0.0f, Variance - 0.00004f );

		Variance *= Power;
		
		float a = Roughness * Roughness;
		float a2 = a * a;
		float B = 2.0f * Variance * (a2 - 1.0f);
		a2 = ( B - a2 ) / ( B - 1.0f );
		Roughness = FMath::Pow( a2, 0.25f );

		const uint32 ResMask = ~(0xFF << ChannelShift);
		const uint32 Value = FMath::Clamp<uint32>( static_cast<uint32>(Roughness * 255.0f), 0u, 255u );

		return  (Value << ChannelShift) | (Base & ResMask); 
	}

	inline void ImageNormalComposite(Image* pResult, const Image* pBase, const Image* pNormal, ECompositeImageMode mode, float power)
	{
		if (mode == ECompositeImageMode::CIM_Disabled)
		{
			// Copy base to result. Using functor layer combine with identity to do the copy for simplicity.
			// This could be optimized but will never happen since it is checked at compile time. 	
			ImageLayerCombineFunctor( pResult, pBase, pNormal, 
				[](uint32 base, uint32) -> uint32 { return base; });

			return;
		}	

		const int8 channel = [mode]() 
			{
				switch (mode)
				{
					case ECompositeImageMode::CIM_NormalRoughnessToRed   : return 0;
					case ECompositeImageMode::CIM_NormalRoughnessToGreen : return 1;
					case ECompositeImageMode::CIM_NormalRoughnessToBlue  : return 2;
					case ECompositeImageMode::CIM_NormalRoughnessToAlpha : return 3;
				}
			
				check(false);
				return 0;		
			}();

		ImageLayerCombineFunctor( pResult, pBase, pNormal,
				[channel, power](uint32 base, uint32 normal) -> uint32 
					{ return NormalComposite(base, normal, channel, power); });
	}
}
