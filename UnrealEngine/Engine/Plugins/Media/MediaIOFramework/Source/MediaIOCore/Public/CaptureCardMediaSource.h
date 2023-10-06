// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TimeSynchronizableMediaSource.h"
#include "Containers/UnrealString.h"
#include "Engine/TextureDefines.h"
#include "HAL/Platform.h"
#include "MediaIOCoreDefinitions.h"
#include "MediaIOCoreDeinterlacer.h"
#include "OpenColorIOColorSpace.h"
#include "OpenColorIORendering.h"
#include "UObject/NameTypes.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"


#include "CaptureCardMediaSource.generated.h"

namespace UE::CaptureCardMediaSource
{
	static FName RenderJIT("RenderJIT");
	static FName Framelock("Framelock");
	static FName EvaluationType("EvaluationType");
	static FName Deinterlacer("Deinterlacer");
	static FName InterlaceFieldOrder("InterlaceFieldOrder");
	static FName OverrideSourceEncoding("OverrideSourceEncoding");
	static FName SourceEncoding("SourceEncoding");
	static FName OverrideSourceColorSpace("OverrideSourceColorSpace");
	static FName SourceColorSpace("SourceColorcSpace");
	static FName OpenColorIOSettings("OpenColorIOSettings");
}


/** List of texture source encodings that can be converted to linear. (Integer values match the ETextureSourceEncoding values in TextureDefines.h */
UENUM()
enum class EMediaIOCoreSourceEncoding : uint8
{
	Linear		= 1 UMETA(DisplayName = "Linear"),
	sRGB		= 2 UMETA(DisplayName = "sRGB"),
	ST2084		= 3 UMETA(DisplayName = "ST 2084/PQ"),
	//BT1886		= 5 UMETA(DisplayName = "BT1886/Gamma 2.4"),
	SLog3		= 12 UMETA(DisplayName = "SLog3"),
	MAX,
};

/**
 * Base class for media sources that are coming from a capture card.
 */
UCLASS(Abstract)
class MEDIAIOCORE_API UCaptureCardMediaSource : public UTimeSynchronizableMediaSource
{
	GENERATED_BODY()
	
public:

	UCaptureCardMediaSource();

public:
	/** Should use JITR technique? It enables late sample picking for render. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Synchronization, meta = (DisplayName = "Just-In-Time Rendering"))
	bool bRenderJIT = true;

	/** Should wait for some time until requested frame arrives? Requires JIT rendering. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Synchronization, meta = (DisplayName = "Framelock"))
	bool bFramelock = false;

	/** Sample evaluation type. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Synchronization, meta = (DisplayName = "Sample Evaluation Type"))
	EMediaIOSampleEvaluationType EvaluationType = EMediaIOSampleEvaluationType::PlatformTime;

	/**
	 * How interlaced video should be treated.
	 */
	UPROPERTY(BlueprintReadOnly, Instanced, EditAnywhere, Category = "Video")
	TObjectPtr<UVideoDeinterlacer> Deinterlacer;

	/**
	 * The order in which interlace fields should be copied.
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Video")
	EMediaIOInterlaceFieldOrder InterlaceFieldOrder = EMediaIOInterlaceFieldOrder::TopFieldFirst;

	/** Whether to override the source encoding or to use the metadata embedded in the ancillary data of the signal. */
	UPROPERTY(EditAnywhere, Category = "Video", meta = (InlineEditConditionToggle))
	bool bOverrideSourceEncoding = true;

	/** Encoding of the source texture. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Video", meta = (EditCondition = "bOverrideSourceEncoding"))
	EMediaIOCoreSourceEncoding OverrideSourceEncoding = EMediaIOCoreSourceEncoding::Linear;

	/** Whether to override the source color space or to use the metadata embedded in the ancillary data of the signal. */
	UPROPERTY(EditAnywhere, Category = "Video", meta = (InlineEditConditionToggle))
	bool bOverrideSourceColorSpace = true;

	/** Color space of the source texture. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Video", meta = (EditCondition = "bOverrideSourceColorSpace"))
	ETextureColorSpace OverrideSourceColorSpace = ETextureColorSpace::TCS_sRGB;

	/**
	 * OCIO Settings used for applying a color conversion to the incoming source.
	 */
	UPROPERTY(BlueprintReadWrite ,EditAnywhere, Category="Video")
	FOpenColorIOColorConversionSettings ColorConversionSettings;

public:
	struct FOpenColorIODataContainer : public IMediaOptions::FDataContainer
	{
		FOpenColorIOColorConversionSettings ColorConversionSettings;
	};

public:
	//~ IMediaOptions interface
	using Super::GetMediaOption;
	virtual FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	virtual int64 GetMediaOption(const FName& Key, int64 DefaultValue) const override;
	virtual bool GetMediaOption(const FName& Key, bool bDefaultValue) const override;
	virtual TSharedPtr<FDataContainer, ESPMode::ThreadSafe> GetMediaOption(const FName& Key, const TSharedPtr<FDataContainer, ESPMode::ThreadSafe>& DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;

public:
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
};
