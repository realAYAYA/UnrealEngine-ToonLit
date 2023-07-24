// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/Resources/AudioResourceCPU.h"

REGISTER_TYPEID(FAudioContextCPU);
REGISTER_TYPEID(FAudioResourceCPU);

FAudioContextCPU::FAudioContextCPU()
{
}

FAudioResourceCPU::FAudioResourceCPU(TSharedRef<FAVDevice> const& Device, TSharedPtr<float> const& Raw, FAVLayout const& Layout, FAudioDescriptor const& Descriptor)
	: TAudioResource(Device, Layout, Descriptor)
	, Raw(Raw)
{
}

FAVResult FAudioResourceCPU::Validate() const
{
	if (!Raw.IsValid())
	{
		return FAVResult(EAVResult::ErrorInvalidState, TEXT("Raw resource is invalid"), TEXT("CPU"));
	}

	return EAVResult::Success;
}
