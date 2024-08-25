// Copyright Epic Games, Inc. All Rights Reserved.

#if AVCODECS_USE_METAL

#pragma once

#include "Templates/RefCounting.h"

#include "AVContext.h"
#include "Video/VideoResource.h"
#include "Containers/ResourceArray.h"

THIRD_PARTY_INCLUDES_START
#include "MetalInclude.h"
THIRD_PARTY_INCLUDES_END


/**
 * Metal platform video context and resource.
 */

class AVCODECSCORE_API FVideoContextMetal : public FAVContext
{
public:
	MTL::Device* Device;

	FVideoContextMetal(MTL::Device* Device);
};


class AVCODECSCORE_API FVideoResourceMetal : public TVideoResource<FVideoContextMetal>
{
private:
    CVPixelBufferRef Raw;

public:
	static FVideoDescriptor GetDescriptorFrom(TSharedRef<FAVDevice> const& Device, CVPixelBufferRef Raw);

	FORCEINLINE CVPixelBufferRef GetRaw() const { return Raw; }

	FVideoResourceMetal(TSharedRef<FAVDevice> const& Device, CVPixelBufferRef Raw, FAVLayout const& Layout);
    virtual ~FVideoResourceMetal() override;
    
	virtual FAVResult Validate() const override;
};

/**
 * Passes a CVPixelBufferRef through to the RHI to wrap in an RHI texture without traversing system memory.
 */
class AVCODECSCORE_API FBulkDataMetal : public FResourceBulkDataInterface
{
public:
    FBulkDataMetal(CFTypeRef InImageBuffer)
        : ImageBuffer(InImageBuffer)
    {
        check(ImageBuffer);
        CFRetain(ImageBuffer);
    }
    virtual ~FBulkDataMetal()
    {
        CFRelease(ImageBuffer);
        ImageBuffer = nullptr;
    }
public:
    virtual void Discard() override
    {
        delete this;
    }
    virtual const void* GetResourceBulkData() const override
    {
        return ImageBuffer;
    }
    virtual uint32 GetResourceBulkDataSize() const override
    {
        return ImageBuffer ? ~0u : 0;
    }
    virtual EBulkDataType GetResourceType() const override
    {
        return EBulkDataType::MediaTexture;
    }
    CFTypeRef ImageBuffer;
};

DECLARE_TYPEID(FVideoContextMetal, AVCODECSCORE_API);
DECLARE_TYPEID(FVideoResourceMetal, AVCODECSCORE_API);

#endif
