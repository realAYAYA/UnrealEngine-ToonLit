// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ColorSpace.h"
#include "Containers/Array.h"
#include "MediaIOCoreDefinitions.h"
#include "MediaIOCoreTextureSampleBase.h"
#include "Templates/SharedPointer.h"

#include "MediaIOCoreDeinterlacer.generated.h"

namespace UE::MediaIOCore
{
	/** Description of a video frame. */
	struct MEDIAIOCORE_API FVideoFrame
	{
		FVideoFrame(const void* InVideoBuffer
		, uint32 InBufferSize
		, uint32 InStride
		, uint32 InWidth
		, uint32 InHeight
		, EMediaTextureSampleFormat InSampleFormat
		, FTimespan InTime
		, const FFrameRate& InFrameRate
		, const TOptional<FTimecode>& InTimecode
		, const UE::MediaIOCore::FColorFormatArgs& InColorFormatArgs)
			: VideoBuffer(InVideoBuffer)
			, BufferSize(InBufferSize)
			, Stride(InStride)
			, Width(InWidth)
			, Height(InHeight)
			, SampleFormat(InSampleFormat)
			, Time(InTime)
			, FrameRate(InFrameRate)
			, Timecode(InTimecode)
			, ColorFormatArgs(InColorFormatArgs)
		{
		}

		const void* VideoBuffer;
		uint32 BufferSize;
		uint32 Stride;
		uint32 Width;
		uint32 Height;
		EMediaTextureSampleFormat SampleFormat;
		FTimespan Time;
		const FFrameRate& FrameRate;
		const TOptional<FTimecode>& Timecode;
		const UE::MediaIOCore::FColorFormatArgs& ColorFormatArgs;
	};

	/**
	 * Handles deinterlacing a video signal.
	 */
	class MEDIAIOCORE_API FDeinterlacer
	{
	public:
		DECLARE_DELEGATE_RetVal(TSharedPtr<FMediaIOCoreTextureSampleBase>, FOnAcquireSample_AnyThread);

		FDeinterlacer() = default;
		FDeinterlacer(FOnAcquireSample_AnyThread InOnAcquireSample, EMediaIOInterlaceFieldOrder InInterlaceFieldOrder) 
			: AcquireSampleDelegate(MoveTemp(InOnAcquireSample))
			, InterlaceFieldOrder(InInterlaceFieldOrder)
		{
		}

		virtual ~FDeinterlacer() {}
        
		/**
		 * Default implementation which applies no deinterlacing.
		 * @param InVideoFrame The video buffer and its metadata.
		 * @return One or more sample depending on the deinterlacing method.
		 */
		virtual TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> Deinterlace(const FVideoFrame& InVideoFrame) const;
	protected:
		/** Delegate called to acquire samples. */
		FOnAcquireSample_AnyThread AcquireSampleDelegate;
		/** What order should fields be read in. */
		EMediaIOInterlaceFieldOrder InterlaceFieldOrder = EMediaIOInterlaceFieldOrder::TopFieldFirst; 
	};
	
	/** Double field lines, keeping temporal resolution but halving texture vertical resolution. */
	class FBobDeinterlacer : public FDeinterlacer
	{
	public:
		FBobDeinterlacer(FOnAcquireSample_AnyThread InOnAcquireSample, EMediaIOInterlaceFieldOrder InInterlaceFieldOrder) 
			: FDeinterlacer(MoveTemp(InOnAcquireSample), InInterlaceFieldOrder)
		{
		}
		virtual TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> Deinterlace(const FVideoFrame& InVideoFrame) const override;
	};

	/** Blend the top and bottom field, halving temporal and texture vertical resoluton. */
	class FBlendDeinterlacer : public FDeinterlacer
	{
	public:
		FBlendDeinterlacer(FOnAcquireSample_AnyThread InOnAcquireSample, EMediaIOInterlaceFieldOrder InInterlaceFieldOrder) 
			: FDeinterlacer(MoveTemp(InOnAcquireSample), InInterlaceFieldOrder)
		{
		}
		virtual TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> Deinterlace(const FVideoFrame& InVideoFrame) const override;
	};

	/** Discards one of the field entirely (based on InterlaceFieldOrder), halving both temporal and spatial resolutions. */
	class FDiscardDeinterlacer : public FDeinterlacer
	{
	public:
		FDiscardDeinterlacer(FOnAcquireSample_AnyThread InOnAcquireSample, EMediaIOInterlaceFieldOrder InInterlaceFieldOrder) 
			: FDeinterlacer(MoveTemp(InOnAcquireSample), InInterlaceFieldOrder)
		{
		}
		virtual TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> Deinterlace(const FVideoFrame& InVideoFrame) const override;
	};
	
	
}

/**
 * Represents a deinterlacing algorithm. Will dictate how the incoming video signal is converted from interlace to a progressive signal.
 */
UCLASS(Abstract, editinlinenew)
class MEDIAIOCORE_API UVideoDeinterlacer : public UObject
{
	GENERATED_BODY()
public:
	/**
	 * Creates an instance of a FDeinterlacer that represents a deinterlacing algorithm.
	 */
	virtual TSharedPtr<UE::MediaIOCore::FDeinterlacer> Instantiate(UE::MediaIOCore::FDeinterlacer::FOnAcquireSample_AnyThread InAcquireSampleDelegate, EMediaIOInterlaceFieldOrder InInterlaceFieldOrder) const PURE_VIRTUAL(UVideoDeinterlacer::Instantiate, return nullptr; );
};


/** Double field lines, keeping temporal resolution but halving texture vertical resolution. */
UCLASS()
class MEDIAIOCORE_API UBobDeinterlacer : public UVideoDeinterlacer
{
	GENERATED_BODY()
public:
	virtual TSharedPtr<UE::MediaIOCore::FDeinterlacer> Instantiate(UE::MediaIOCore::FDeinterlacer::FOnAcquireSample_AnyThread InAcquireSampleDelegate, EMediaIOInterlaceFieldOrder InInterlaceFieldOrder) const override
	{
		return MakeShared<UE::MediaIOCore::FBobDeinterlacer>(MoveTemp(InAcquireSampleDelegate), InInterlaceFieldOrder);
	}
};

/** Blend the top and bottom field, keeping temporal resolution but halving texture vertical resoluton. */
UCLASS()
class MEDIAIOCORE_API UBlendDeinterlacer : public UVideoDeinterlacer
{
	GENERATED_BODY()
public:
	virtual TSharedPtr<UE::MediaIOCore::FDeinterlacer> Instantiate(UE::MediaIOCore::FDeinterlacer::FOnAcquireSample_AnyThread InAcquireSampleDelegate, EMediaIOInterlaceFieldOrder InInterlaceFieldOrder) const override
	{
		return MakeShared<UE::MediaIOCore::FBlendDeinterlacer>(MoveTemp(InAcquireSampleDelegate), InInterlaceFieldOrder);
	}
};

/** Discards one of the field entirely (based on InterlaceFieldOrder), halving both temporal and spatial resolutions. */
UCLASS()
class MEDIAIOCORE_API UDiscardDeinterlacer : public UVideoDeinterlacer
{
	GENERATED_BODY()
public:
	virtual TSharedPtr<UE::MediaIOCore::FDeinterlacer> Instantiate(UE::MediaIOCore::FDeinterlacer::FOnAcquireSample_AnyThread InAcquireSampleDelegate, EMediaIOInterlaceFieldOrder InInterlaceFieldOrder) const override
	{
		return MakeShared<UE::MediaIOCore::FDiscardDeinterlacer>(MoveTemp(InAcquireSampleDelegate), InInterlaceFieldOrder);
	}
};
