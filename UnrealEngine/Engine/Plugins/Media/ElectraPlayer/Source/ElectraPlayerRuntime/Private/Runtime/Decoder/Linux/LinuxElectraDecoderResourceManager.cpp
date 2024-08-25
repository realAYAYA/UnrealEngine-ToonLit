// Copyright Epic Games, Inc. All Rights Reserved.

#include "LinuxElectraDecoderResourceManager.h"

#if PLATFORM_LINUX || PLATFORM_UNIX

#include "IElectraDecoderOutputVideo.h"
#include "ElectraDecodersUtils.h"
#include "Renderer/RendererBase.h"
#include "ElectraVideoDecoder_Linux.h"

#if WITH_LIBAV
#include "libav_Decoder_Common_Video.h"
#endif


namespace Electra
{

bool FElectraDecoderResourceManagerLinux::Startup()
{
	return true;
}

void FElectraDecoderResourceManagerLinux::Shutdown()
{
}

TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> FElectraDecoderResourceManagerLinux::GetDelegate()
{
	static TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> Self = MakeShared<FElectraDecoderResourceManagerLinux, ESPMode::ThreadSafe>();
	return Self;
}


FElectraDecoderResourceManagerLinux::~FElectraDecoderResourceManagerLinux()
{
}

#if WITH_LIBAV
namespace LibavcodecConversion
{
static void ConvertDecodedImageToNV12(TArray<uint8>& OutNV12Buffer, FIntPoint OutBufDim, int32 NumBits, ILibavDecoderDecodedImage* InImage)
{
	check(InImage);
	const ILibavDecoderVideoCommon::FOutputInfo& ImageInfo = InImage->GetOutputInfo();
	if (OutNV12Buffer.GetData())
	{
		if (ImageInfo.NumPlanes == 2 &&
			ImageInfo.Planes[0].Content == ILibavDecoderVideoCommon::FPlaneInfo::EContent::Luma &&
			ImageInfo.Planes[1].Content == ILibavDecoderVideoCommon::FPlaneInfo::EContent::ChromaUV)
		{
			if (ImageInfo.Planes[0].BytesPerPixel == 1)
			{
				const int32 w = ImageInfo.Planes[0].Width;
				const int32 h = ImageInfo.Planes[0].Height;
				const int32 aw = ((w + 1) / 2) * 2;
				const int32 ah = ((h + 1) / 2) * 2;
				uint8* DstY = OutNV12Buffer.GetData();
				uint8* DstUV = DstY + aw * ah;
				const uint8* SrcY = (const uint8*)ImageInfo.Planes[0].Address;
				const uint8* SrcUV = (const uint8*)ImageInfo.Planes[1].Address;
				// To simplify the conversion we require the output buffer to have the dimension of the planes.
				check(OutBufDim.X == aw && OutBufDim.Y == ah);
				if (!SrcY || !SrcUV || OutBufDim.X != aw || OutBufDim.Y != ah)
				{
					return;
				}
				if ((w & 1) == 0)
				{
					FMemory::Memcpy(DstY, SrcY, w*h);
					FMemory::Memcpy(DstUV, SrcUV, w*h/2);
				}
				else
				{
					for(int32 y=0; y<h; ++y)
					{
						FMemory::Memcpy(DstY, SrcY, w);
						DstY += aw;
						SrcY += w;
					}
					for(int32 y=0; y<h/2; ++y)
					{
						FMemory::Memcpy(DstUV, SrcUV, w);
						DstUV += aw;
						SrcUV += w;
					}
				}
			}
			else if (ImageInfo.Planes[0].BytesPerPixel == 2)
			{
				const int32 w = ImageInfo.Planes[0].Width;
				const int32 h = ImageInfo.Planes[0].Height;
				const int32 aw = ((w + 1) / 2) * 2;
				const int32 ah = ((h + 1) / 2) * 2;
				uint16* DstY = (uint16*)OutNV12Buffer.GetData();
				uint16* DstUV = DstY + aw * ah;
				const uint16* SrcY = (const uint16*)ImageInfo.Planes[0].Address;
				const uint16* SrcUV = (const uint16*)ImageInfo.Planes[1].Address;
				// To simplify the conversion we require the output buffer to have the dimension of the planes.
				check(OutBufDim.X == aw && OutBufDim.Y == ah);
				if (!SrcY || !SrcUV || OutBufDim.X != aw || OutBufDim.Y != ah)
				{
					return;
				}
#if 0
				// Copy as 16 bits
				if ((w & 1) == 0)
				{
					FMemory::Memcpy(DstY, SrcY, w*h*2);
					FMemory::Memcpy(DstUV, SrcUV, w*h);
				}
				else
				{
					for(int32 y=0; y<h; ++y)
					{
						FMemory::Memcpy(DstY, SrcY, w*2);
						DstY += aw;
						SrcY += w;
					}
					for(int32 y=0; y<h/2; ++y)
					{
						FMemory::Memcpy(DstUV, SrcUV, w*2);
						DstUV += aw;
						SrcUV += w;
					}
				}
#else
				// Convert down to 8 bits
				uint8* Dst8Y = OutNV12Buffer.GetData();
				uint8* Dst8UV = Dst8Y + aw * ah;
				for(int32 y=0; y<h; ++y)
				{
					for(int32 x=0; x<w; ++x)
					{
						Dst8Y[x] = (*SrcY++) >> 8;
					}
					Dst8Y += aw;
				}
				for(int32 y=0; y<h/2; ++y)
				{
					for(int32 u=0; u<w/2; ++u)
					{
						Dst8UV[u*2+0] = (*SrcUV++) >> 8;
						Dst8UV[u*2+1] = (*SrcUV++) >> 8;
					}
					Dst8UV += aw;
				}
#endif
			}
		}
		else if (ImageInfo.NumPlanes == 3 &&
				 ImageInfo.Planes[0].Content == ILibavDecoderVideoCommon::FPlaneInfo::EContent::Luma &&
				 ImageInfo.Planes[1].Content == ILibavDecoderVideoCommon::FPlaneInfo::EContent::ChromaU &&
				 ImageInfo.Planes[2].Content == ILibavDecoderVideoCommon::FPlaneInfo::EContent::ChromaV)
		{
			if (ImageInfo.Planes[0].BytesPerPixel == 1)
			{
				const int32 w = ImageInfo.Planes[0].Width;
				const int32 h = ImageInfo.Planes[0].Height;
				const int32 aw = ((w + 1) / 2) * 2;
				const int32 ah = ((h + 1) / 2) * 2;
				uint8* DstY = OutNV12Buffer.GetData();
				uint8* DstUV = DstY + aw * ah;
				const uint8* SrcY = (const uint8*)ImageInfo.Planes[0].Address;
				const uint8* SrcU = (const uint8*)ImageInfo.Planes[1].Address;
				const uint8* SrcV = (const uint8*)ImageInfo.Planes[2].Address;
				// To simplify the conversion we require the output buffer to have the dimension of the planes.
				check(OutBufDim.X == aw && OutBufDim.Y == ah);
				if (!SrcY || !SrcU || !SrcV || OutBufDim.X != aw || OutBufDim.Y != ah)
				{
					return;
				}
				if ((w & 1) == 0)
				{
					FMemory::Memcpy(DstY, SrcY, w*h);
					for(int32 i=0, iMax=w*h/4; i<iMax; ++i)
					{
						*DstUV++ = *SrcU++;
						*DstUV++ = *SrcV++;
					}
				}
				else
				{
					for(int32 y=0; y<h; ++y)
					{
						FMemory::Memcpy(DstY, SrcY, w);
						DstY += aw;
						SrcY += w;
					}
					int32 padUV = (aw - w) * 2;
					for(int32 v=0; v<h/2; ++v)
					{
						for(int32 u=0; u<w/2; ++u)
						{
							*DstUV++ = *SrcU++;
							*DstUV++ = *SrcV++;
						}
						DstUV += padUV;
					}
				}
			}
			else if (ImageInfo.Planes[0].BytesPerPixel == 2)
			{
				const int32 w = ImageInfo.Planes[0].Width;
				const int32 h = ImageInfo.Planes[0].Height;
				const int32 aw = ((w + 1) / 2) * 2;
				const int32 ah = ((h + 1) / 2) * 2;
			// The destination is 8 bits only!
				uint8* DstY = OutNV12Buffer.GetData();
				uint8* DstUV = DstY + aw * ah;
				const uint16* SrcY = (const uint16*)ImageInfo.Planes[0].Address;
				const uint16* SrcU = (const uint16*)ImageInfo.Planes[1].Address;
				const uint16* SrcV = (const uint16*)ImageInfo.Planes[2].Address;
				// To simplify the conversion we require the output buffer to have the dimension of the planes.
				check(OutBufDim.X == aw && OutBufDim.Y == ah);
				if (!SrcY || !SrcU || !SrcV || OutBufDim.X != aw || OutBufDim.Y != ah)
				{
					return;
				}
				for(int32 y=0; y<h; ++y)
				{
					for(int32 x=0; x<w; ++x)
					{
						DstY[x] = SrcY[x] >> 2;
					}
					DstY += aw;
					SrcY += w;
				}
				int32 padUV = (aw - w) * 2;
				for(int32 v=0; v<h/2; ++v)
				{
					for(int32 u=0; u<w/2; ++u)
					{
						*DstUV++ = (*SrcU++) >> 2;
						*DstUV++ = (*SrcV++) >> 2;
					}
					DstUV += padUV;
				}
			}
		}
	}
}
} // LibavcodecConversion
#endif


bool FElectraDecoderResourceManagerLinux::SetupRenderBufferFromDecoderOutput(IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, IDecoderPlatformResource* InPlatformSpecificResource)
{
	check(InOutBufferToSetup);
	check(InOutBufferPropertes.IsValid());
	check(InDecoderOutput.IsValid());

	TMap<FString, FVariant> ExtraValues;
	InDecoderOutput->GetExtraValues(ExtraValues);

	TSharedPtr<FElectraPlayerVideoDecoderOutputLinux, ESPMode::ThreadSafe> DecoderOutput = InOutBufferToSetup->GetBufferProperties().GetValue(RenderOptionKeys::Texture).GetSharedPointer<FElectraPlayerVideoDecoderOutputLinux>();
	if (DecoderOutput.IsValid())
	{
		ILibavDecoderDecodedImage* DecodedImage = reinterpret_cast<ILibavDecoderDecodedImage*>(InDecoderOutput->GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType::LibavDecoderDecodedImage));
		IElectraDecoderVideoOutputImageBuffers* ImageBuffers = !DecodedImage ? reinterpret_cast<IElectraDecoderVideoOutputImageBuffers*>(InDecoderOutput->GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType::ImageBuffers)) : nullptr;

		if (DecodedImage || ImageBuffers)
		{
			FElectraVideoDecoderOutputCropValues Crop = InDecoderOutput->GetCropValues();
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::Width, FVariantValue((int64)InDecoderOutput->GetWidth()));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::Height, FVariantValue((int64)InDecoderOutput->GetHeight()));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::CropLeft, FVariantValue((int64)Crop.Left));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::CropRight, FVariantValue((int64)Crop.Right));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::CropTop, FVariantValue((int64)Crop.Top));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::CropBottom, FVariantValue((int64)Crop.Bottom));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::AspectRatio, FVariantValue((double)InDecoderOutput->GetAspectRatioW() / (double)InDecoderOutput->GetAspectRatioH()));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::AspectW, FVariantValue((int64)InDecoderOutput->GetAspectRatioW()));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::AspectH, FVariantValue((int64)InDecoderOutput->GetAspectRatioH()));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::FPSNumerator, FVariantValue((int64)InDecoderOutput->GetFrameRateNumerator()));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::FPSDenominator, FVariantValue((int64)InDecoderOutput->GetFrameRateDenominator()));

			int32 num_bits = InDecoderOutput->GetNumberOfBits();
		// We are currently converting output to 8 bit NV12 format!
			num_bits = 8;
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::BitsPerComponent, FVariantValue((int64) num_bits));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::PixelFormat, FVariantValue((int64)((num_bits <= 8) ? EPixelFormat::PF_NV12 : EPixelFormat::PF_P010)));

			if (DecodedImage)
			{
#if WITH_LIBAV
				InOutBufferPropertes->Set(IDecoderOutputOptionNames::Pitch, FVariantValue((int64)InDecoderOutput->GetDecodedWidth() * ((num_bits <= 8) ? 1 : 2)));

				const ILibavDecoderVideoCommon::FOutputInfo& DecodedImageInfo = DecodedImage->GetOutputInfo();
				FIntPoint BufferDim(DecodedImageInfo.Planes[0].Width, DecodedImageInfo.Planes[0].Height);
				if (DecoderOutput->InitializeForBuffer(BufferDim, num_bits <= 8 ? EPixelFormat::PF_NV12 : EPixelFormat::PF_P010, num_bits, InOutBufferPropertes, true))
				{
					LibavcodecConversion::ConvertDecodedImageToNV12(*DecoderOutput->GetMutableBuffer(), DecoderOutput->GetBufferDimensions(), num_bits, DecodedImage);
					// Have the decoder output keep a reference to the decoded image in case it will share data with it at some point.
					DecoderOutput->SetDecodedImage(DecodedImage->AsShared());
					return true;
				}
#endif
			}
			else if (ImageBuffers && ImageBuffers->GetNumberOfBuffers() == 1)
			{
				// Compatible format?
				if ((ImageBuffers->GetBufferFormatByIndex(0) == EElectraDecoderPlatformPixelFormat::NV12 || ImageBuffers->GetBufferFormatByIndex(0) == EElectraDecoderPlatformPixelFormat::P010)&&
					ImageBuffers->GetBufferEncodingByIndex(0) == EElectraDecoderPlatformPixelEncoding::Native)
				{
					InOutBufferPropertes->Set(IDecoderOutputOptionNames::Pitch, FVariantValue((int64)ImageBuffers->GetBufferPitchByIndex(0)));

					FIntPoint BufferDim((int32) InDecoderOutput->GetWidth(), (int32) InDecoderOutput->GetHeight());
					if (DecoderOutput->InitializeForBuffer(BufferDim, num_bits <= 8 ? EPixelFormat::PF_NV12 : EPixelFormat::PF_P010, num_bits, InOutBufferPropertes, false))
					{
						// Share the output buffer.
						DecoderOutput->GetMutableBuffer() = ImageBuffers->GetBufferDataByIndex(0);
						return true;
					}
				}
			}
		}
	}
	return false;
}

} // namespace Electra

#endif
