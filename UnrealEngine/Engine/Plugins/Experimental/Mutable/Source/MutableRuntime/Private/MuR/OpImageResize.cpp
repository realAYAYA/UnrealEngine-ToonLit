// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/ImagePrivate.h"
#include "MuR/MutableTrace.h"
#include "Async/ParallelFor.h"

namespace mu
{
	/** */
	template< int32 NC >
	void ImageMagnifyX(Image* Dest, const Image* Base)
	{
		int32 BaseSizeX = Base->GetSizeX();
		int32 DestSizeX = Dest->GetSizeX();
		int32 SizeY = Base->GetSizeY();

		uint32 dx_16 = (uint32(BaseSizeX) << 16) / DestSizeX;

		// Linear filtering
		//for ( int32 y=0; y<SizeY; ++y )
		const auto& ProcessLine = [
			Dest, Base, dx_16, BaseSizeX, DestSizeX
		] (uint32 y)
		{
			uint32 px_16 = 0;
			const uint8* BaseBuf = Base->GetData() + y * BaseSizeX * NC;
			uint8* DestBuf = Dest->GetData() + y * DestSizeX * NC;

			for (int32 x = 0; x < DestSizeX; ++x)
			{
				uint32 px = px_16 >> 16;
				uint32 epx_16 = px_16 + dx_16;

				if ((px_16 & 0xffff0000) == ((epx_16 - 1) & 0xffff0000))
				{
					// One fraction
					for (int32 c = 0; c < NC; ++c)
					{
						DestBuf[c] = BaseBuf[px * NC + c];
					}
				}
				else
				{
					// Two fractions
					uint32 frac1 = (px_16 & 0xffff);
					uint32 frac0 = 0x10000 - frac1;

					for (int32 c = 0; c < NC; ++c)
					{
						DestBuf[c] = uint8((BaseBuf[px * NC + c] * frac0 + BaseBuf[(px + 1) * NC + c] * frac1) >> 16);
					}

					++px;
				}

				px_16 = epx_16;
				DestBuf += NC;
			}
		};

		// TODO: ensure 64K-aligned batches?
		ParallelFor(SizeY, ProcessLine);
	}


	/** */
	void ImageMagnifyX(Image* Dest, const Image* Base)
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageMagnifyX)

		check(Dest->GetSizeY() == Base->GetSizeY());
		check(Dest->GetSizeX() > Base->GetSizeX());

		switch (Base->GetFormat())
		{

		case EImageFormat::IF_L_UBYTE:
		{
			ImageMagnifyX<1>(Dest, Base);
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
			ImageMagnifyX<3>(Dest, Base);
			break;
		}

		case EImageFormat::IF_BGRA_UBYTE:
		case EImageFormat::IF_RGBA_UBYTE:
		{
			ImageMagnifyX<4>(Dest, Base);
			break;
		}

		default:
			// Case not implemented
			check(false);
		}
	}


	/** General image minimisation. */
	template< int32 NC >
	void ImageMinifyX(Image* Dest, const Image* Base)
	{
		int32 BaseSizeX = Base->GetSizeX();
		int32 DestSizeX = Dest->GetSizeX();
		int32 SizeY = Base->GetSizeY();

		uint32 dx_16 = (uint32(BaseSizeX) << 16) / DestSizeX;

		// Linear filtering
		//for ( int32 y=0; y<SizeY; ++y )
		const auto& ProcessLine = [
			Dest, Base, dx_16, BaseSizeX, DestSizeX
		] (uint32 y)
		{
			const uint8* BaseBuf = Base->GetData() + y * BaseSizeX * NC;
			uint8* DestBuf = Dest->GetData() + y * DestSizeX * NC;

			uint32 px_16 = 0;
			for (int32 x = 0; x < DestSizeX; ++x)
			{
				uint32 r_16[NC];
				for (int32 c = 0; c < NC; ++c)
				{
					r_16[c] = 0;
				}

				uint32 epx_16 = px_16 + dx_16;
				uint32 px = px_16 >> 16;
				uint32 epx = epx_16 >> 16;

				// First fraction
				uint32 frac0 = px_16 & 0xffff;
				if (frac0)
				{
					for (int32 c = 0; c < NC; ++c)
					{
						r_16[c] += (0x10000 - frac0) * BaseBuf[px * NC + c];
					}

					++px;
				}

				// Whole pixels
				while (px < epx)
				{
					for (int32 c = 0; c < NC; ++c)
					{
						r_16[c] += uint32(BaseBuf[px * NC + c]) << 16;
					}

					++px;
				}

				// Second fraction
				uint32 frac1 = epx_16 & 0xffff;
				if (frac1)
				{
					for (int32 c = 0; c < NC; ++c)
					{
						r_16[c] += frac1 * BaseBuf[px * NC + c];
					}
				}

				for (int32 c = 0; c < NC; ++c)
				{
					DestBuf[c] = (uint8)(r_16[c] / dx_16);
				}

				px_16 = epx_16;
				DestBuf += NC;
			}
		};

		ParallelFor(SizeY, ProcessLine);
	}


	//---------------------------------------------------------------------------------------------
	//! Optimised for whole factors
	//---------------------------------------------------------------------------------------------
	template< int32 NC, int32 FACTOR >
	void ImageMinifyX_Exact(Image* Dest, const Image* Base)
	{
		int32 BaseSizeX = Base->GetSizeX();
		int32 DestSizeX = Dest->GetSizeX();
		int32 SizeY = Base->GetSizeY();

		const uint8* BaseBuf = Base->GetData();
		uint8* DestBuf = Dest->GetData();

		// Linear filtering
		const auto& ProcessLine = [
			Dest, Base, BaseSizeX, DestSizeX
		] (uint32 y)
		{
			const uint8* BaseBuf = Base->GetData() + y * BaseSizeX * NC;
			uint8* DestBuf = Dest->GetData() + y * DestSizeX * NC;

			uint32 r[NC];
			for (int32 x = 0; x < DestSizeX; ++x)
			{
				for (int32 c = 0; c < NC; ++c)
				{
					r[c] = 0;
					for (int32 a = 0; a < FACTOR; ++a)
					{
						r[c] += BaseBuf[a * NC + c];
					}
				}

				for (int32 c = 0; c < NC; ++c)
				{
					DestBuf[c] = (uint8)(r[c] / FACTOR);
				}

				DestBuf += NC;
				BaseBuf += NC * FACTOR;
			}
		};

		ParallelFor(SizeY, ProcessLine);
	}

	inline uint32 AverageChannel(uint32 a, uint32 b)
	{
		uint32 r = (a + b) >> 1;
		return r;
	}

	template<>
	void ImageMinifyX_Exact<4, 2>(Image* Dest, const Image* Base)
	{
		int32 BaseSizeX = Base->GetSizeX();
		int32 DestSizeX = Dest->GetSizeX();
		int32 SizeY = Base->GetSizeY();

		const uint8* BaseBuf = Base->GetData();
		uint8* DestBuf = Dest->GetData();

		int32 TotalBasePixels = BaseSizeX * SizeY;
		constexpr int32 BasePixelsPerBatch = 4096 * 2;
		int32 NumBatches = FMath::DivideAndRoundUp(TotalBasePixels, BasePixelsPerBatch);

		// Linear filtering
		const auto& ProcessBatchUnaligned =
			[DestBuf, BaseBuf, BaseSizeX, DestSizeX, BasePixelsPerBatch, TotalBasePixels]
		(int32 BatchIndex)
		{
			const uint8* pBatchBaseBuf = BaseBuf + BatchIndex * BasePixelsPerBatch * 4;
			uint8* pBatchDestBuf = DestBuf + BatchIndex * BasePixelsPerBatch * 4 / 2;

			int32 NumBasePixels = FMath::Min(BasePixelsPerBatch, TotalBasePixels - BatchIndex * BasePixelsPerBatch);

			uint16 r[4];
			for (int32 x = 0; x < NumBasePixels; x += 2)
			{
				r[0] = pBatchBaseBuf[0] + pBatchBaseBuf[0 + 4];
				r[1] = pBatchBaseBuf[1] + pBatchBaseBuf[1 + 4];
				r[2] = pBatchBaseBuf[2] + pBatchBaseBuf[2 + 4];
				r[3] = pBatchBaseBuf[3] + pBatchBaseBuf[3 + 4];

				pBatchDestBuf[0] = (uint8)(r[0] >> 1);
				pBatchDestBuf[1] = (uint8)(r[1] >> 1);
				pBatchDestBuf[2] = (uint8)(r[2] >> 1);
				pBatchDestBuf[3] = (uint8)(r[3] >> 1);

				pBatchBaseBuf += 4 * 2;
				pBatchDestBuf += 4;
			}
		};

		const auto& ProcessBatchAligned =
			[DestBuf, BaseBuf, BaseSizeX, DestSizeX, BasePixelsPerBatch, TotalBasePixels]
		(int32 BatchIndex)
		{
			const uint32* pBatchBaseBuf = reinterpret_cast<const uint32*>(BaseBuf) + BatchIndex * BasePixelsPerBatch;
			uint32* pBatchDestBuf = reinterpret_cast<uint32*>(DestBuf) + BatchIndex * (BasePixelsPerBatch >> 1);

			int32 NumBasePixels = FMath::Min(BasePixelsPerBatch, TotalBasePixels - BatchIndex * BasePixelsPerBatch);

			for (int32 p = 0; p < NumBasePixels; ++p)
			{
				uint32 FullSource0 = pBatchBaseBuf[p * 2 + 0];
				uint32 FullSource1 = pBatchBaseBuf[p * 2 + 1];

				uint32 FullResult = 0;

				FullResult |= AverageChannel((FullSource0 >> 0) & 0xff, (FullSource1 >> 0) & 0xff) << 0;
				FullResult |= AverageChannel((FullSource0 >> 8) & 0xff, (FullSource1 >> 8) & 0xff) << 8;
				FullResult |= AverageChannel((FullSource0 >> 16) & 0xff, (FullSource1 >> 16) & 0xff) << 16;
				FullResult |= AverageChannel((FullSource0 >> 24) & 0xff, (FullSource1 >> 24) & 0xff) << 24;

				pBatchDestBuf[p] = FullResult;
			}
		};


		ParallelFor(NumBatches, ProcessBatchUnaligned);
	}


	//---------------------------------------------------------------------------------------------
	//! Image minify X version hub.
	//---------------------------------------------------------------------------------------------
	void ImageMinifyX(Image* Dest, const Image* Base)
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageMinifyX)

		check(Dest->GetSizeY() == Base->GetSizeY());
		check(Dest->GetSizeX() < Base->GetSizeX());

		switch (Base->GetFormat())
		{

		case EImageFormat::IF_L_UBYTE:
		{
			if (2 * Dest->GetSizeX() == Base->GetSizeX())
			{
				// Optimised case
				ImageMinifyX_Exact<1, 2>(Dest, Base);
			}
			else
			{
				// Generic case
				ImageMinifyX<1>(Dest, Base);
			}
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
			if (2 * Dest->GetSizeX() == Base->GetSizeX())
			{
				// Optimised case
				ImageMinifyX_Exact<3, 2>(Dest, Base);
			}
			else
			{
				// Generic case
				ImageMinifyX<3>(Dest, Base);
			}
			break;
		}

		case EImageFormat::IF_BGRA_UBYTE:
		case EImageFormat::IF_RGBA_UBYTE:
		{
			if (2 * Dest->GetSizeX() == Base->GetSizeX())
			{
				// Optimised case
				ImageMinifyX_Exact<4, 2>(Dest, Base);
			}
			else if (4 * Dest->GetSizeX() == Base->GetSizeX())
			{
				// Optimised case
				ImageMinifyX_Exact<4, 4>(Dest, Base);
			}
			else
			{
				// Generic case
				ImageMinifyX<4>(Dest, Base);
			}
			break;
		}

		default:
			// Case not implemented
			check(false);
		}

	}


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	template< int32 NC >
	void ImageMagnifyY(Image* Dest, const Image* Base)
	{
		if (!Base || !Dest ||
			!Base->GetSizeX() || !Base->GetSizeY() || !Dest->GetSizeX() || !Dest->GetSizeY())
		{
			return;
		}

		int32 BaseSizeY = Base->GetSizeY();
		int32 DestSizeY = Dest->GetSizeY();
		int32 SizeX = Base->GetSizeX();

		SIZE_T rowSize = SizeX * NC;

		// Common case, optimised.
		if (DestSizeY == BaseSizeY * 2)
		{
			//for (int32 y = 0; y < BaseSizeY; ++y)
			const auto& ProcessLine = [
				Dest, Base, rowSize
			] (uint32 y)
			{
				uint8* DestBuf = Dest->GetData() + 2 * y * rowSize;
				const uint8* BaseBuf = Base->GetData() + y * rowSize;

				FMemory::Memcpy(DestBuf, BaseBuf, rowSize);
				DestBuf += rowSize;

				FMemory::Memcpy(DestBuf, BaseBuf, rowSize);
			};

				ParallelFor(BaseSizeY, ProcessLine);
		}
		else
		{
			uint32 dy_16 = (uint32(BaseSizeY) << 16) / DestSizeY;

			// Linear filtering
			// \todo: optimise: swap loops, etc.
			//for ( int32 x=0; x<SizeX; ++x )
			const auto& ProcessColumn = [
				Dest, Base, SizeX, DestSizeY, dy_16
			] (uint32 x)
			{
				uint32 py_16 = 0;
				uint8* DestBuf = Dest->GetData() + x * NC;
				const uint8* BaseBuf = Base->GetData();

				for (int32 y = 0; y < DestSizeY; ++y)
				{
					uint32 py = py_16 >> 16;
					uint32 epy_16 = py_16 + dy_16;

					if ((py_16 & 0xffff0000) == ((epy_16 - 1) & 0xffff0000))
					{
						// One fraction
						for (int32 c = 0; c < NC; ++c)
						{
							DestBuf[c] = BaseBuf[(py * SizeX + x) * NC + c];
						}
					}
					else
					{
						// Two fractions
						uint32 frac1 = (py_16 & 0xffff);
						uint32 frac0 = 0x10000 - frac1;

						for (int32 c = 0; c < NC; ++c)
						{
							DestBuf[c] = (uint8)((BaseBuf[(py * SizeX + x) * NC + c] * frac0 +
								BaseBuf[((py + 1) * SizeX + x) * NC + c] * frac1
								) >> 16);
						}

						++py;
					}

					py_16 = epy_16;
					DestBuf += SizeX * NC;
				}
			};

				ParallelFor(SizeX, ProcessColumn);
		}
	}


	void ImageMagnifyY(Image* Dest, const Image* Base)
	{
		check(Dest->GetSizeY() > Base->GetSizeY());
		check(Dest->GetSizeX() == Base->GetSizeX());

		MUTABLE_CPUPROFILER_SCOPE(ImageMagnifyY)

		switch (Base->GetFormat())
		{

		case EImageFormat::IF_L_UBYTE:
		{
			ImageMagnifyY<1>(Dest, Base);
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
			ImageMagnifyY<3>(Dest, Base);
			break;
		}

		case EImageFormat::IF_RGBA_UBYTE:
		case EImageFormat::IF_BGRA_UBYTE:
		{
			ImageMagnifyY<4>(Dest, Base);
			break;
		}

		default:
			// Case not implemented
			check(false);
		}
	}


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	template< int32 NC >
	void ImageMinifyY(Image* Dest, const Image* Base)
	{
		int32 BaseSizeY = Base->GetSizeY();
		int32 DestSizeY = Dest->GetSizeY();
		int32 SizeX = Base->GetSizeX();

		uint32 dy_16 = (uint32(BaseSizeY) << 16) / DestSizeY;

		const uint8* BaseBuf = Base->GetData();

		// Linear filtering
		//for (int32 x = 0; x < SizeX; ++x)
		const auto& ProcessColumn = [
			Dest, BaseBuf, SizeX, DestSizeY, dy_16
		] (uint32 x)
		{
			uint8* DestBuf = Dest->GetData() + x * NC;
			uint32 py_16 = 0;
			for (int32 y = 0; y < DestSizeY; ++y)
			{
				uint32 r_16[NC];
				for (int32 c = 0; c < NC; ++c)
				{
					r_16[c] = 0;
				}

				uint32 epy_16 = py_16 + dy_16;
				uint32 py = py_16 >> 16;
				uint32 epy = epy_16 >> 16;

				// First fraction
				uint32 frac0 = py_16 & 0xffff;
				if (frac0)
				{
					for (int32 c = 0; c < NC; ++c)
					{
						r_16[c] += (0x10000 - frac0) * BaseBuf[(py * SizeX + x) * NC + c];
					}

					++py;
				}

				// Whole pixels
				while (py < epy)
				{
					for (int32 c = 0; c < NC; ++c)
					{
						r_16[c] += uint32(BaseBuf[(py * SizeX + x) * NC + c]) << 16;
					}

					++py;
				}

				// Second fraction
				uint32 frac1 = epy_16 & 0xffff;
				if (frac1)
				{
					for (int32 c = 0; c < NC; ++c)
					{
						r_16[c] += frac1 * BaseBuf[(py * SizeX + x) * NC + c];
					}
				}

				for (int32 c = 0; c < NC; ++c)
				{
					DestBuf[c] = (uint8)(r_16[c] / dy_16);
				}

				py_16 = epy_16;
				DestBuf += SizeX * NC;
			}
		};

		ParallelFor(SizeX, ProcessColumn);

	}


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	template< int32 NC, int32 FACTOR >
	void ImageMinifyY_Exact(Image* Dest, const Image* Base)
	{
		int32 DestSizeY = Dest->GetSizeY();
		int32 SizeX = Base->GetSizeX();

		// Linear filtering
		//for ( int32 y=0; y<DestSizeY; ++y )
		const auto ProcessRow = [
			Dest, Base, SizeX, DestSizeY
		] (uint32 y)
		{
			uint8* DestBuf = Dest->GetData() + y * NC * SizeX;
			const uint8* BaseBuf = Base->GetData() + y * FACTOR * SizeX * NC;

			for (int32 x = 0; x < SizeX; ++x)
			{
				uint32 r[NC];
				for (int32 c = 0; c < NC; ++c)
				{
					r[c] = 0;
				}

				// Whole pixels
				for (int32 f = 0; f < FACTOR; ++f)
				{
					for (int32 c = 0; c < NC; ++c)
					{
						r[c] += BaseBuf[SizeX * NC * f + x * NC + c];
					}
				}

				for (int32 c = 0; c < NC; ++c)
				{
					DestBuf[c] = (uint8)(r[c] / FACTOR);
				}

				DestBuf += NC;
			}
			BaseBuf += FACTOR * SizeX * NC;
		};

		ParallelFor(DestSizeY, ProcessRow);
	}


	//---------------------------------------------------------------------------------------------
	//! Image minify Y version hub.
	//---------------------------------------------------------------------------------------------
	void ImageMinifyY(Image* Dest, const Image* Base)
	{
		check(Dest->GetSizeY() < Base->GetSizeY());
		check(Dest->GetSizeX() == Base->GetSizeX());

		MUTABLE_CPUPROFILER_SCOPE(ImageMinifyY)

		switch (Base->GetFormat())
		{

		case EImageFormat::IF_L_UBYTE:
		{
			if (2 * Dest->GetSizeY() == Base->GetSizeY())
			{
				// Optimised case
				ImageMinifyY_Exact<1, 2>(Dest, Base);
			}
			else
			{
				// Generic case
				ImageMinifyY<1>(Dest, Base);
			}
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
			if (2 * Dest->GetSizeY() == Base->GetSizeY())
			{
				// Optimised case
				ImageMinifyY_Exact<3, 2>(Dest, Base);
			}
			else
			{
				// Generic case
				ImageMinifyY<3>(Dest, Base);
			}
			break;
		}

		case EImageFormat::IF_RGBA_UBYTE:
		case EImageFormat::IF_BGRA_UBYTE:
		{
			if (2 * Dest->GetSizeY() == Base->GetSizeY())
			{
				// Optimised case
				ImageMinifyY_Exact<4, 2>(Dest, Base);
			}
			else
			{
				// Generic case
				ImageMinifyY<4>(Dest, Base);
			}
			break;
		}

		default:
			// Case not implemented
			check(false);
			//mu::Halt();
		}

	}


	void FImageOperator::ImageResizeLinear(Image* Dest, int32 ImageCompressionQuality, const Image* InBase)
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageResizeLinear);

		check(!(InBase->m_flags & Image::IF_CANNOT_BE_SCALED));

		ImagePtrConst Base = InBase;
		ImagePtr BaseReformat;

		// Shouldn't happen! But if it does...
		EImageFormat SourceFormat = Base->GetFormat();
		EImageFormat UncompressedFormat = GetUncompressedFormat(SourceFormat);
		if (SourceFormat != UncompressedFormat)
		{
			BaseReformat = ImagePixelFormat(ImageCompressionQuality, InBase, UncompressedFormat);
			Base = BaseReformat;
		}

		FImageSize BaseSize = FImageSize(Base->GetSizeX(), Base->GetSizeY());
		FImageSize DestSize = FImageSize(Dest->GetSizeX(), Dest->GetSizeY());
		if (!DestSize[0] || !DestSize[1] || !BaseSize[0] || !BaseSize[1])
		{
			return;
		}

		// First resize X
		ImagePtr Temp;
		if (DestSize[0] > BaseSize[0])
		{
			Temp = CreateImage(DestSize[0], BaseSize[1], 1, Base->GetFormat(), EInitializationType::NotInitialized);
			ImageMagnifyX(Temp.get(), Base.get());
		}
		else if (DestSize[0] < BaseSize[0])
		{
			Temp = CreateImage(DestSize[0], BaseSize[1], 1, Base->GetFormat(), EInitializationType::NotInitialized);
			ImageMinifyX(Temp.get(), Base.get());
		}
		else
		{
			Temp = CloneImage(Base.get());
		}

		// Now resize Y
		ImagePtr Temp2;
		if (DestSize[1] > BaseSize[1])
		{
			Temp2 = CreateImage(DestSize[0], DestSize[1], 1, Base->GetFormat(), EInitializationType::NotInitialized);
			ImageMagnifyY(Temp2.get(), Temp.get());
			ReleaseImage(Temp);
		}
		else if (DestSize[1] < BaseSize[1])
		{
			Temp2 = CreateImage(DestSize[0], DestSize[1], 1, Base->GetFormat(), EInitializationType::NotInitialized);
			ImageMinifyY(Temp2.get(), Temp.get());
			ReleaseImage(Temp);
		}
		else
		{
			Temp2 = Temp;
		}
		Temp = nullptr;


		// Reset format if it was changed to scale
		if (SourceFormat != UncompressedFormat)
		{
			Ptr<Image> OldTemp = Temp2;
			Temp2 = ImagePixelFormat(ImageCompressionQuality, Temp2.get(), SourceFormat);
			ReleaseImage(OldTemp);
		}

		Dest->CopyMove(Temp2.get());
		ReleaseImage(Temp2);
		Temp2 = nullptr;

		// Update the relevancy data of the image.
		if (Base->m_flags & Image::EImageFlags::IF_HAS_RELEVANCY_MAP)
		{
			Dest->m_flags |= Image::EImageFlags::IF_HAS_RELEVANCY_MAP;

			float FactorY = float(DestSize[1]) / float(BaseSize[1]);

			Dest->RelevancyMinY = uint16(FMath::FloorToFloat(Base->RelevancyMinY * FactorY));
			Dest->RelevancyMaxY = uint16(FMath::Min((int32)FMath::CeilToFloat(Base->RelevancyMinY * FactorY), Dest->GetSizeY() - 1));
		}

		ReleaseImage(BaseReformat);
	}

}