// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Containers/Array.h"

#define __LLP64__ 1

#include "ProResProperties.h"
#include "ProResEncoder.h"
#include "ProResFileWriter.h"

#include "Protocols/FrameGrabberProtocol.h"

#include "UObject/StrongObjectPtr.h"

#include "AppleProResEncoderProtocol.generated.h"

UENUM()
enum class EAppleProResEncoderFormats : uint8
{
	F_422HQ UMETA(DisplayName = "422 HQ"),
	F_422 UMETA(DisplayName = "422"),
	F_422LT UMETA(DisplayName = "422 LT"),
	F_422Proxy UMETA(DisplayName = "422 Proxy"),
	F_4444 UMETA(DisplayName = "4444"),
	F_4444XQ UMETA(DisplayName = "4444 XQ"),
};

UENUM()
enum class EAppleProResEncoderColorDescription : uint8
{
	CD_SDREC601_525_60HZ UMETA(DisplayName = "SD Rec. 601 525/60Hz"),
	CD_SDREC601_625_50HZ UMETA(DisplayName = "SD Rec. 601 625/50Hz"),
	CD_HDREC709 UMETA(DisplayName = "HD Rec. 709"),
};

UENUM()
enum class EAppleProResEncoderScanType : uint8
{
	IM_PROGRESSIVE_SCAN UMETA(DisplayName = "Progressive encoding mode"),
	IM_INTERLACED_TOP_FIELD_FIRST UMETA(DisplayName = "Interlaced mode; first (top) image line belongs to first temporal field"),
	IM_INTERLATED_BOTTOM_FIRST_FIRST UMETA(DisplayName = "Interlaced mode; second (bottom) image line belongs to first temporal field"),
};

UCLASS(meta = (DisplayName = "Apple ProRes Encoder (mov)", CommandLineID = "AppleProRes") )
class APPLEPRORESMEDIA_API UAppleProResEncoderProtocol : public UFrameGrabberProtocol
{
public:
	GENERATED_BODY()

	UAppleProResEncoderProtocol(const FObjectInitializer& InObjInit);

public:
	virtual bool SetupImpl() override;
	virtual void FinalizeImpl() override;
	virtual FFramePayloadPtr GetFramePayload(const FFrameMetrics& InFrameMetrics) override;
	virtual void ProcessFrame(FCapturedFrameData InFrame) override;
	virtual bool CanWriteToFileImpl(const TCHAR* InFilename, bool bInOverwriteExisting) const override;

public:

	UPROPERTY(config, EditAnywhere, Category = "Apple ProRes Settings")
	EAppleProResEncoderFormats EncodingFormat;

	UPROPERTY(config, EditAnywhere, Category = "Apple ProRes Settings")
	EAppleProResEncoderColorDescription ColorDescription;

	UPROPERTY(config, EditAnywhere, Category = "Apple ProRes Settings")
	EAppleProResEncoderScanType ScanType;

	UPROPERTY(config, EditAnywhere, Category = "Apple ProRes Settings", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "64.0", UIMax = "64.0"))
	int32 NumberOfEncodingThreads;

	UPROPERTY(config, EditAnywhere, Category = "Apple ProRes Settings")
	bool bEmbedTimecodeTrack;

	/** Use Drop Frame Timecode when applicable (29.97p or 59.94i). */
	UPROPERTY(config, EditAnywhere, Category = "Apple ProRes Settings")
	bool bDropFrameTimecode;

private:

	void ParseCommandLine();
	bool CreateProResFile(const FString& InFilename);
	void ConvertFColorToRGBA4444(const TArray<FColor>& InColorbuffer);

	PRCodecType GetSelectedCodecType() const;
	PRVideoCodecType GetSelectedVideoCodecType() const;
	ProResFormatDescriptionColorPrimaries GetColorPrimaries() const;
	ProResFormatDescriptionTransferFunction GetTransferFunction() const;
	ProResFormatDescriptionYCbCrMatrix GetYCbCrMatrix() const;
	int32_t GetFrameHeaderColorPrimaries() const;
	int32_t GetFrameHeaderTransferCharacteristic() const;
	int32_t GetFrameHeaderMatrixCoefficients() const;
	PRInterlaceMode GetInterlaceMode() const;
	PRPixelFormat GetPixelFormat() const;
	uint32 GetPixelAspectRatioHorizontalSpacing() const;
	uint32 GetPixelAspectRatioVerticalSpacing() const;
	uint32 GetStride() const;
	uint32_t GetBitDepth() const;
	uint32_t GetFieldCount() const;
	ProResFormatDescriptionFieldDetail GetFieldDetail() const;
	bool HasAlpha() const;

private:

	ProResFileWriterRef FileWriter;
	PRPersistentTrackID VideoTrackID;
	PRPersistentTrackID TimecodeTrackID;
	PRTime CurrentTime;
	ProResFormatDescriptionRef FormatDescription;
	ProResTimecodeFormatDescriptionRef TimecodeFormatDescription;
	PREncoderRef Encoder;
	PRVideoDimensions Dimensions;

	FFrameRate FrameRate;
	int32 MaxCompressedFrameSize;
	int32 TargetCompressedFrameSize;
	TArray<uint8> IntermediateFrameBuffer;
	TArray<uint8> OutputFrameBuffer;
};
