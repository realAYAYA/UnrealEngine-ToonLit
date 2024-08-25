// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Device/DeviceBuffer.h"
#include "Data/RawBuffer.h"

class Device;
class Device_Disk;

//////////////////////////////////////////////////////////////////////////
/// This is simply a wrapper around RawBuffer. Must be owned by the 
/// CPU device (though that can change in the future)
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API DeviceBuffer_Disk : public DeviceBuffer
{
protected:
	RawBufferPtr					RawObj;				/// The underlying raw buffer

	virtual DeviceBuffer*			CreateFromRaw(RawBufferPtr NewRawObj) override;
	virtual DeviceBuffer*			CreateFromDesc(BufferDescriptor NewDesc, CHashPtr NewHashValue) override;

	virtual AsyncBufferResultPtr	TransferFrom(DeviceBufferRef& Source) override;

public:
									DeviceBuffer_Disk(Device_Disk* Dev, BufferDescriptor Desc, CHashPtr HashValue);
									DeviceBuffer_Disk(Device_Disk* Dev, RawBufferPtr RawObj);
	virtual							~DeviceBuffer_Disk() override;

	//////////////////////////////////////////////////////////////////////////
	/// DeviceBuffer implementation
	//////////////////////////////////////////////////////////////////////////
	virtual RawBufferPtr			Raw_Now() override;
	virtual size_t					MemSize() const override;
	virtual AsyncBufferResultPtr	Bind(const BlobTransform* Transform, const ResourceBindInfo& BindInfo) override;

	/// With base implementations
	//virtual void					Release() override;
	virtual bool					IsCompatible(Device* Dev) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
};
