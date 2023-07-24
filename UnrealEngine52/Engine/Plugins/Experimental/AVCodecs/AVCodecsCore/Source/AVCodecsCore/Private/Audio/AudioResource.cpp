// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/AudioResource.h"

FAudioResource::FAudioResource(TSharedRef<FAVDevice> const& Device, FAVLayout const& Layout, FAudioDescriptor const& Descriptor)
	: FAVResource(Device, Layout)
	, Descriptor(Descriptor)
{
}
