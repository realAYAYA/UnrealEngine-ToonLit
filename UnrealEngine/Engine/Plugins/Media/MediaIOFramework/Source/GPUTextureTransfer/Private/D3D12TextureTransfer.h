// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextureTransferBase.h"

#if DVP_SUPPORTED_PLATFORM

struct ID3D12CommandQueue;
struct ID3D12Device;

namespace UE::GPUTextureTransfer::Private
{
class FD3D12TextureTransfer : public FTextureTransferBase
{
protected:
	virtual DVPStatus Init_Impl(const FInitializeDMAArgs& InArgs) override;
	virtual DVPStatus GetConstants_Impl(uint32* OutBufferAddrAlignment, uint32* OutBufferGPUStrideAlignment, uint32* OutSemaphoreAddrAlignment, uint32* OutSemaphoreAllocSize, uint32* OutSemaphorePayloadOffset, uint32* OutSemaphorePayloadSize) const override;
	virtual DVPStatus BindBuffer_Impl(DVPBufferHandle InBufferHandle) const override;
	virtual DVPStatus CreateGPUResource_Impl(const FRegisterDMATextureArgs& InArgs, FTextureInfo* OutTextureInfo) const override;
	virtual DVPStatus UnbindBuffer_Impl(DVPBufferHandle InBufferHandle) const override;
	virtual DVPStatus CloseDevice_Impl() const override;
	virtual DVPStatus MapBufferWaitAPI_Impl(DVPBufferHandle InHandle) const;
	virtual DVPStatus MapBufferEndAPI_Impl(DVPBufferHandle InHandle) const;


private:
	ID3D12Device* D3DDevice = nullptr;
	ID3D12CommandQueue* D3DCommandQueue = nullptr;
};
}

#endif // DVP_SUPPORTED_PLATFORM