// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpImageTransformPrivate.h"
#include "Math/Vector.h"
#include "Math/IntVector.h"

namespace mu
{

    //---------------------------------------------------------------------------------------------
    //! Image transform, translate, scale, rotate 
    //---------------------------------------------------------------------------------------------

    template<int32 NC, EAddressMode AddressMode>
	void ImageTransformImpl(Image* pDest, const Image* pBase, FVector2f Offset, FVector2f Scale, float RotationRad)
    {
		using FUInt16Vector2 = UE::Math::TIntVector2<uint16>;
		struct FPixelData
		{
			alignas(8) uint16 Data[NC];
		};

		FUInt16Vector2 BaseSize = FUInt16Vector2(pBase->GetSizeX(), pBase->GetSizeY());
		FUInt16Vector2 DestSize = FUInt16Vector2(pDest->GetSizeX(), pDest->GetSizeY());

		const FVector2f BaseSizeF = FVector2f(BaseSize.X, BaseSize.Y);
		const FVector2f DestSizeF = FVector2f(DestSize.X, DestSize.Y);

		float SinRot, CosRot;
		FMath::SinCos(&SinRot, &CosRot, RotationRad);

		const FVector2f DestNormFactor = FVector2f(1.0f) / DestSizeF;
		const FVector2f BaseNormFactor = FVector2f(1.0f) / BaseSizeF;

		Scale.X = FMath::IsNearlyZero(Scale.X) ? UE_SMALL_NUMBER : Scale.X;
		Scale.Y = FMath::IsNearlyZero(Scale.Y) ? UE_SMALL_NUMBER : Scale.Y;

		const FVector2f ScaleReciprocal = FVector2f(1.0f) / Scale; 
		const FVector2f ScaledOffset = Offset * ScaleReciprocal;

		for (int32 Y = 0; Y < DestSize.Y; ++Y)
		{
			for (int32 X = 0; X < DestSize.X; ++X)
			{
				FVector2f DestUv = (FVector2f(X, Y) + 0.5f) * DestNormFactor - 0.5f;	

				DestUv -= Offset;
				DestUv = FVector2f(CosRot * DestUv.X + SinRot * DestUv.Y,
								   CosRot * DestUv.Y - SinRot * DestUv.X);
				DestUv *= ScaleReciprocal;
				DestUv = DestUv + 0.5f;

				// TODO: Make the bloack edge antialiazed.
				if constexpr (AddressMode == EAddressMode::ClampToBlack)
				{
					if ((DestUv.X < 0.0f) | (DestUv.X > 1.0f) | (DestUv.Y < 0.0f) | (DestUv.Y > 1.0f))
					{
						for (uint32 Channel = 0; Channel < NC; ++Channel)
						{
							pDest->GetData()[(Y*DestSize.X + X)*NC + Channel] = 0;
						}

						continue;
					}
				}

				const FVector2f Uv = Invoke([&]() -> FVector2f
				{
					if constexpr (AddressMode == EAddressMode::Wrap)
					{
						return FVector2f(FMath::Frac(DestUv.X), FMath::Frac(DestUv.Y));
					}
					else if (AddressMode == EAddressMode::ClampToBlack || AddressMode == EAddressMode::ClampToEdge)
					{
						return FVector2f(FMath::Clamp(DestUv.X, 0.0f, 1.0f), FMath::Clamp(DestUv.Y, 0.0f, 1.0f));
					}
					else
					{
						return DestUv;
					}
				});

				auto SampleImageBilinear = [&](FVector2f Uv, FUInt16Vector2 Size, const uint8* DataPtr)
				{
					auto ComputeInterpolator = [](float T) -> uint16
					{
						return static_cast<uint16>(255.0f * T);
					};

					const FVector2f SizeF(Size.X, Size.Y);

					const FVector2f CoordsF = FVector2f(
						FMath::Clamp(Uv.X * SizeF.X - 0.5f, 0.0f, SizeF.X - 1.0f),
						FMath::Clamp(Uv.Y * SizeF.Y - 0.5f, 0.0f, SizeF.Y - 1.0f));

					const FUInt16Vector2 Frac = FUInt16Vector2(
						ComputeInterpolator(FMath::Frac(CoordsF.X)),
						ComputeInterpolator(FMath::Frac(CoordsF.Y)));

					const FIntVector2 Coords = FIntVector2(CoordsF.X, CoordsF.Y);
					const FIntVector2 CoordsPlusOne = Invoke([&]() -> FIntVector2
					{	
						if constexpr (AddressMode == EAddressMode::Wrap)
						{
							return FIntVector2(Coords.X + 1 < Size.X ? Coords.X + 1 : 0, 
											   Coords.Y + 1 < Size.Y ? Coords.Y + 1 : 0);
						}
						else if (AddressMode == EAddressMode::ClampToBlack || AddressMode == EAddressMode::ClampToEdge)
						{
							return FIntVector2(FMath::Min(Size.X - 1, Coords.X + 1), 
											   FMath::Min(Size.Y - 1, Coords.Y + 1));
						}
					});
						
					uint8 const * const Pixel00Ptr = DataPtr + (Coords.Y        * Size.X + Coords.X)        * NC;
					uint8 const * const Pixel10Ptr = DataPtr + (Coords.Y        * Size.X + CoordsPlusOne.X) * NC;
					uint8 const * const Pixel01Ptr = DataPtr + (CoordsPlusOne.Y * Size.X + Coords.X)        * NC;
					uint8 const * const Pixel11Ptr = DataPtr + (CoordsPlusOne.Y * Size.X + CoordsPlusOne.X) * NC;

					auto LoadPixel = [](const uint8* Ptr) -> FPixelData
					{
						FPixelData Result;
						if constexpr (NC == 4)
						{
							const uint32 PackedData = *reinterpret_cast<const uint32*>(Ptr);
							
							Result.Data[0] = static_cast<uint16>((PackedData >> (8 * 0)) & 0xFF);
							Result.Data[1] = static_cast<uint16>((PackedData >> (8 * 1)) & 0xFF);
							Result.Data[2] = static_cast<uint16>((PackedData >> (8 * 2)) & 0xFF);
							Result.Data[3] = static_cast<uint16>((PackedData >> (8 * 3)) & 0xFF);
						}
						else
						{
							for (int32 C = 0; C < NC; ++C)
							{
								Result.Data[C] = static_cast<uint16>(Ptr[C]);
							}
						}

						return Result;
					};

					FPixelData PixelData00 = LoadPixel(Pixel00Ptr);
					FPixelData PixelData10 = LoadPixel(Pixel10Ptr);
					FPixelData PixelData01 = LoadPixel(Pixel01Ptr);
					FPixelData PixelData11 = LoadPixel(Pixel11Ptr);

					FPixelData FilteredPixelData;
				
					for (int32 C = 0; C < NC; ++C)
					{
						const uint16 LerpY0 = ((PixelData10.Data[C] * Frac.X) + PixelData00.Data[C] * (255 - Frac.X)) / 255;
						const uint16 LerpY1 = ((PixelData11.Data[C] * Frac.X) + PixelData01.Data[C] * (255 - Frac.X)) / 255;
						FilteredPixelData.Data[C] = ((LerpY1 * Frac.Y) + LerpY0*(255 - Frac.Y)) / 255;
					} 

					return FilteredPixelData;
				};

				FPixelData PixelData = SampleImageBilinear(Uv, BaseSize, pBase->GetData());
				static_assert(NC <= 4);

				uint8 * const DestData = pDest->GetData();
				for (uint32 Channel = 0; Channel < NC; ++Channel)
				{
					DestData[(Y*DestSize.X + X)*NC + Channel] = PixelData.Data[Channel];
				}
			}
		}
    }

	// Force instantiation of used templates. A link error will occur if any of them is missing.
	template void ImageTransformImpl<1, EAddressMode::ClampToBlack>(Image*, const Image*, FVector2f, FVector2f, float);
	template void ImageTransformImpl<1, EAddressMode::ClampToEdge>(Image*, const Image*, FVector2f, FVector2f, float);
	template void ImageTransformImpl<1, EAddressMode::Wrap>(Image*, const Image*, FVector2f, FVector2f, float);
	template void ImageTransformImpl<3, EAddressMode::ClampToBlack>(Image*, const Image*, FVector2f, FVector2f, float);
	template void ImageTransformImpl<3, EAddressMode::ClampToEdge>(Image*, const Image*, FVector2f, FVector2f, float);
	template void ImageTransformImpl<3, EAddressMode::Wrap>(Image*, const Image*, FVector2f, FVector2f, float);
	template void ImageTransformImpl<4, EAddressMode::ClampToBlack>(Image*, const Image*, FVector2f, FVector2f, float);
	template void ImageTransformImpl<4, EAddressMode::ClampToEdge>(Image*, const Image*, FVector2f, FVector2f, float);
	template void ImageTransformImpl<4, EAddressMode::Wrap>(Image*, const Image*, FVector2f, FVector2f, float);
}
