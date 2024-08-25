// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "ElectraPlayerPrivate.h"
#include "MediaVideoDecoderOutputPC.h"
#include "VideoDecoderResourceDelegate.h"

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

#include "Windows/AllowWindowsPlatformTypes.h"
#include "HAL/LowLevelMemTracker.h"

THIRD_PARTY_INCLUDES_START
#include "mftransform.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
THIRD_PARTY_INCLUDES_END

#if defined(NTDDI_WIN10_NI)
#include <mfd3d12.h>
#define HAVE_MFSAMPLE_WITH_DX12 1
#else
#define HAVE_MFSAMPLE_WITH_DX12 0
#endif

#include "Windows/HideWindowsPlatformTypes.h"

struct ID3D12Resource;
struct ID3D12Fence;

class FElectraMediaDecoderOutputBufferPool_DX12;
class FElectraDecoderOutputSync;

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

class FElectraPlayerVideoDecoderOutputPC : public FVideoDecoderOutputPC
{
public:
	FElectraPlayerVideoDecoderOutputPC() : SampleDim(0, 0)
	{ }

	// Hardware decode to buffer (Win7/DX12)
	void InitializeWithBuffer(const void* InBuffer, uint32 InSize, uint32 InStride, FIntPoint Dim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict);

	void InitializeWithBuffer(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> InBuffer, uint32 InStride, FIntPoint Dim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict);

	void InitializeWithResource(const TRefCountPtr<ID3D12Device>& InD3D12Device, const TRefCountPtr<ID3D12Resource> Resource, uint32 ResourcePitch, const FElectraDecoderOutputSync& OutputSync, const FIntPoint& OutputDim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict, TWeakPtr<Electra::IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> ResourceDelegate,
								TSharedPtr<FElectraMediaDecoderOutputBufferPool_DX12>& InOutD3D12ResourcePool, uint32 MaxWidth, uint32 MaxHeight, uint32 MaxOutputBuffers);

	// Hardware decode to shared DX11 texture (Win8+) from IMFSample
	void InitializeWithSharedTexture(const TRefCountPtr<ID3D11Device>& InD3D11Device, const TRefCountPtr<ID3D11DeviceContext> InDeviceContext, const TRefCountPtr<IMFSample> MFSample, const FIntPoint& OutputDim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict);

	// Hardware decode to shared DX11 texture (Win8+) from DX11 texture
	void InitializeWithSharedTexture(const TRefCountPtr<ID3D11Device>& InD3D11Device, const TRefCountPtr<ID3D11DeviceContext> InDeviceContext, const TRefCountPtr<ID3D11Texture2D> DecoderTexture, uint32 ViewIndex, const FIntPoint& OutputDim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict);

	void SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& InOwningRenderer) override;
	void ShutdownPoolable() override;
	bool IsReadyForReuse() override;
	EOutputType GetOutputType() const override;
	const TArray<uint8>& GetBuffer() const override;
	uint32 GetStride() const override;
	TRefCountPtr<IUnknown> GetTexture() const override;
	TRefCountPtr<ID3D11Device> GetDevice() const override;
	FIntPoint GetDim() const override;
	TRefCountPtr<IUnknown> GetSync(uint64& SyncValue) const override;

private:
	static void TriggerDataCopy(TRefCountPtr<ID3D12GraphicsCommandList> D3DCmdList, TRefCountPtr<ID3D12Fence> D3DFence, uint64 FenceValue, const FElectraDecoderOutputSync& OutputSync, Electra::IVideoDecoderResourceDelegate* InResourceDelegate);

	// Decoder output type
	EOutputType OutputType = EOutputType::Unknown;

	// Output texture (with device that created it) for SW decoder output Win8+
	TRefCountPtr<ID3D11Texture2D> Texture;
	TRefCountPtr<ID3D11Texture2D> SharedTexture;
	TRefCountPtr<ID3D11Device> D3D11Device;

	TRefCountPtr<ID3D12Resource> TextureDX12;
	FIntPoint TextureDX12Dim = {0, 0};
	TRefCountPtr<ID3D12Fence> D3DFence;
	uint64 FenceValue = 0;

	TRefCountPtr<ID3D12CommandAllocator> D3DCmdAllocator;
	TRefCountPtr<ID3D12GraphicsCommandList> D3DCmdList;
	TSharedPtr<FElectraMediaDecoderOutputBufferPool_DX12> D3D12ResourcePool;

	TRefCountPtr<ID3D12Resource> DecoderOutputResource;

	// CPU-side buffer
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> Buffer;
	uint32 Stride = 0;

	// Dimension of any internally allocated buffer - stored explicitly to cover various special cases for DX
	FIntPoint SampleDim;

	// We hold a weak reference to the video renderer. During destruction the video renderer could be destroyed while samples are still out there..
	TWeakPtr<IDecoderOutputOwner, ESPMode::ThreadSafe> OwningRenderer;
};
