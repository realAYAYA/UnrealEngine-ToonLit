// Copyright Epic Games, Inc. All Rights Reserved.

#include "Video/VideoResource.h"

FVideoResource::FVideoResource(TSharedRef<FAVDevice> const& Device, FAVLayout const& Layout, FVideoDescriptor const& Descriptor)
	: FAVResource(Device, Layout)
	, Descriptor(Descriptor)
{
}
