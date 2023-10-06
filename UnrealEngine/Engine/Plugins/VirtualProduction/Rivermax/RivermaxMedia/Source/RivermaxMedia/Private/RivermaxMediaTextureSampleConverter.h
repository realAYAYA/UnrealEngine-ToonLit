// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaTextureSampleConverter.h"

#include "RivermaxMediaSource.h"


class FRDGPooledBuffer;
class FRDGBuilder;

namespace UE::RivermaxMedia
{

class FRivermaxMediaTextureSample;
class FRivermaxMediaPlayer;

using FPreInputConvertFunc = TUniqueFunction<void(FRDGBuilder& GraphBuilder)>;
using FGetSystemBufferFunc = TFunction<const void *()>;
using FGetGPUBufferFunc = TFunction<TRefCountPtr<FRDGPooledBuffer>()>;
using FPostInputConvertFunc = TUniqueFunction<void(FRDGBuilder& GraphBuilder)>;

/** Structure used during late update to let player configure some operations */
struct FSampleConverterOperationSetup
{


	/** Function to be called before setting up the sample conversion graph */
	FPreInputConvertFunc PreConvertFunc = nullptr;

	/** Function used to retrieve which system buffer to use. Can block until data is available. */
	FGetSystemBufferFunc GetSystemBufferFunc = nullptr;
	
	/** Function used to retrieve gpu buffer if available */
	FGetGPUBufferFunc GetGPUBufferFunc = nullptr;

	/** Function to be called after setting up the sample conversion graph */
	FPostInputConvertFunc PostConvertFunc = nullptr;
};


/**
 * Sample converter used for 2110 video samples. Supports some of the pixel formats
 * from 2110-20. Data is expected to be packed in a buffer and will be converted to 
 * an output texture to be rendered.
 */
class FRivermaxMediaTextureSampleConverter : public IMediaTextureSampleConverter
{
public:

	FRivermaxMediaTextureSampleConverter() = default;
	~FRivermaxMediaTextureSampleConverter() = default;

	/** Configures settings to convert incoming sample */
	void Setup(const TSharedPtr<FRivermaxMediaTextureSample>& InSample);

	//~ Begin IMediaTextureSampleConverter interface
	virtual bool Convert(FTexture2DRHIRef& InDstTexture, const FConversionHints& Hints) override;
	virtual uint32 GetConverterInfoFlags() const override;
	//~ End IMediaTextureSampleConverter interface

private:

	/** Sample holding the buffer to convert */
	TWeakPtr<FRivermaxMediaTextureSample> Sample;
};

}
