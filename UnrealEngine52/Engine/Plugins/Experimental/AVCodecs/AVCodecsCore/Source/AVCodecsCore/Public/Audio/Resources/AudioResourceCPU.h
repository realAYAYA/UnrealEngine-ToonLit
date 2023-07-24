// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVContext.h"
#include "Audio/AudioResource.h"

/**
 * CPU audio context and resource.
 */

class AVCODECSCORE_API FAudioContextCPU : public FAVContext
{
public:
	FAudioContextCPU();
};

class AVCODECSCORE_API FAudioResourceCPU : public TAudioResource<FAudioContextCPU>
{
private:
	TSharedPtr<float> Raw;

public:
	FORCEINLINE TSharedPtr<float> const& GetRaw() const { return Raw; }

	FAudioResourceCPU(TSharedRef<FAVDevice> const& Device, TSharedPtr<float> const& Raw, FAVLayout const& Layout, FAudioDescriptor const& Descriptor);
	virtual ~FAudioResourceCPU() override = default;

	virtual FAVResult Validate() const override;
};

DECLARE_TYPEID(FAudioContextCPU, AVCODECSCORE_API);
DECLARE_TYPEID(FAudioResourceCPU, AVCODECSCORE_API);
