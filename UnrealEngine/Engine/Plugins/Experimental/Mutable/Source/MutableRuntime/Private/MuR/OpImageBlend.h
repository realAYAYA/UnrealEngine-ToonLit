// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "Math/VectorRegister.h"

namespace mu
{

	inline uint32 BlendChannel( uint32 // base
		, uint32 blended )
	{
		return blended;
	}

	inline uint32 BlendChannelMasked( uint32 base, uint32 blended, uint32 mask )
	{
		uint32 blend = blended;
		uint32 masked = ( ( ( 255 - mask ) * base ) + ( mask * blend ) ) / 255;
		return masked;
	}


	//---------------------------------------------------------------------------------------------
	inline uint32 ScreenChannelMasked(uint32 base, uint32 blended, uint32 mask)
	{
		// R = 1 - (1-Base) × (1-Blend)
		uint32 screen = 255 - (((255 - base) * (255 - blended)) >> 8);
		uint32 masked = (((255 - mask) * base) + (mask * screen)) >> 8;
		return masked;
	}

	inline uint32 ScreenChannel(uint32 base, uint32 blended)
	{
		// R = 1 - (1-Base) × (1-Blend)
		uint32 screen = 255 - (((255 - base) * (255 - blended)) >> 8);
		return screen;
	}

	//---------------------------------------------------------------------------------------------
	inline uint32 SoftLightChannel(uint32 base, uint32 blended)
	{
		// gimp-like
		uint32 mix = (base * blended) >> 8;
		uint32 screen = 255 - (((255 - base) * (255 - blended)) >> 8);
		uint32 softlight = (((255 - base) * mix) + (base * screen)) >> 8;
		return softlight;
	}


	inline uint32 SoftLightChannelMasked(uint32 base, uint32 blended, uint32 mask)
	{
		uint32 softlight = SoftLightChannel(base, blended);
		uint32 masked = (((255 - mask) * base) + (mask * softlight)) >> 8;
		return masked;
	}

	//---------------------------------------------------------------------------------------------
	inline uint32 HardLightChannel(uint32 base, uint32 blended)
	{
		// gimp-like


		// photoshop-like
		// if (Blend > ½) R = 1 - (1-Base) × (1-2×(Blend-½))
		// if (Blend <= ½) R = Base × (2×Blend)
		uint32 hardlight = blended > 128
			? 255 - (((255 - base) * (255 - 2 * (blended - 128))) >> 8)
			: (base * (2 * blended)) >> 8;

		hardlight = FMath::Min(255u, hardlight);

		return hardlight;
	}

	inline uint32 HardLightChannelMasked(uint32 base, uint32 blended, uint32 mask)
	{
		uint32 hardlight = HardLightChannel(base, blended);

		uint32 masked = (((255 - mask) * base) + (mask * hardlight)) >> 8;
		return masked;
	}

	//---------------------------------------------------------------------------------------------
	inline uint32 BurnChannel(uint32 base, uint32 blended)
	{
		// R = 1 - (1-Base) / Blend
		uint32 burn =
			FMath::Min(255,
				FMath::Max(0,
					255 - (((255 - (int)base) << 8) / ((int)blended + 1))
				)
			);
		return burn;
	}

	inline uint32 BurnChannelMasked(uint32 base, uint32 blended, uint32 mask)
	{
		uint32 burn = BurnChannel(base, blended);
		uint32 masked = (((255 - mask) * base) + (mask * burn)) >> 8;
		return masked;
	}

	//---------------------------------------------------------------------------------------------
	inline uint32 DodgeChannelMasked(uint32 base, uint32 blended, uint32 mask)
	{
		// R = Base / (1-Blend)
		uint32 dodge = (base << 8) / (256 - blended);
		uint32 masked = (((255 - mask) * base) + (mask * dodge)) >> 8;
		return masked;
	}

	inline uint32 DodgeChannel(uint32 base, uint32 blended)
	{
		// R = Base / (1-Blend)
		uint32 dodge = (base << 8) / (256 - blended);
		return dodge;
	}

	//---------------------------------------------------------------------------------------------
	inline uint32 LightenChannelMasked(uint32 base, uint32 blended, uint32 mask)
	{
		uint32 overlay = base + (blended * uint32(255 - base) >> 8);
		uint32 masked = (((255 - mask) * base) + (mask * overlay)) >> 8;
		return masked;
	}	

	inline uint32 LightenChannel(uint32 base, uint32 blended)
	{
		uint32 overlay = base + (blended * uint32(255 - base) >> 8);
		return overlay;
	}

	//---------------------------------------------------------------------------------------------
	inline uint32 MultiplyChannelMasked(uint32 base, uint32 blended, uint32 mask)
	{
		uint32 multiply = (base * blended) >> 8;
		uint32 masked = (((255 - mask) * base) + (mask * multiply)) >> 8;
		return masked;
	}

	inline uint32 MultiplyChannel(uint32 base, uint32 blended)
	{
		uint32 multiply = (base * blended) >> 8;
		return multiply;
	}

	//---------------------------------------------------------------------------------------------
	inline uint32 OverlayChannelMasked(uint32 base, uint32 blended, uint32 mask)
	{
		uint32 overlay = (base * (base + ((2 * blended * (255 - base)) >> 8))) >> 8;
		uint32 masked = (((255 - mask) * base) + (mask * overlay)) >> 8;
		return masked;
	}


	inline uint32 OverlayChannel(uint32 base, uint32 blended)
	{
		uint32 overlay = (base * (base + ((2 * blended * (255 - base)) >> 8))) >> 8;
		return overlay;
	}

	//---------------------------------------------------------------------------------------------
	FORCEINLINE VectorRegister4Int VectorBlendChannelMasked(
		const VectorRegister4Int& Base, const VectorRegister4Int& Blended, const VectorRegister4Int& Mask)
	{
		const VectorRegister4Int Value = VectorIntAdd(
				VectorIntMultiply(Base, VectorIntSubtract(MakeVectorRegisterIntConstant(255, 255, 255, 255), Mask)),
				VectorIntMultiply(Blended, Mask));

		// fast division by 255 assuming Value is in the range [0, (1 << 16)]
		return VectorShiftRightImmLogical(
				VectorIntMultiply(Value, MakeVectorRegisterIntConstant(32897, 32897, 32897, 32897)),
				23);
	}

	FORCEINLINE int32 VectorLightenChannel(int32 Base, int32 Blended)
	{
		return Base + (Blended * (255 - Base) >> 8);
	}

}
