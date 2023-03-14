// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TimeSynchronizableMediaSource.h"

#include "MediaIOCoreDefinitions.h"

#include "RivermaxMediaSource.generated.h"

/**
 * Native data format.
 */
UENUM()
enum class ERivermaxMediaSourcePixelFormat : uint8
{
	YUV422_8bit UMETA(DisplayName = "8bit YUV422"),
	YUV422_10bit UMETA(DisplayName = "10bit YUV422"),
	RGB_8bit UMETA(DisplayName = "8bit RGB"),
	RGB_10bit UMETA(DisplayName = "10bit RGB"),
	RGB_12bit UMETA(DisplayName = "12bit RGB"),
	RGB_16bit_Float UMETA(DisplayName = "16bit Float RGB"),
};

/**
 * Media source for Rivermax streams.
 */
UCLASS(BlueprintType, hideCategories=(Platforms,Object), meta=(MediaIOCustomLayout="Rivermax"))
class RIVERMAXMEDIA_API URivermaxMediaSource : public UTimeSynchronizableMediaSource
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Format")
	FIntPoint Resolution = {1920, 1080};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Format")
	FFrameRate FrameRate = {24,1};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Format")
	ERivermaxMediaSourcePixelFormat PixelFormat = ERivermaxMediaSourcePixelFormat::RGB_10bit;

	/** Network card interface to use to receive data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Format")
	FString InterfaceAddress;

	/** IP address where incoming stream is coming from.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Format")
	FString StreamAddress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Format")
	int32 Port = 50000;

	/** Whether the video input is in sRGB color space.If true, sRGBToLinear will be done on incoming pixels before writing to media texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
	bool bIsSRGBInput = false;

public:
	//~ Begin IMediaOptions interface
	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual int64 GetMediaOption(const FName& Key, int64 DefaultValue) const override;
	virtual FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;
	//~ End IMediaOptions interface

public:
	//~ Begin UMediaSource interface
	virtual FString GetUrl() const override;
	virtual bool Validate() const override;
	//~ End UMediaSource interface
};
