// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Device/DeviceBuffer.h"

class Device_FX;
class DeviceBuffer_Mem;

class Tex;
typedef std::shared_ptr<Tex>		TexPtr;

class TEXTUREGRAPHENGINE_API DeviceBuffer_FX : public DeviceBuffer
{
	friend class Device_FX;
protected: 
	TexPtr							Texture;				/// The underlying texture object

	AsyncBufferResultPtr			AllocateTexture();
	AsyncBufferResultPtr			AllocateRenderTarget();
	void							UpdateRenderTarget(TexPtr TextureObj);

	virtual DeviceBuffer*			CreateFromRaw(RawBufferPtr RawObj) override;
	virtual DeviceBuffer*			CreateFromDesc(BufferDescriptor NewDesc, CHashPtr NewHashValue) override;
	virtual DeviceBuffer*			CopyFrom(DeviceBuffer* RHS) override;
	virtual AsyncBufferResultPtr	UpdateRaw(RawBufferPtr RawObj) override;

	virtual CHashPtr				CalcHash() override;

	virtual AsyncBufferResultPtr	TransferFrom(DeviceBufferRef& Source) override;

public:
									DeviceBuffer_FX(Device_FX* Dev, BufferDescriptor Desc, CHashPtr NewHashValue);
									DeviceBuffer_FX(Device_FX* Dev, TexPtr TextureObj, RawBufferPtr RawObj);
									DeviceBuffer_FX(Device_FX* Dev, TexPtr TextureObj, BufferDescriptor Desc, CHashPtr NewHashValue);
									DeviceBuffer_FX(Device_FX* Dev, RawBufferPtr RawObj);
	virtual							~DeviceBuffer_FX() override;

	//////////////////////////////////////////////////////////////////////////
	/// DeviceBuffer Implementation
	//////////////////////////////////////////////////////////////////////////
	virtual RawBufferPtr			Raw_Now() override;
	virtual size_t					MemSize() const override;
	virtual size_t					DeviceNative_MemSize() const override;
	virtual AsyncBufferResultPtr	Bind(const BlobTransform* Transform, const ResourceBindInfo& BindInfo) override;
	virtual AsyncBufferResultPtr	Unbind(const BlobTransform* Transform, const ResourceBindInfo& BindInfo) override;
	virtual AsyncBufferResultPtr	Flush(const ResourceBindInfo& BindInfo) override;
	virtual AsyncPrepareResult		PrepareForWrite(const ResourceBindInfo& BindInfo) override;
	virtual bool					IsNull() const override;

	virtual void					ReleaseNative() override;

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE TexPtr				GetTexture() { return Texture; }
};

typedef std::shared_ptr<DeviceBuffer_FX> DeviceBuffer_FXPtr;
