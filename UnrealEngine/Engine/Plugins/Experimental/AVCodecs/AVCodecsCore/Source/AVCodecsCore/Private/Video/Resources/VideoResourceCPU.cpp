// Copyright Epic Games, Inc. All Rights Reserved.

#include "Video/Resources/VideoResourceCPU.h"

REGISTER_TYPEID(FVideoContextCPU);
REGISTER_TYPEID(FVideoResourceCPU);

FVideoContextCPU::FVideoContextCPU()
{
}

FVideoResourceCPU::FVideoResourceCPU(TSharedRef<FAVDevice> const& Device, TSharedPtr<uint8> const& Raw, FAVLayout const& Layout, FVideoDescriptor const& Descriptor)
	: TVideoResource(Device, Layout, Descriptor)
	, Raw(Raw)
{
}

FAVResult FVideoResourceCPU::Validate() const
{
	if (!Raw.IsValid())
	{
		return FAVResult(EAVResult::ErrorInvalidState, TEXT("Raw resource is invalid"), TEXT("CPU"));
	}

	return EAVResult::Success;
}
