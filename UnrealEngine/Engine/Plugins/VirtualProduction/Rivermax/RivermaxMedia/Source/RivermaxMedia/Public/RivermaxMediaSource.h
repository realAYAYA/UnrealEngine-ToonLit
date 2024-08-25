// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TimeSynchronizableMediaSource.h"

#include "MediaIOCoreDefinitions.h"
#include "RivermaxTypes.h"

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
 * Player mode to be used.
 */
UENUM()
enum class ERivermaxPlayerMode : uint8
{
	Latest, // Uses latest sample available
	Framelock // Uses incoming samples frame number to match with local engine frame number. Should be used with nDisplay
};

/**
 * Media source for Rivermax streams.
 */
UCLASS(BlueprintType, hideCategories=(Platforms,Object), meta=(MediaIOCustomLayout="Rivermax", DisplayName = "NVIDIA Rivermax Source"))
class RIVERMAXMEDIA_API URivermaxMediaSource : public UTimeSynchronizableMediaSource
{
	GENERATED_BODY()

public:

	/**
	 * Player mode to be used.
	 * Latest : Default mode. Will use latest available at render time. No alignment.
	 * Framelock : Will match sample's frame number with engine frame number. Meant to be used for UE-UE contexts like for nDisplay
	 *           : Will wait for an expected to arrive before moving with render
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	ERivermaxPlayerMode PlayerMode = ERivermaxPlayerMode::Latest;

	/** 
	 * If true, when looking for the sample to render, the current frame number will be looked for.
	 * If expected frame hasn't been received, waiting will occur.
	 * If false, player will look for one frame behind
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player", meta = (EditCondition = "PlayerMode == ERivermaxPlayerMode::Framelock"))
	bool bUseZeroLatency  = true;

	/** If false, use the default source buffer size. If true, a specific resolution will be used. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Media", meta = (InlineEditConditionToggle))
	bool bOverrideResolution = false;

	/** Incoming stream video resolution */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Options", meta = (EditCondition = "bOverrideResolution"))
	FIntPoint Resolution = {1920, 1080};
	
	/** Incoming stream video frame rate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	FFrameRate FrameRate = {24,1};
	
	/** Incoming stream pixel format */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	ERivermaxMediaSourcePixelFormat PixelFormat = ERivermaxMediaSourcePixelFormat::RGB_10bit;

	/** 
	 * Network card interface to use to receive data
	 * Wildcards are supported to match against an interface found on the machine
	 * 192.*.0.110
	 * 192.168.0.1?0
	 * 192.168.0.1*
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	FString InterfaceAddress = TEXT("*.*.*.*");

	/** IP address where incoming stream is coming from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	FString StreamAddress = UE::RivermaxCore::DefaultStreamAddress;

	/** Port used by the sender to send its stream */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	int32 Port = 50000;

	/** Whether the video input is in sRGB color space.If true, sRGBToLinear will be done on incoming pixels before writing to media texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
	bool bIsSRGBInput = false;

	/** Whether to use GPUDirect if available (Memcopy from NIC to GPU directly bypassing system memory) if available */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Video")
	bool bUseGPUDirect = true;

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
