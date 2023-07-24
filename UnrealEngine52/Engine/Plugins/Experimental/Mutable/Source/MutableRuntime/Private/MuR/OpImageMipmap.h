// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/OpImagePixelFormat.h"
#include "Async/ParallelFor.h"

namespace mu
{

	namespace OpImageMipmap_Detail
	{

		struct SharpenKernelStorage8
		{
			constexpr static size_t SIZE = 8;
			float m_storage[SIZE * SIZE];
		};

		template<size_t ITERS, size_t N>
		inline void BlurKernel(float(&inOutKernelStorage)[N])
		{
			static_assert(N > 2);

			float temp[N];

			float* inBuf = &inOutKernelStorage[0];
			float* outBuf = &temp[0];

			for (int32 i = 0; i < ITERS; ++i)
			{
				constexpr float oneOverThree = 1.0f / 3.0f;

				outBuf[0] = (inBuf[0] + inBuf[1]) * oneOverThree;

				for (int32 j = 1; j < N - 1; ++j)
				{
					outBuf[j] = (inBuf[j - 1] + inBuf[j] + inBuf[j + 1]) * oneOverThree;
				}

				outBuf[N - 1] = (inBuf[N - 2] + inBuf[N - 1]) * oneOverThree;

				// Ping-pong buffers.
				std::swap(outBuf, inBuf);
			}

			if (ITERS % 2 == 1)
			{
				for (uint32 i = 0; i < N; ++i)
				{
					inOutKernelStorage[i] = temp[i];
				}
			}
		}

		template<size_t N>
		inline void BuildGaussianBlurKernel(float(&kernel)[N], float variance)
		{
			constexpr float sqrtTau = 2.5066282746310002f;
			const auto normalDist = [sdSqrtTauInv = 1.0f / (FMath::Sqrt(variance) * sqrtTau),
				twoVInv = 1.0f / (2.0f * variance)]
				(float x)
				-> float
			{
				return FMath::Exp(-x * x * twoVInv) * sdSqrtTauInv;
			};

			const float center = static_cast<float>(N) * 0.5f;
			float sum = 0;
			for (int32 i = 0; i < N; ++i)
			{
				const float dist = normalDist(static_cast<float>(i) - center + 0.5f);
				kernel[i] = dist;
				sum += dist;
			}

			const float normFactor = 1.0f / sum;
			for (int32 i = 0; i < N; ++i)
			{
				kernel[i] *= normFactor;
			}
		}

		template<size_t N>
		inline void BuildSharpenKernel(float(&kernel)[N], float factor)
		{
			for (uint32 i = 0; i < N; ++i)
			{
				kernel[i] = 0.0f;
			}

			kernel[(N >> 1) - 1] = factor * 0.5f;
			kernel[(N >> 1)] = factor * 0.5f;
		}

		template<size_t N>
		inline void BuildKernel2DFromSeparable(float(&kernel1D)[N], SharpenKernelStorage8& outKernel)
		{
			static_assert(N == SharpenKernelStorage8::SIZE);

			for (int32 y = 0; y < N; ++y)
			{
				for (int32 x = 0; x < N; ++x)
				{
					outKernel.m_storage[y * N + x] = kernel1D[y] * kernel1D[x];
				}
			}
		}

		inline SharpenKernelStorage8 MakeMipGaussianSharpenKernel(float factor)
		{
			SharpenKernelStorage8 kernel;

			float kernel1DStorage0[SharpenKernelStorage8::SIZE];
			float kernel1DStorage1[SharpenKernelStorage8::SIZE];

			// negative factors indicate blur where the -factor is the variance.
			if (factor < 0)
			{
				BuildGaussianBlurKernel(kernel1DStorage0, -factor);

				BuildKernel2DFromSeparable(kernel1DStorage0, kernel);

				return kernel;
			}

			BuildSharpenKernel(kernel1DStorage0, 1.0f + factor);
			BuildSharpenKernel(kernel1DStorage1, -factor);

			BlurKernel<1>(kernel1DStorage0);
			BlurKernel<3>(kernel1DStorage1);

			for (int32 i = 0; i < SharpenKernelStorage8::SIZE; ++i)
			{
				kernel1DStorage0[i] += kernel1DStorage1[i];
			}

			BuildKernel2DFromSeparable(kernel1DStorage0, kernel);

			return kernel;
		}

		template<int32 PIXEL_SIZE, typename CHANNEL_TYPE, EAddressMode AD_MODE = EAddressMode::AM_NONE>
		class ImageAccessor
		{
			const FIntVector2 m_dim;
			CHANNEL_TYPE const* const m_pImageBuf;

		public:
			ImageAccessor(const CHANNEL_TYPE* const pImageBuf, FIntVector2 dim)
				: m_dim(dim)
				, m_pImageBuf(pImageBuf)
			{
			}

			uint8 operator()(int32 x, int32 y, int32 c) const
			{
				if constexpr (AD_MODE == EAddressMode::AM_WRAP)
				{
					// (y & (m_dim.Y - 1)) * m_dim.Y + (x & (m_dim.X - 1))
					return m_pImageBuf[((y % m_dim.Y) * m_dim.X + (x % m_dim.X)) * PIXEL_SIZE + c];
				}
				else if (AD_MODE == EAddressMode::AM_CLAMP)
				{
					return m_pImageBuf[(mu::clamp(y, 0, m_dim.Y - 1) * m_dim.X +
						mu::clamp(x, 0, m_dim.X - 1))
						* PIXEL_SIZE + c];
				}
				else if (AD_MODE == EAddressMode::AM_BLACK_BORDER)
				{
					return ((x < m_dim.X) && (y < m_dim.Y) && (x >= 0) && (y >= 0))
						? m_pImageBuf[(y * m_dim.X + x) * PIXEL_SIZE + c]
						: 0;
				}
				else
				{
					return m_pImageBuf[(y * m_dim.X + x) * PIXEL_SIZE + c];
				}
			}
		};


		template<int32 PIXEL_SIZE, EAddressMode AD, typename PIXEL_TYPE>
		void GenerateMipSharpenedRegion(
			const SharpenKernelStorage8& kernel,
			const ImageAccessor<PIXEL_SIZE, PIXEL_TYPE, AD>& sourceAccessor,
			PIXEL_TYPE* pDest, FIntVector2 destSize,
			FIntVector2 regionStart, FIntVector2 regionEnd)
		{
			// Only kernels of size 8 supported.
			constexpr int32 kernelSize = SharpenKernelStorage8::SIZE;
			static_assert(kernelSize == 8);

			constexpr int32 kernelCenter = (kernelSize >> 1) - 1;
			for (int32 y = regionStart.Y; y < regionEnd.Y; ++y)
			{
				for (int32 x = regionStart.X; x < regionEnd.X; ++x)
				{
					float stagingStorage[kernelSize * kernelSize * PIXEL_SIZE];

					const FIntVector2 sourceCoord = FIntVector2(x << 1, y << 1);


					// Convert Image kernel stamp to float. This conversion will be done multiple times.
					// and should be done once before applying the kernel, or operate directly in unorm8/snorm8.
					for (int32 ky = 0; ky < kernelSize; ++ky)
					{
						for (int32 kx = 0; kx < kernelSize; ++kx)
						{
							for (int32 c = 0; c < PIXEL_SIZE; ++c)
							{
								stagingStorage[(ky * kernelSize + kx) + (kernelSize * kernelSize * c)] = static_cast<float>(
									sourceAccessor(sourceCoord.X + kx - kernelCenter,
										sourceCoord.Y + ky - kernelCenter,
										c)) / 255.0f;
							}
						}
					}

					// Apply kernel. Although the kernel is separable, the cost would not differ much from the not separable one.
					// That is because to make it separable a partial convolution of the source is needed. 
					// Since here we are only applying to half the image and the kernel used is small (8x8) the benefits are not 
					// clear.

					for (int32 c = 0; c < PIXEL_SIZE; ++c)
					{
						float sampleValue = 0.0f;
						float sum = 0.0f;
						for (int32 k = 0; k < kernelSize * kernelSize; ++k)
						{
							sampleValue += kernel.m_storage[k] * stagingStorage[k + (kernelSize * kernelSize * c)];
							sum += kernel.m_storage[k];
						}

						pDest[(y * destSize.X + x) * PIXEL_SIZE + c] = static_cast<uint8>(mu::clamp(sampleValue, 0.0f, 1.0f) * 255.0f);
					}
				}
			}
		}

		template<size_t PIXEL_SIZE, EAddressMode AD_MODE>
		inline void GenerateMipmapUint8Sharpen(int mips,
			const uint8* pSource, uint8* pDest,
			FIntVector2 sourceSize,
			float sharpeningFactor)
		{
			SharpenKernelStorage8 kernel = MakeMipGaussianSharpenKernel(sharpeningFactor);

			size_t totalDestSize = 0;

			for (; mips >= 0; --mips)
			{
				ImageAccessor<PIXEL_SIZE, uint8, AD_MODE> sourceSampler(pSource, sourceSize);
				ImageAccessor<PIXEL_SIZE, uint8, EAddressMode::AM_NONE> sourceSamplerAddressModeNone(pSource, sourceSize);

				FIntVector2 destSize = FIntVector2(FMath::DivideAndRoundUp(sourceSize.X, 2), FMath::DivideAndRoundUp(sourceSize.Y, 2));

				// Core image, will never sample outside the image so we don't need to care about address mode.
				// Only sample source pixels where the kernel can be fully applied.
				// Border cases are treated separately.
				GenerateMipSharpenedRegion(
					kernel,
					sourceSamplerAddressModeNone,
					pDest, destSize,
					FIntVector2(2, 2), FIntVector2(destSize.X - 2, destSize.Y - 2));

				// Horizontal borders.
				GenerateMipSharpenedRegion(
					kernel,
					sourceSampler,
					pDest, destSize,
					FIntVector2(0, 0), FIntVector2(destSize.X, FMath::Min(2, destSize.Y)));

				GenerateMipSharpenedRegion(
					kernel,
					sourceSampler,
					pDest, destSize,
					FIntVector2(0, FMath::Max(destSize.Y - 2, 0)), FIntVector2(destSize.X, destSize.Y));

				// Vertical borders
				GenerateMipSharpenedRegion(
					kernel,
					sourceSampler,
					pDest, destSize,
					FIntVector2(0, FMath::Min(2, destSize.Y)), FIntVector2(FMath::Min(2, destSize.X), destSize.Y - 2));

				GenerateMipSharpenedRegion(
					kernel,
					sourceSampler,
					pDest, destSize,
					FIntVector2(destSize.X - 2, FMath::Min(2, destSize.Y)), FIntVector2(destSize.X, destSize.Y - 2));

				sourceSize = destSize;

				pSource = pDest;
				pDest = pDest + destSize.X * destSize.Y * PIXEL_SIZE;

				totalDestSize += sourceSize.X * sourceSize.Y * PIXEL_SIZE;
			}
		}

		template<int32 PIXEL_SIZE>
		inline void GenerateMipmapUint8Unfiltered(
			int mips,
			const uint8* pSource, uint8* pDest,
			FIntVector2 sourceSize)
		{
			for (; mips >= 0; --mips)
			{
				FIntVector2 destSize = FIntVector2(FMath::DivideAndRoundUp(sourceSize.X, 2), FMath::DivideAndRoundUp(sourceSize.Y, 2));

				for (int32 y = 0; y < destSize.Y; ++y)
				{
					for (int32 x = 0; x < destSize.X; ++x)
					{
						for (int32 c = 0; c < PIXEL_SIZE; ++c)
						{
							pDest[(y * destSize.X + x) * PIXEL_SIZE + c] =
								pSource[((y << 1) * sourceSize.X + (x << 1)) * PIXEL_SIZE + c];
						}
					}
				}

				sourceSize = destSize;
				pSource = pDest;
				pDest = pDest + destSize.X * destSize.Y * PIXEL_SIZE;
			}
		}

		template<int32 PIXEL_SIZE>
		inline void GenerateMipmapsUint8SimpleAverage(
			int mips,
			const uint8* pSource, uint8* pDest,
			FIntVector2 sourceSize)
		{
			const uint8* pMipSource = pSource;
			uint8* pMipDest = pDest;

			FIntVector2 destSize = sourceSize;
			for (int m = 0; m < mips; ++m)
			{
				check(destSize[0] > 1 || destSize[1] > 1);

				sourceSize = destSize;

				int fullColumns = destSize[0] / 2;
				bool strayColumn = (destSize[0] % 2) != 0;
				int fullRows = destSize[1] / 2;
				bool strayRow = (destSize[1] % 2) != 0;

				destSize[0] = FMath::DivideAndRoundUp(destSize[0], 2);
				destSize[1] = FMath::DivideAndRoundUp(destSize[1], 2);

				int sourceStride = sourceSize[0] * PIXEL_SIZE;
				int destStride = destSize[0] * PIXEL_SIZE;

				const auto ProcessRow = [
					pMipDest, pMipSource, fullColumns, strayColumn, sourceStride, destStride
				] (uint32 y)
				{
					const uint8* pSourceRow0 = pMipSource + 2 * y * sourceStride;
					const uint8* pSourceRow1 = pSourceRow0 + sourceStride;
					uint8* pDestRow = pMipDest + y * destStride;

					for (int x = 0; x < fullColumns; ++x)
					{
						for (int c = 0; c < PIXEL_SIZE; ++c)
						{
							int p = pSourceRow0[c] + pSourceRow0[PIXEL_SIZE + c] + pSourceRow1[c] +
								pSourceRow1[PIXEL_SIZE + c];
							pDestRow[c] = (uint8)(p >> 2);
						}

						pSourceRow0 += 2 * PIXEL_SIZE;
						pSourceRow1 += 2 * PIXEL_SIZE;
						pDestRow += PIXEL_SIZE;
					}

					if (strayColumn)
					{
						for (int c = 0; c < PIXEL_SIZE; ++c)
						{
							int p = pSourceRow0[c] + pSourceRow1[c];
							pDestRow[c] = (uint8)(p >> 1);
						}
					}
				};

					constexpr int PixelConcurrencyThreshold = 0xffff;
					if (destSize[0] * destSize[1] < PixelConcurrencyThreshold)
					{
						for (int y = 0; y < fullRows; ++y)
						{
							ProcessRow(y);
						}
					}
					else
					{
						ParallelFor(fullRows, ProcessRow);
					}

					if (strayRow)
					{
						const uint8* pSourceRow0 = pMipSource + 2 * fullRows * sourceStride;
						const uint8* pSourceRow1 = pSourceRow0 + sourceStride;
						uint8* pDestRow = pMipDest + fullRows * destStride;

						for (int x = 0; x < fullColumns; ++x)
						{
							for (int c = 0; c < PIXEL_SIZE; ++c)
							{
								int p = pSourceRow0[c] + pSourceRow0[PIXEL_SIZE + c];
								pDestRow[c] = (uint8)(p >> 1);
							}

							pSourceRow0 += 2 * PIXEL_SIZE;
							pDestRow += PIXEL_SIZE;
						}

						if (strayColumn)
						{
							for (int c = 0; c < PIXEL_SIZE; ++c)
							{
								pDestRow[c] = pSourceRow0[c];
							}
						}
					}

					// Reset the source pointer for the next mip, to use the dest that we have just
					// generated.
					pMipSource = pMipDest;
					pMipDest += destSize[0] * destSize[1] * PIXEL_SIZE;
			}
		}

	} // namespace OpImageMipmap_Detail


		//---------------------------------------------------------------------------------------------
		//! Generate the mipmaps for byte-based images of whatever number of channels.
		//! \param mips number of additional levels to build from the source.
		//---------------------------------------------------------------------------------------------
	template<int32 PIXEL_SIZE>
	inline void GenerateMipmapsUint8(int mips,
		const uint8* pSource, uint8* pDest,
		FIntVector2 sourceSize,
		const FMipmapGenerationSettings& settings)
	{
		using namespace OpImageMipmap_Detail;

		switch (settings.m_filterType)
		{
		case EMipmapFilterType::MFT_SimpleAverage:
		{
			GenerateMipmapsUint8SimpleAverage<PIXEL_SIZE>(mips, pSource, pDest, sourceSize);
			break;
		}
		case EMipmapFilterType::MFT_Sharpen:
		{
			switch (settings.m_addressMode)
			{
			case EAddressMode::AM_CLAMP:
			{
				GenerateMipmapUint8Sharpen<PIXEL_SIZE, EAddressMode::AM_CLAMP>(
					mips, pSource, pDest, sourceSize, settings.m_sharpenFactor);
				break;
			}
			case EAddressMode::AM_WRAP:
			{
				GenerateMipmapUint8Sharpen<PIXEL_SIZE, EAddressMode::AM_WRAP>(
					mips, pSource, pDest, sourceSize, settings.m_sharpenFactor);
				break;
			}
			case EAddressMode::AM_BLACK_BORDER:
			{
				GenerateMipmapUint8Sharpen<PIXEL_SIZE, EAddressMode::AM_BLACK_BORDER>(
					mips, pSource, pDest, sourceSize, settings.m_sharpenFactor);
				break;
			}
			default:
			{
				GenerateMipmapUint8Sharpen<PIXEL_SIZE, EAddressMode::AM_NONE>(
					mips, pSource, pDest, sourceSize, settings.m_sharpenFactor);
				break;
			}
			}

			break;
		}
		case EMipmapFilterType::MFT_Unfiltered:
		{
			GenerateMipmapUint8Unfiltered<PIXEL_SIZE>(mips, pSource, pDest, sourceSize);
			break;
		}
		default:
		{
			check(false);
			break;
		}
		}
	}

	struct SCRATCH_IMAGE_MIPMAP
	{
		ImagePtr pUncompressed;
		ImagePtr pUncompressedMips;
		ImagePtr pCompressedMips;
	};


    //---------------------------------------------------------------------------------------------
    inline void ImageMipmap_PrepareScratch( Image* pDest, const Image* pBase, int levelCount,
                                            SCRATCH_IMAGE_MIPMAP* scratch )
    {
        int startLevel = pBase->GetLODCount() - 1;

        check(pDest->GetLODCount() == levelCount);
        check(pDest->GetSizeX() == pBase->GetSizeX());
        check(pDest->GetSizeY() == pBase->GetSizeY());
        check(pDest->GetFormat() == pBase->GetFormat());
		(void)pDest;

        switch (pBase->GetFormat())
        {
        case EImageFormat::IF_L_UBYTE:
        case EImageFormat::IF_RGB_UBYTE:
        case EImageFormat::IF_RGBA_UBYTE:
        case EImageFormat::IF_BGRA_UBYTE:
            break;

        // Bad cases: we need to decompress, mip and then recompress. It may be necessary to
        // generate the latests mips after composing, or in unoptimised code.
        // TODO: Make sure that the code optimisations avoid this cases generating separate
        // operations to generate the mip tail.
        case EImageFormat::IF_BC1:
        case EImageFormat::IF_BC2:
        case EImageFormat::IF_BC3:
        case EImageFormat::IF_BC4:
        case EImageFormat::IF_BC5:
        case EImageFormat::IF_ASTC_4x4_RGB_LDR:
        case EImageFormat::IF_ASTC_4x4_RGBA_LDR:
        case EImageFormat::IF_ASTC_4x4_RG_LDR:
        {
		// Uncompress the last mip that we already have
			FIntVector2 uncompressedSize = pBase->CalculateMipSize(startLevel);
			scratch->pUncompressed = new Image(
				(uint16)uncompressedSize[0],
				(uint16)uncompressedSize[1],
				1,
				EImageFormat::IF_RGBA_UBYTE);

			FIntVector2 uncompressedMipsSize = pBase->CalculateMipSize(startLevel + 1);
			// Generate the mipmaps from there on
			scratch->pUncompressedMips = new Image(
				(uint16)uncompressedMipsSize[0],
				(uint16)uncompressedMipsSize[1],
				FMath::Max(1, levelCount - startLevel - 1),
				EImageFormat::IF_RGBA_UBYTE);

			// Compress the mipmapped image
			scratch->pCompressedMips = new Image(
				(uint16)uncompressedMipsSize[0],
				(uint16)uncompressedMipsSize[1],
				scratch->pUncompressedMips->GetLODCount(),
				pBase->GetFormat());

            break;
        }

        case EImageFormat::IF_L_UBIT_RLE:
        case EImageFormat::IF_L_UBYTE_RLE:
        {
            // Uncompress the last mip that we already have
			FIntVector2 uncompressedSize = pBase->CalculateMipSize(startLevel);
            scratch->pUncompressed = new Image(
                (uint16)uncompressedSize[0],
                (uint16)uncompressedSize[1],
                1,
                EImageFormat::IF_L_UBYTE);


			FIntVector2 uncompressedMipsSize = pBase->CalculateMipSize(startLevel + 1);
            // Generate the mipmaps from there on
            scratch->pUncompressedMips = new Image(
                (uint16)uncompressedMipsSize[0],
                (uint16)uncompressedMipsSize[1],
				FMath::Max(1, levelCount - startLevel - 1),
                EImageFormat::IF_L_UBYTE);


            // Compress the mipmapped image
            scratch->pCompressedMips = new Image(
                (uint16)uncompressedMipsSize[0],
                (uint16)uncompressedMipsSize[1],
                scratch->pUncompressedMips->GetLODCount(),
                pBase->GetFormat());

            // Preallocate ample memory for the compressed data
            scratch->pCompressedMips->m_data.SetNum(scratch->pUncompressedMips->GetDataSize());

            // Preallocate ample memory for the destination data
            pDest->m_data.SetNum(pBase->GetDataSize() + scratch->pUncompressedMips->GetDataSize());

            break;
        }

        default:
            checkf( false, TEXT("Format not implemented in mipmap generation."));
        }
    }

    //---------------------------------------------------------------------------------------------
    //! Generate the mipmaps for images.
	//! if bGenerateOnlyTail is true, generates the mips missing from pBase to levelCount and sets
	//! them in pDest (the full chain is spit in two images). Otherwise generate the mips missing 
	//! from pBase up to levelCount and append them in pDest to the already generated pBase's mips.
    //---------------------------------------------------------------------------------------------
    inline void ImageMipmap( int imageCompressionQuality, Image* pDest, const Image* pBase,
                             int levelCount, SCRATCH_IMAGE_MIPMAP* scratch, 
							 const FMipmapGenerationSettings& settings, bool bGenerateOnlyTail = false )
    {
        int startLevel = pBase->GetLODCount() - 1;

		check(!(pBase->m_flags & Image::IF_CANNOT_BE_SCALED));

	
		if (!bGenerateOnlyTail)
		{
			check(pDest->GetLODCount() == levelCount);
			check(pDest->GetSizeX() == pBase->GetSizeX());
			check(pDest->GetSizeY() == pBase->GetSizeY());
		}
		else
		{	
			check(pDest->GetLODCount() + pBase->GetLODCount() == levelCount);

			check(
				[&]() -> bool
				{
					const FIntVector2 BaseImageNextMipSize = pBase->CalculateMipSize(startLevel + 1);
					return BaseImageNextMipSize.X == pDest->GetSizeX() && BaseImageNextMipSize.Y == pDest->GetSizeY();
				}());
		}

        check(pDest->GetFormat() == pBase->GetFormat());

        // Calculate the data size, since the base could have more data allocated.
		int32 BaseMipDataSize = 0;
		if (!bGenerateOnlyTail)
		{
			BaseMipDataSize = pBase->GetMipsDataSize();
			check(pDest->GetDataSize() >= BaseMipDataSize);
			FMemory::Memcpy(pDest->GetData(), pBase->GetData(), BaseMipDataSize);
		}

        int mipsToBuild = levelCount - startLevel - 1;
        if (!mipsToBuild)
        {
            return;
        }

        const uint8* pSourceBuf = !bGenerateOnlyTail ? pDest->GetMipData(startLevel) : pBase->GetMipData(startLevel);

        uint8* pDestBuf = !bGenerateOnlyTail ? pDest->GetMipData(startLevel + 1) : pDest->GetMipData(0);
	
		FIntVector2 sourceSize = pBase->CalculateMipSize(startLevel);

        switch (pBase->GetFormat())
        {
        case EImageFormat::IF_L_UBYTE:
            GenerateMipmapsUint8<1>(levelCount - startLevel - 1, pSourceBuf, pDestBuf, sourceSize, settings);
            break; 

        case EImageFormat::IF_RGB_UBYTE:
            GenerateMipmapsUint8<3>(levelCount - startLevel - 1, pSourceBuf, pDestBuf, sourceSize, settings);
            break;

        case EImageFormat::IF_BGRA_UBYTE:
        case EImageFormat::IF_RGBA_UBYTE:
            GenerateMipmapsUint8<4>(levelCount - startLevel - 1, pSourceBuf, pDestBuf, sourceSize, settings);
            break;

        // Bad cases: we need to decompress, mip and then recompress. It may be necessary to
        // generate the latests mips after composing, or in unoptimised code.
        // TODO: Make sure that the code optimisations avoid this cases generating separate
        // operations to generate the mip tail.
        case EImageFormat::IF_BC1:
        case EImageFormat::IF_BC2:
        case EImageFormat::IF_BC3:
        case EImageFormat::IF_BC4:
        case EImageFormat::IF_BC5:
        case EImageFormat::IF_ASTC_4x4_RGB_LDR:
        case EImageFormat::IF_ASTC_4x4_RGBA_LDR:
        case EImageFormat::IF_ASTC_4x4_RG_LDR:
        case EImageFormat::IF_L_UBIT_RLE:
        case EImageFormat::IF_L_UBYTE_RLE:
        {
            // Uncompress the last mip that we already have
            bool bSuccess = ImagePixelFormatInPlace(imageCompressionQuality, scratch->pUncompressed.get(), pBase, startLevel);
			check(bSuccess);

            // Generate the mipmaps from there on

			constexpr bool bGenerateOnlyTailForCompressed = true;
            ImageMipmap(imageCompressionQuality, scratch->pUncompressedMips.get(),
                        scratch->pUncompressed.get(), levelCount - startLevel, 0, settings, bGenerateOnlyTailForCompressed);

            // Compress the mipmapped image
			bSuccess = ImagePixelFormatInPlace(imageCompressionQuality, scratch->pCompressedMips.get(), scratch->pUncompressedMips.get());
			int32 ExcessDataSize = FMath::Max(2, scratch->pCompressedMips->GetDataSize());
			while (!bSuccess)
			{
				// Bad case: this should almost never happen.
				MUTABLE_CPUPROFILER_SCOPE(Mipmap_Recompression_OutOfSpace);

				scratch->pCompressedMips->m_data.SetNum(ExcessDataSize);
				bSuccess = ImagePixelFormatInPlace(imageCompressionQuality, scratch->pCompressedMips.get(), scratch->pUncompressedMips.get());
				ExcessDataSize *= 4;
			}

			check(!bGenerateOnlyTail || BaseMipDataSize == 0);
			const int32 FinalDestSize = BaseMipDataSize + scratch->pCompressedMips->GetDataSize();

			if (FinalDestSize > pDest->GetDataSize())
			{
				// Bad case: this should almost never happen.
				pDest->m_data.SetNum(FinalDestSize);
				pDestBuf = !bGenerateOnlyTail ? pDest->GetMipData(startLevel + 1) : pDest->GetMipData(0);
			}

            FMemory::Memcpy(pDestBuf, scratch->pCompressedMips->GetData(), scratch->pCompressedMips->GetDataSize());
            break;
        }

        default:
            checkf( false, TEXT("Format not implemented in mipmap generation."));
        }
    }


	/** Update all the mipmaps in the image from the data in the base one. 
	* Only the mipmaps already existing in the image are updated.
	*/
	inline void ImageMipmapInPlace(int InImageCompressionQuality, Image* InBase, const FMipmapGenerationSettings& InSettings)
	{
		int32 StartLevel = 0;
		int32 LevelCount = InBase->GetLODCount();

		check(!(InBase->m_flags & Image::IF_CANNOT_BE_SCALED));

		int32 MipsToBuild = InBase->GetLODCount()-1;
		if (!MipsToBuild)
		{
			return;
		}

		const uint8* pSourceBuf = InBase->GetMipData(StartLevel);
		uint8* pDestBuf = InBase->GetMipData(StartLevel + 1);

		FIntVector2 sourceSize = InBase->CalculateMipSize(StartLevel);

		switch (InBase->GetFormat())
		{
		case EImageFormat::IF_L_UBYTE:
			GenerateMipmapsUint8<1>(MipsToBuild, pSourceBuf, pDestBuf, sourceSize, InSettings);
			break;

		case EImageFormat::IF_RGB_UBYTE:
			GenerateMipmapsUint8<3>(MipsToBuild, pSourceBuf, pDestBuf, sourceSize, InSettings);
			break;

		case EImageFormat::IF_BGRA_UBYTE:
		case EImageFormat::IF_RGBA_UBYTE:
			GenerateMipmapsUint8<4>(MipsToBuild, pSourceBuf, pDestBuf, sourceSize, InSettings);
			break;

		default:
			checkf(false, TEXT("Format not implemented in mipmap generation."));
		}
	}

}
