// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Device/DeviceBuffer.h"
#include "Data/RawBuffer.h"

class Device;
class Device_Mem;

//////////////////////////////////////////////////////////////////////////
/// This is simply a wrapper around RawBuffer. Must be owned by the 
/// CPU device (though that can change in the future)
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API DeviceBuffer_Mem : public DeviceBuffer
{
protected:
	virtual DeviceBuffer*			CreateFromRaw(RawBufferPtr RawObj) override;
	virtual DeviceBuffer*			CreateFromDesc(BufferDescriptor NewDesc, CHashPtr NewHashValue) override;

	void							Allocate();
	virtual AsyncBufferResultPtr	TransferFrom(DeviceBufferRef& Source) override;

public:
									DeviceBuffer_Mem(Device_Mem* Device, BufferDescriptor Desc, CHashPtr HashValue);
									DeviceBuffer_Mem(Device_Mem* Device, RawBufferPtr RawObj);
	virtual							~DeviceBuffer_Mem() override;

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
