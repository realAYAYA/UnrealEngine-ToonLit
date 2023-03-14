// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaOutput.h"

#include "MediaOutput.h"
#include "RivermaxMediaCapture.h"


/* URivermaxMediaOutput
*****************************************************************************/

bool URivermaxMediaOutput::Validate(FString& OutFailureReason) const
{
	if (!Super::Validate(OutFailureReason))
	{
		return false;
	}
	
	return true;
}

FIntPoint URivermaxMediaOutput::GetRequestedSize() const
{
	return Resolution;
}

EPixelFormat URivermaxMediaOutput::GetRequestedPixelFormat() const
{
	// All output types go through buffer conversion
	EPixelFormat Result = EPixelFormat::PF_A2B10G10R10;
	return Result;
}

EMediaCaptureConversionOperation URivermaxMediaOutput::GetConversionOperation(EMediaCaptureSourceType InSourceType) const
{
	EMediaCaptureConversionOperation Result = EMediaCaptureConversionOperation::CUSTOM;
	switch (PixelFormat)
	{
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_YUV422:
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_YUV422:
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_12BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_FLOAT16_RGB:
	default:
		Result = EMediaCaptureConversionOperation::CUSTOM; //We handle all conversion for rivermax since it's really tied to endianness of 2110
		break;
	}
	return Result;
}

UMediaCapture* URivermaxMediaOutput::CreateMediaCaptureImpl()
{
	UMediaCapture* Result = NewObject<URivermaxMediaCapture>();
	if (Result)
	{
		Result->SetMediaOutput(this);
	}
	return Result;
}

#if WITH_EDITOR
bool URivermaxMediaOutput::CanEditChange(const FProperty* InProperty) const
{
	return Super::CanEditChange(InProperty);
}

void URivermaxMediaOutput::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif //WITH_EDITOR

