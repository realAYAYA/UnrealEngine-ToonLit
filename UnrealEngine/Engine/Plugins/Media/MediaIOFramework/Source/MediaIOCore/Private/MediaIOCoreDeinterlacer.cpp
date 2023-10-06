// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaIOCoreDeinterlacer.h"

#include "Async/ParallelFor.h"
#include "ColorManagementDefines.h"

namespace UE::MediaIOCore
{
	TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> FDeinterlacer::Deinterlace(const FVideoFrame& InVideoFrame) const
 	{
		TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> Samples;

		const TSharedPtr<FMediaIOCoreTextureSampleBase> TextureSample = AcquireSampleDelegate.Execute();

		if (TextureSample && TextureSample->Initialize(InVideoFrame.VideoBuffer
			, InVideoFrame.BufferSize
			, InVideoFrame.Stride
			, InVideoFrame.Width
			, InVideoFrame.Height
			, InVideoFrame.SampleFormat
			, InVideoFrame.Time
			, InVideoFrame.FrameRate
			, InVideoFrame.Timecode
			, InVideoFrame.ColorFormatArgs))
		{
			Samples.Add(TextureSample.ToSharedRef());
		}

		return Samples;
 	}

	TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> FBobDeinterlacer::Deinterlace(const FVideoFrame& InVideoFrame) const
	{
		TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> Samples;

		bool bEven = true;

		const TSharedPtr<FMediaIOCoreTextureSampleBase> TextureSampleEven = AcquireSampleDelegate.Execute();
		const TSharedPtr<FMediaIOCoreTextureSampleBase> TextureSampleOdd = AcquireSampleDelegate.Execute();

		if (TextureSampleEven && TextureSampleOdd)
		{
			TArray<uint8> EvenBuffer;
			EvenBuffer.Reserve(InVideoFrame.BufferSize);

			TArray<uint8> OddBuffer;
			OddBuffer.Reserve(InVideoFrame.BufferSize);

			for (uint32 IndexY = (InterlaceFieldOrder == EMediaIOInterlaceFieldOrder::TopFieldFirst ? 0 : 1); IndexY < InVideoFrame.Height; IndexY += 2)
			{
				const uint8* Source = reinterpret_cast<const uint8*>(InVideoFrame.VideoBuffer) + (IndexY * InVideoFrame.Stride);
				EvenBuffer.Append(Source, InVideoFrame.Stride);
				EvenBuffer.Append(Source, InVideoFrame.Stride);
			}
			
			for (uint32 IndexY = (InterlaceFieldOrder == EMediaIOInterlaceFieldOrder::TopFieldFirst ? 1 : 0); IndexY < InVideoFrame.Height; IndexY += 2)
			{
				const uint8* Source = reinterpret_cast<const uint8*>(InVideoFrame.VideoBuffer) + (IndexY * InVideoFrame.Stride);
				OddBuffer.Append(Source, InVideoFrame.Stride);
				OddBuffer.Append(Source, InVideoFrame.Stride);
			}

			if (TextureSampleEven->Initialize(
				  MoveTemp(EvenBuffer)
				, InVideoFrame.Stride
				, InVideoFrame.Width
				, InVideoFrame.Height
				, InVideoFrame.SampleFormat
				, InVideoFrame.Time
				, InVideoFrame.FrameRate
				, InVideoFrame.Timecode
				, InVideoFrame.ColorFormatArgs))
			{
				Samples.Add(TextureSampleEven.ToSharedRef());
			}


			// Don't create second sample if first one fails in order to avoid introducing field flipping.
			if (Samples.Num() && TextureSampleOdd->Initialize(
				  MoveTemp(OddBuffer)
				, InVideoFrame.Stride
				, InVideoFrame.Width
				, InVideoFrame.Height
				, InVideoFrame.SampleFormat
				, InVideoFrame.Time
				, InVideoFrame.FrameRate
				, InVideoFrame.Timecode
				, InVideoFrame.ColorFormatArgs))
			{
				Samples.Add(TextureSampleOdd.ToSharedRef());
			}
		}

		return Samples;
	}

	TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> FBlendDeinterlacer::Deinterlace(const FVideoFrame& InVideoFrame) const
	{
		TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> Samples;
		
		if (const TSharedPtr<FMediaIOCoreTextureSampleBase> TextureSample = AcquireSampleDelegate.Execute())
		{
			TArray<uint8> BlendedBuffer;
			BlendedBuffer.AddZeroed(InVideoFrame.BufferSize / 2);

			ParallelFor(InVideoFrame.Height / 2, [&](int32 LineIndex)
			{
				int32 SrcY = LineIndex * 2;
				const uint8* Source1 = static_cast<const uint8*>(InVideoFrame.VideoBuffer) + (SrcY * InVideoFrame.Stride);
				const uint8* Source2 = static_cast<const uint8*>(InVideoFrame.VideoBuffer) + ((SrcY + 1) * InVideoFrame.Stride);

				uint8* Dest = BlendedBuffer.GetData() + LineIndex * InVideoFrame.Stride;
				
				for (uint32 XIndex = 0; XIndex < InVideoFrame.Stride; XIndex++)
				{
					*(Dest + XIndex) =  FMath::DivideAndRoundUp(Source1[XIndex] + Source2[XIndex], 2);
				}
			});

			if (TextureSample->Initialize(
				  MoveTemp(BlendedBuffer)
				, InVideoFrame.Stride
				, InVideoFrame.Width
				, InVideoFrame.Height / 2
				, InVideoFrame.SampleFormat
				, InVideoFrame.Time
				, InVideoFrame.FrameRate
				, InVideoFrame.Timecode
				, InVideoFrame.ColorFormatArgs))
			{
				Samples.Add(TextureSample.ToSharedRef());
			}
		}

		return Samples;
	}

	TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> FDiscardDeinterlacer::Deinterlace(const FVideoFrame& InVideoFrame) const
	{
		TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> Samples;
		if (const TSharedPtr<FMediaIOCoreTextureSampleBase> TextureSample = AcquireSampleDelegate.Execute())
		{
			TArray<uint8> Buffer;
			Buffer.Reserve(InVideoFrame.BufferSize / 2);

			for (uint32 YIndex = InterlaceFieldOrder == EMediaIOInterlaceFieldOrder::TopFieldFirst ? 0 : 1; YIndex < InVideoFrame.Height; YIndex += 2)
			{
				const uint8* Source1 = reinterpret_cast<const uint8*>(InVideoFrame.VideoBuffer) + (YIndex * InVideoFrame.Stride);
				Buffer.Append(Source1, InVideoFrame.Stride);
			}

			if (TextureSample->Initialize(
				  MoveTemp(Buffer)
				, InVideoFrame.Stride
				, InVideoFrame.Width
				, InVideoFrame.Height / 2
				, InVideoFrame.SampleFormat
				, InVideoFrame.Time
				, InVideoFrame.FrameRate
				, InVideoFrame.Timecode
				, InVideoFrame.ColorFormatArgs))
			{
				Samples.Add(TextureSample.ToSharedRef());
			}
		}

		return Samples;
	}
}