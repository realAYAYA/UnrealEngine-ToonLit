// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Device/DeviceBuffer.h"

class Device_Null;

class TEXTUREGRAPHENGINE_API DeviceBuffer_Null : public DeviceBuffer
{
protected:
	virtual DeviceBuffer*			CreateFromRaw(RawBufferPtr rawBuffer) override;
	virtual DeviceBuffer*			CreateFromDesc(BufferDescriptor desc, CHashPtr hash) override;

	virtual AsyncBufferResultPtr	TransferFrom(DeviceBufferRef& source);

public:
									DeviceBuffer_Null(Device_Null* device, BufferDescriptor desc, CHashPtr hash);
	virtual							~DeviceBuffer_Null();

	virtual RawBufferPtr			Raw_Now();
	virtual size_t					MemSize() const;
	virtual AsyncBufferResultPtr	Bind(const BlobTransform* transform, const ResourceBindInfo& bindInfo) override;
	virtual bool					IsValid() const;
	virtual bool					IsNull() const;
	virtual bool					IsTransient() const override;
};
