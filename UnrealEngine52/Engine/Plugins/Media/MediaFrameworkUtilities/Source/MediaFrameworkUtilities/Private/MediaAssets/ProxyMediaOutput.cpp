// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaAssets/ProxyMediaOutput.h"
#include "MediaFrameworkUtilitiesModule.h"


/* UProxyMediaOutput structors
 *****************************************************************************/

 
UProxyMediaOutput::UProxyMediaOutput()
	: bLeafMediaOutput(false)
	, bValidateGuard(false)
	, bRequestedSizeGuard(false)
	, bRequestedPixelFormatGuard(false)
	, bCreateMediaCaptureImplGuard(false)
{}


/* UMediaOutput interface
 *****************************************************************************/

 
bool UProxyMediaOutput::Validate(FString& OutFailureReason) const
{
	// Guard against reentrant calls.
	if (bValidateGuard)
	{
		UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("UProxyMediaOutput::Validate - Reentrant calls are not supported. Asset: %s"), *GetPathName());
		OutFailureReason = TEXT("Reentrant calls");
		return false;
	}
	TGuardValue<bool> ValidatingGuard(bValidateGuard, true);

	UMediaOutput* CurrentProxy = GetMediaOutput();
	if (CurrentProxy != nullptr)
	{
		return CurrentProxy->Validate(OutFailureReason);
	}
	
	OutFailureReason = FString::Printf(TEXT("There is no proxy for MediaOutput '%s'."), *GetName());
	return false;
}


FIntPoint UProxyMediaOutput::GetRequestedSize() const
{
	// Guard against reentrant calls.
	if (bRequestedSizeGuard)
	{
		UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("UProxyMediaOutput::GetRequestedSize - Reentrant calls are not supported. Asset: %s"), *GetPathName());
		return FIntPoint::ZeroValue;
	}
	TGuardValue<bool> GettingUrlGuard(bRequestedSizeGuard, true);

	UMediaOutput* CurrentProxy = GetMediaOutput();
	return (CurrentProxy != nullptr) ? CurrentProxy->GetRequestedSize() : FIntPoint::ZeroValue;
}


EPixelFormat UProxyMediaOutput::GetRequestedPixelFormat() const
{
	// Guard against reentrant calls.
	if (bRequestedPixelFormatGuard)
	{
		UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("UProxyMediaOutput::GetRequestedPixelFormat - Reentrant calls are not supported. Asset: %s"), *GetPathName());
		return EPixelFormat::PF_Unknown;
	}
	TGuardValue<bool> GettingUrlGuard(bRequestedPixelFormatGuard, true);

	UMediaOutput* CurrentProxy = GetMediaOutput();
	return (CurrentProxy != nullptr) ? CurrentProxy->GetRequestedPixelFormat() : EPixelFormat::PF_Unknown;
}


EMediaCaptureConversionOperation UProxyMediaOutput::GetConversionOperation(EMediaCaptureSourceType InSourceType) const
{
	// Guard against reentrant calls.
	if (bRequestedPixelFormatGuard)
	{
		UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("UProxyMediaOutput::GetConversionOperation - Reentrant calls are not supported. Asset: %s"), *GetPathName());
		return EMediaCaptureConversionOperation::NONE;
	}
	TGuardValue<bool> GettingUrlGuard(bRequestedPixelFormatGuard, true);

	UMediaOutput* CurrentProxy = GetMediaOutput();
	return (CurrentProxy != nullptr) ? CurrentProxy->GetConversionOperation(InSourceType) : EMediaCaptureConversionOperation::NONE;
}


UMediaCapture* UProxyMediaOutput::CreateMediaCaptureImpl()
{
	// Guard against reentrant calls.
	if (bCreateMediaCaptureImplGuard)
	{
		UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("UProxyMediaOutput::CreateMediaCaptureImpl - Reentrant calls are not supported. Asset: %s"), *GetPathName());
		return nullptr;
	}
	TGuardValue<bool> GettingUrlGuard(bCreateMediaCaptureImplGuard, true);

	UMediaOutput* CurrentProxy = GetMediaOutput();
	return (CurrentProxy != nullptr) ? CurrentProxy->CreateMediaCapture() : nullptr;
}


/* UProxyMediaOutput implementation
 *****************************************************************************/

UMediaOutput* UProxyMediaOutput::GetMediaOutput() const
{
	return DynamicProxy ? DynamicProxy : Proxy;
}


UMediaOutput* UProxyMediaOutput::GetLeafMediaOutput() const
{
	// Guard against reentrant calls.
	if (bLeafMediaOutput)
	{
		UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("UMediaSourceProxy::GetLeafMediaOutput - Reentrant calls are not supported. Asset: %s"), *GetPathName());
		return nullptr;
	}
	TGuardValue<bool> ValidatingGuard(bLeafMediaOutput, true);

	UMediaOutput* MediaOutput = GetMediaOutput();
	if (UProxyMediaOutput* ProxyMediaOutput = Cast<UProxyMediaOutput>(MediaOutput))
	{
		MediaOutput = ProxyMediaOutput->GetLeafMediaOutput();
	}
	return MediaOutput;
}


bool UProxyMediaOutput::IsProxyValid() const
{
	return GetLeafMediaOutput() != nullptr;
}


void UProxyMediaOutput::SetDynamicMediaOutput(UMediaOutput* InProxy)
{
	DynamicProxy = InProxy;
}


#if WITH_EDITOR
void UProxyMediaOutput::SetMediaOutput(UMediaOutput* InProxy)
{
	Proxy = InProxy;
}
#endif