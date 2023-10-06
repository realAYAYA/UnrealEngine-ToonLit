// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/ImagePrivate.h"
#include "MuR/SystemPrivate.h"
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

		template<int32 PIXEL_SIZE, typename CHANNEL_TYPE, EAddressMode AD_MODE = EAddressMode::Wrap>
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
				if constexpr (AD_MODE == EAddressMode::Wrap)
				{
					// (y & (m_dim.Y - 1)) * m_dim.Y + (x & (m_dim.X - 1))
					return m_pImageBuf[((y % m_dim.Y) * m_dim.X + (x % m_dim.X)) * PIXEL_SIZE + c];
				}
				else if (AD_MODE == EAddressMode::ClampToEdge)
				{
					return m_pImageBuf[(mu::clamp(y, 0, m_dim.Y - 1) * m_dim.X +
						mu::clamp(x, 0, m_dim.X - 1))
						* PIXEL_SIZE + c];
				}
				else if (AD_MODE == EAddressMode::ClampToBlack)
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
			PIXEL_TYPE* Dest, FIntVector2 destSize,
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

						Dest[(y * destSize.X + x) * PIXEL_SIZE + c] = static_cast<uint8>(mu::clamp(sampleValue, 0.0f, 1.0f) * 255.0f);
					}
				}
			}
		}

		template<size_t PIXEL_SIZE, EAddressMode AD_MODE>
		inline void GenerateMipmapUint8Sharpen(int mips,
			const uint8* pSource, uint8* Dest,
			FIntVector2 sourceSize,
			float sharpeningFactor)
		{
			SharpenKernelStorage8 kernel = MakeMipGaussianSharpenKernel(sharpeningFactor);

			size_t totalDestSize = 0;

			for (; mips >= 0; --mips)
			{
				ImageAccessor<PIXEL_SIZE, uint8, AD_MODE> sourceSampler(pSource, sourceSize);
				ImageAccessor<PIXEL_SIZE, uint8, EAddressMode::None> sourceSamplerAddressModeNone(pSource, sourceSize);

				FIntVector2 destSize = FIntVector2(FMath::DivideAndRoundUp(sourceSize.X, 2), FMath::DivideAndRoundUp(sourceSize.Y, 2));

				// Core image, will never sample outside the image so we don't need to care about address mode.
				// Only sample source pixels where the kernel can be fully applied.
				// Border cases are treated separately.
				GenerateMipSharpenedRegion(
					kernel,
					sourceSamplerAddressModeNone,
					Dest, destSize,
					FIntVector2(2, 2), FIntVector2(destSize.X - 2, destSize.Y - 2));

				// Horizontal borders.
				GenerateMipSharpenedRegion(
					kernel,
					sourceSampler,
					Dest, destSize,
					FIntVector2(0, 0), FIntVector2(destSize.X, FMath::Min(2, destSize.Y)));

				GenerateMipSharpenedRegion(
					kernel,
					sourceSampler,
					Dest, destSize,
					FIntVector2(0, FMath::Max(destSize.Y - 2, 0)), FIntVector2(destSize.X, destSize.Y));

				// Vertical borders
				GenerateMipSharpenedRegion(
					kernel,
					sourceSampler,
					Dest, destSize,
					FIntVector2(0, FMath::Min(2, destSize.Y)), FIntVector2(FMath::Min(2, destSize.X), destSize.Y - 2));

				GenerateMipSharpenedRegion(
					kernel,
					sourceSampler,
					Dest, destSize,
					FIntVector2(destSize.X - 2, FMath::Min(2, destSize.Y)), FIntVector2(destSize.X, destSize.Y - 2));

				sourceSize = destSize;

				pSource = Dest;
				Dest = Dest + destSize.X * destSize.Y * PIXEL_SIZE;

				totalDestSize += sourceSize.X * sourceSize.Y * PIXEL_SIZE;
			}
		}

		template<int32 PIXEL_SIZE>
		inline void GenerateMipmapUint8Unfiltered(
			int mips,
			const uint8* pSource, uint8* Dest,
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
							Dest[(y * destSize.X + x) * PIXEL_SIZE + c] =
								pSource[((y << 1) * sourceSize.X + (x << 1)) * PIXEL_SIZE + c];
						}
					}
				}

				sourceSize = destSize;
				pSource = Dest;
				Dest = Dest + destSize.X * destSize.Y * PIXEL_SIZE;
			}
		}

		template<int32 PIXEL_SIZE>
		inline void GenerateMipmapsUint8SimpleAverage(
			int mips,
			const uint8* pSource, uint8* Dest,
			FIntVector2 sourceSize)
		{
			const uint8* pMipSource = pSource;
			uint8* pMipDest = Dest;

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


	/** Generate the mipmaps for byte-based images of whatever number of channels.
	* \param mips number of additional levels to build from the source.
	*/
	template<int32 PIXEL_SIZE>
	inline void GenerateMipmapsUint8(int32 mips,
		const uint8* pSource, uint8* Dest,
		FIntVector2 sourceSize,
		const FMipmapGenerationSettings& settings)
	{
		using namespace OpImageMipmap_Detail;

		switch (settings.m_filterType)
		{
		case EMipmapFilterType::MFT_SimpleAverage:
		{
			GenerateMipmapsUint8SimpleAverage<PIXEL_SIZE>(mips, pSource, Dest, sourceSize);
			break;
		}
		case EMipmapFilterType::MFT_Sharpen:
		{
			switch (settings.m_addressMode)
			{
			case EAddressMode::ClampToEdge:
			{
				GenerateMipmapUint8Sharpen<PIXEL_SIZE, EAddressMode::ClampToEdge>(
					mips, pSource, Dest, sourceSize, settings.m_sharpenFactor);
				break;
			}
			case EAddressMode::Wrap:
			{
				GenerateMipmapUint8Sharpen<PIXEL_SIZE, EAddressMode::Wrap>(
					mips, pSource, Dest, sourceSize, settings.m_sharpenFactor);
				break;
			}
			case EAddressMode::ClampToBlack:
			{
				GenerateMipmapUint8Sharpen<PIXEL_SIZE, EAddressMode::ClampToBlack>(
					mips, pSource, Dest, sourceSize, settings.m_sharpenFactor);
				break;
			}
			default:
			{
				GenerateMipmapUint8Sharpen<PIXEL_SIZE, EAddressMode::None>(
					mips, pSource, Dest, sourceSize, settings.m_sharpenFactor);
				break;
			}
			}

			break;
		}
		case EMipmapFilterType::MFT_Unfiltered:
		{
			GenerateMipmapUint8Unfiltered<PIXEL_SIZE>(mips, pSource, Dest, sourceSize);
			break;
		}
		default:
		{
			check(false);
			break;
		}
		}
	}



    //---------------------------------------------------------------------------------------------
    void FImageOperator::ImageMipmap_PrepareScratch(Image* Dest, const Image* Base, int32 LevelCount, FScratchImageMipmap& Scratch )
    {
        int StartLevel = Base->GetLODCount() - 1;

        check(Dest->GetLODCount() == LevelCount);
        check(Dest->GetSizeX() == Base->GetSizeX());
        check(Dest->GetSizeY() == Base->GetSizeY());
        check(Dest->GetFormat() == Base->GetFormat());

        switch (Base->GetFormat())
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
			FIntVector2 UncompressedSize = Base->CalculateMipSize(StartLevel);
			Scratch.Uncompressed = CreateImage( 
				(uint16)UncompressedSize[0], (uint16)UncompressedSize[1], 
				1, 
				EImageFormat::IF_RGBA_UBYTE, EInitializationType::NotInitialized );

			FIntVector2 UncompressedMipsSize = Base->CalculateMipSize(StartLevel + 1);
			// Generate the mipmaps from there on
			Scratch.UncompressedMips = CreateImage( 
				(uint16)UncompressedMipsSize[0], (uint16)UncompressedMipsSize[1],
				FMath::Max(1, LevelCount - StartLevel - 1), 
				EImageFormat::IF_RGBA_UBYTE, EInitializationType::NotInitialized);

			// Compress the mipmapped image
			Scratch.CompressedMips = CreateImage(
				(uint16)UncompressedMipsSize[0], (uint16)UncompressedMipsSize[1],
				Scratch.UncompressedMips->GetLODCount(),
				Base->GetFormat(), EInitializationType::NotInitialized);

            break;
        }

        case EImageFormat::IF_L_UBIT_RLE:
        case EImageFormat::IF_L_UBYTE_RLE:
        {
            // Uncompress the last mip that we already have
			FIntVector2 UncompressedSize = Base->CalculateMipSize(StartLevel);
            Scratch.Uncompressed = CreateImage(
                (uint16)UncompressedSize[0], (uint16)UncompressedSize[1],
                1,
                EImageFormat::IF_L_UBYTE, EInitializationType::NotInitialized);


			FIntVector2 UncompressedMipsSize = Base->CalculateMipSize(StartLevel + 1);
            // Generate the mipmaps from there on
            Scratch.UncompressedMips = CreateImage(
                (uint16)UncompressedMipsSize[0], (uint16)UncompressedMipsSize[1],
				FMath::Max(1, LevelCount - StartLevel - 1),
                EImageFormat::IF_L_UBYTE, EInitializationType::NotInitialized);


            // Compress the mipmapped image
            Scratch.CompressedMips = CreateImage(
                (uint16)UncompressedMipsSize[0], (uint16)UncompressedMipsSize[1],
                Scratch.UncompressedMips->GetLODCount(),
                Base->GetFormat(), EInitializationType::NotInitialized);

            // Preallocate ample memory for the compressed data
			uint32 TotalMemory = Scratch.UncompressedMips->GetDataSize();
            Scratch.CompressedMips->m_data.SetNumUninitialized(TotalMemory);

            // Preallocate ample memory for the destination data
			TotalMemory = Base->GetDataSize() + Scratch.UncompressedMips->GetDataSize();
            Dest->m_data.SetNumUninitialized(TotalMemory);

            break;
        }

        default:
            checkf( false, TEXT("Format not implemented in mipmap generation."));
        }
    }


	void FImageOperator::ImageMipmap_ReleaseScratch(FScratchImageMipmap& Scratch)
	{
		ReleaseImage(Scratch.Uncompressed);
		ReleaseImage(Scratch.UncompressedMips);
		ReleaseImage(Scratch.CompressedMips);
	}


	void FImageOperator::ImageMipmap(FScratchImageMipmap& Scratch, int32 CompressionQuality, Image* Dest, const Image* Base,
		int32 LevelCount,
		const FMipmapGenerationSettings& Settings, bool bGenerateOnlyTail)
	{
		int32 StartLevel = Base->GetLODCount() - 1;

		check(!(Base->m_flags & Image::IF_CANNOT_BE_SCALED));


		if (!bGenerateOnlyTail)
		{
			check(Dest->GetLODCount() == LevelCount);
			check(Dest->GetSizeX() == Base->GetSizeX());
			check(Dest->GetSizeY() == Base->GetSizeY());
		}
		else
		{
			check(Dest->GetLODCount() + Base->GetLODCount() == LevelCount);

			check(
				[&]() -> bool
				{
					const FIntVector2 BaseImageNextMipSize = Base->CalculateMipSize(StartLevel + 1);
					return BaseImageNextMipSize.X == Dest->GetSizeX() && BaseImageNextMipSize.Y == Dest->GetSizeY();
				}());
		}

		check(Dest->GetFormat() == Base->GetFormat());

		// Calculate the data size, since the base could have more data allocated.
		int32 BaseMipDataSize = 0;
		if (!bGenerateOnlyTail)
		{
			BaseMipDataSize = Base->GetMipsDataSize();
			check(Dest->GetDataSize() >= BaseMipDataSize);
			FMemory::Memcpy(Dest->GetData(), Base->GetData(), BaseMipDataSize);
		}

		int mipsToBuild = LevelCount - StartLevel - 1;
		if (!mipsToBuild)
		{
			return;
		}

		const uint8* pSourceBuf = !bGenerateOnlyTail ? Dest->GetMipData(StartLevel) : Base->GetMipData(StartLevel);

		uint8* pDestBuf = !bGenerateOnlyTail ? Dest->GetMipData(StartLevel + 1) : Dest->GetMipData(0);

		FIntVector2 sourceSize = Base->CalculateMipSize(StartLevel);

		switch (Base->GetFormat())
		{
		case EImageFormat::IF_L_UBYTE:
			GenerateMipmapsUint8<1>(LevelCount - StartLevel - 1, pSourceBuf, pDestBuf, sourceSize, Settings);
			break;

		case EImageFormat::IF_RGB_UBYTE:
			GenerateMipmapsUint8<3>(LevelCount - StartLevel - 1, pSourceBuf, pDestBuf, sourceSize, Settings);
			break;

		case EImageFormat::IF_BGRA_UBYTE:
		case EImageFormat::IF_RGBA_UBYTE:
			GenerateMipmapsUint8<4>(LevelCount - StartLevel - 1, pSourceBuf, pDestBuf, sourceSize, Settings);
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
			bool bSuccess = false;
			ImagePixelFormat(bSuccess, CompressionQuality, Scratch.Uncompressed.get(), Base, StartLevel);
			check(bSuccess);

			// Generate the mipmaps from there on

			constexpr bool bGenerateOnlyTailForCompressed = true;
			ImageMipmap(Scratch, CompressionQuality, Scratch.UncompressedMips.get(),
				Scratch.Uncompressed.get(), LevelCount - StartLevel, Settings, bGenerateOnlyTailForCompressed);

			// Compress the mipmapped image
			bSuccess = false;
			ImagePixelFormat(bSuccess, CompressionQuality, Scratch.CompressedMips.get(), Scratch.UncompressedMips.get());
			int32 ExcessDataSize = FMath::Max(2, Scratch.CompressedMips->GetDataSize());
			while (!bSuccess)
			{
				// Bad case: this should almost never happen.
				MUTABLE_CPUPROFILER_SCOPE(Mipmap_Recompression_OutOfSpace);

				Scratch.CompressedMips->m_data.SetNumUninitialized(ExcessDataSize);
				bSuccess = false;
				ImagePixelFormat(bSuccess, CompressionQuality, Scratch.CompressedMips.get(), Scratch.UncompressedMips.get());
				ExcessDataSize *= 4;
			}

			check(!bGenerateOnlyTail || BaseMipDataSize == 0);
			const int32 FinalDestSize = BaseMipDataSize + Scratch.CompressedMips->GetDataSize();

			if (FinalDestSize > Dest->GetDataSize())
			{
				// Bad case: this should almost never happen.
				Dest->m_data.SetNumUninitialized(FinalDestSize);
				pDestBuf = !bGenerateOnlyTail ? Dest->GetMipData(StartLevel + 1) : Dest->GetMipData(0);
			}

			FMemory::Memcpy(pDestBuf, Scratch.CompressedMips->GetData(), Scratch.CompressedMips->GetDataSize());
			break;
		}

		default:
			checkf(false, TEXT("Format not implemented in mipmap generation."));
		}
	}


	void FImageOperator::ImageMipmap(int32 CompressionQuality, Image* Dest, const Image* Base,
		int32 LevelCount,
		const FMipmapGenerationSettings& Settings, bool bGenerateOnlyTail)
	{
		FScratchImageMipmap Scratch;
		ImageMipmap_PrepareScratch(Dest, Base, LevelCount, Scratch);

		ImageMipmap(Scratch, CompressionQuality, Dest, Base, LevelCount, Settings, bGenerateOnlyTail);

		ImageMipmap_ReleaseScratch(Scratch);
	}


	/** Update all the mipmaps in the image from the data in the base one. 
	* Only the mipmaps already existing in the image are updated.
	*/
	void ImageMipmapInPlace(int32 InImageCompressionQuality, Image* InBase, const FMipmapGenerationSettings& InSettings)
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
