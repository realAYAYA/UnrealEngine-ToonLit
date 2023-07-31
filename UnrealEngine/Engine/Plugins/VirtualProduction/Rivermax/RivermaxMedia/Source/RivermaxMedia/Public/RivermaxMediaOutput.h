// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaOutput.h"

#include "MediaIOCoreDefinitions.h"
#include "UObject/ObjectMacros.h"

#include "RivermaxMediaOutput.generated.h"


UENUM()
enum class ERivermaxMediaOutputPixelFormat : uint8
{
	PF_8BIT_YUV422 UMETA(DisplayName = "8bit YUV422"),
	PF_10BIT_YUV422 UMETA(DisplayName = "10bit YUV422"),
	PF_8BIT_RGB UMETA(DisplayName = "8bit RGB"),
	PF_10BIT_RGB UMETA(DisplayName = "10bit RGB"),
	PF_12BIT_RGB UMETA(DisplayName = "12bit RGB"),
	PF_FLOAT16_RGB UMETA(DisplayName = "16bit Float RGB")
};


/**
 * Output information for a Rivermax media capture.
 */
UCLASS(BlueprintType, meta=(MediaIOCustomLayout="Rivermax"))
class RIVERMAXMEDIA_API URivermaxMediaOutput : public UMediaOutput
{
	GENERATED_BODY()

public:
	
	//~ Begin UMediaOutput interface
	virtual bool Validate(FString& FailureReason) const override;
	virtual FIntPoint GetRequestedSize() const override;
	virtual EPixelFormat GetRequestedPixelFormat() const override;
	virtual EMediaCaptureConversionOperation GetConversionOperation(EMediaCaptureSourceType InSourceType) const override;

protected:
	virtual UMediaCapture* CreateMediaCaptureImpl() override;
	//~ End UMediaOutput interface


public:
	//~ UObject interface
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
	//~ End UObject interface

	/** Resolution of this output stream */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Format")
	FIntPoint Resolution = {1920, 1080};
	
	/** Frame rate of this output stream */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Format")
	FFrameRate FrameRate = {24,1};
	
	/** Pixel format for this output stream */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Format")
	ERivermaxMediaOutputPixelFormat PixelFormat = ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB;

	/** Address of NIC interface to use. Supports wildcard, i.e. 10.*.69.*. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Format")
	FString InterfaceAddress;

	/** Address of the stream. Can be multicast, i.e. 224.1.1.1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Format")
	FString StreamAddress;

	/** Port to use for this output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Format")
	int32 Port = 50000;

	/** Whether to use GPUDirect if available (Memcopy from GPU to NIC directly bypassing system) if available */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Output")
	bool bUseGPUDirect = false;
};
