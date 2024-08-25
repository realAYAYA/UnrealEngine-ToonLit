// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Device/Mem/DeviceBuffer_Mem.h"
#include "Data/RawBuffer.h"

class Device;
class Device_MemCM;

//////////////////////////////////////////////////////////////////////////
/// This is simply a wrapper around RawBuffer. Must be owned by the 
/// CPU device (though that can change in the future)
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API DeviceBuffer_MemCM : public DeviceBuffer_Mem
{
protected:
	virtual DeviceBuffer*			CreateFromRaw(RawBufferPtr InRawObj) override;
	virtual DeviceBuffer*			CreateFromDesc(BufferDescriptor InDesc, CHashPtr InHashValue) override;

	virtual AsyncBufferResultPtr	TransferFrom(DeviceBufferRef& Source) override;
public:
									DeviceBuffer_MemCM(Device_MemCM* Dev, BufferDescriptor InDesc, CHashPtr InHashValue);
									DeviceBuffer_MemCM(Device_MemCM* Dev, RawBufferPtr InRawObj);
	virtual							~DeviceBuffer_MemCM() override;

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
