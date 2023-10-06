// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVContext.h"
#include "Video/VideoResource.h"

/**
 * CPU video context and resource.
 */

class AVCODECSCORE_API FVideoContextCPU : public FAVContext
{
public:
	FVideoContextCPU();
};

class AVCODECSCORE_API FVideoResourceCPU : public TVideoResource<FVideoContextCPU>
{
private:
	TSharedPtr<uint8> Raw;

public:
	FORCEINLINE TSharedPtr<uint8> const& GetRaw() const { return Raw; }

	FVideoResourceCPU(TSharedRef<FAVDevice> const& Device, TSharedPtr<uint8> const& Raw, FAVLayout const& Layout, FVideoDescriptor const& Descriptor);
	virtual ~FVideoResourceCPU() override = default;

	virtual FAVResult Validate() const override;
};

DECLARE_TYPEID(FVideoContextCPU, AVCODECSCORE_API);
DECLARE_TYPEID(FVideoResourceCPU, AVCODECSCORE_API);
