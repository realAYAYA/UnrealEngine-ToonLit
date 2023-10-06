// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaTextureSampleConverter.h"

class FMediaIOCoreTextureSampleBase;


/**
 * Media IO base texture sample converter.
 *
 * It's mostly responsible for Just-In-Time Rendering (JITR), but also provides a way
 * to implement custom sample conversion when inherited.
 * 
 * @note: Don't forget to call FMediaIOTextureSampleConverter::Convert(...) from the child classes
 *        to trigger JITR pipeline. This is the place where late sample picking is actually happens.
 */
class MEDIAIOCORE_API FMediaIOCoreTextureSampleConverter
	: public IMediaTextureSampleConverter
{
public:
	FMediaIOCoreTextureSampleConverter() = default;
	virtual ~FMediaIOCoreTextureSampleConverter() = default;

public:
	/** Configures settings to convert incoming sample */
	virtual void Setup(const TSharedPtr<FMediaIOCoreTextureSampleBase>& InSample);

public:
	//~ Begin IMediaTextureSampleConverter interface
	virtual bool Convert(FTexture2DRHIRef& InDstTexture, const FConversionHints& Hints) override;
	virtual uint32 GetConverterInfoFlags() const override;
	//~ End IMediaTextureSampleConverter interface

private:

	/** Proxy sample for JITR */
	TWeakPtr<FMediaIOCoreTextureSampleBase> JITRProxySample;
};
