// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraVideoDecoder_PC.h"
#include "Renderer/RendererVideo.h"
#include COMPILED_PLATFORM_HEADER(ElectraDecoderGPUBufferHelpers.h)
#include COMPILED_PLATFORM_HEADER(ElectraDecoderPlatformOutputHandleTypes.h)
#include "WindowsElectraDecoderResourceManager.h"

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START

#include <d3d12.h>
#include "mftransform.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
#if defined(NTDDI_WIN10_NI)
#include "mfd3d12.h"
#define ALLOW_MFSAMPLE_WITH_DX12	1	// Windows SDK 22621 and up do feature APIs to support DX12 texture resources with WMF transforms
#else
#define ALLOW_MFSAMPLE_WITH_DX12	0
#endif

THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

// ------------------------------------------------------------------------------------

void FElectraPlayerVideoDecoderOutputPC::SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& InOwningRenderer)
{
	OwningRenderer = InOwningRenderer;
}

FElectraPlayerVideoDecoderOutputPC::EOutputType FElectraPlayerVideoDecoderOutputPC::GetOutputType() const
{
	return OutputType;
}

const TArray<uint8>& FElectraPlayerVideoDecoderOutputPC::GetBuffer() const
{
	if ((OutputType == EOutputType::SoftwareWin8Plus || OutputType == EOutputType::SoftwareWin7 || OutputType == EOutputType::HardwareDX9_DX12))
	{
		if (Buffer.IsValid())
		{
			return *Buffer;
		}
	}
	static TArray<uint8> Empty;
	return Empty;
}

uint32 FElectraPlayerVideoDecoderOutputPC::GetStride() const
{
	return Stride;
}

TRefCountPtr<IUnknown> FElectraPlayerVideoDecoderOutputPC::GetTexture() const
{
	return Texture ? static_cast<IUnknown*>(Texture.GetReference()) : static_cast<IUnknown*>(TextureDX12.GetReference());
}

TRefCountPtr<ID3D11Device> FElectraPlayerVideoDecoderOutputPC::GetDevice() const
{
	check(OutputType == EOutputType::SoftwareWin8Plus || OutputType == EOutputType::HardwareWin8Plus);
	return D3D11Device;
}

FIntPoint FElectraPlayerVideoDecoderOutputPC::GetDim() const
{
	return SampleDim;
}

TRefCountPtr<IUnknown> FElectraPlayerVideoDecoderOutputPC::GetSync(uint64& SyncValue) const
{
	if (Texture)
	{
		TRefCountPtr<IUnknown> KeyedMutex;
		HRESULT res = Texture->QueryInterface(_uuidof(IDXGIKeyedMutex), (void**)&KeyedMutex);
		SyncValue = 0;
		return KeyedMutex;
	}
	else if (TextureDX12)
	{
		SyncValue = FenceValue;
		return static_cast<IUnknown*>(D3DFence.GetReference());
	}
	return nullptr;
}

void FElectraPlayerVideoDecoderOutputPC::InitializeWithBuffer(const void* InBuffer, uint32 InSize, uint32 InStride, FIntPoint Dim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict)
{
	FVideoDecoderOutput::Initialize(MoveTemp(InParamDict));

	OutputType = EOutputType::HardwareDX9_DX12;

	Buffer = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
	Buffer->Append((uint8*)InBuffer, InSize);

	SampleDim = Dim;
	Stride = InStride;

	Texture = nullptr;
	SharedTexture = nullptr;
	TextureDX12 = nullptr;
	D3DFence = nullptr;
}

void FElectraPlayerVideoDecoderOutputPC::InitializeWithBuffer(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> InBuffer, uint32 InStride, FIntPoint Dim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict)
{
	FVideoDecoderOutput::Initialize(MoveTemp(InParamDict));

	OutputType = EOutputType::HardwareDX9_DX12;

	Buffer = MoveTemp(InBuffer);

	SampleDim = Dim;
	Stride = InStride;

	Texture = nullptr;
	SharedTexture = nullptr;
	TextureDX12 = nullptr;
	D3DFence = nullptr;
}

void FElectraPlayerVideoDecoderOutputPC::InitializeWithResource(const TRefCountPtr<ID3D12Device>& InD3D12Device, const TRefCountPtr<ID3D12Resource> InResource, uint32 ResourcePitch, const FElectraDecoderOutputSync& OutputSync, const FIntPoint& InOutputDim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict, TWeakPtr<Electra::IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate,
																TSharedPtr<FElectraMediaDecoderOutputBufferPool_DX12>& InOutD3D12ResourcePool, uint32 MaxWidth, uint32 MaxHeight, uint32 MaxOutputBuffers)
{
	// We must not allow re-initialization of used instances
	check(!D3DFence.IsValid());

	// General initialization
	FVideoDecoderOutput::Initialize(InParamDict);

	// Local reference to the resource passed in as we need to ensure it is alive until we have copied from it
	DecoderOutputResource = InResource;

	SampleDim = InOutputDim;
	OutputType = EOutputType::Hardware_DX;
	if (!D3D12ResourcePool.IsValid() && InOutD3D12ResourcePool.IsValid())
	{
		D3D12ResourcePool = InOutD3D12ResourcePool;
	}

	Buffer.Reset();

	HRESULT Res;

	// Create D3D12 resources as needed (we will reuse if possible as this instance is reused)
	if (!D3DCmdAllocator.IsValid())
	{
		Res = InD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(D3DCmdAllocator.GetInitReference()));
		check(!FAILED(Res));
#if !UE_BUILD_SHIPPING
		D3DCmdAllocator->SetName(TEXT("ElectraVideoDecoderCmdAllocator"));
#endif
	}
	if (!D3DCmdList.IsValid())
	{
		Res = InD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, D3DCmdAllocator.GetReference(), nullptr, __uuidof(ID3D12CommandList), reinterpret_cast<void**>(D3DCmdList.GetInitReference()));
		check(!FAILED(Res));
		D3DCmdList->Close();
#if !UE_BUILD_SHIPPING
		D3DCmdList->SetName(TEXT("ElectraVideoDecoderCmdList"));
#endif
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	// Create texture to copy decoder output buffer into as needed

	D3D12_RESOURCE_DESC ResourceDesc;
	ResourceDesc = InResource->GetDesc();

	const bool bIsTexture = (ResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

	const FIntPoint Dim = GetDim();
	const EPixelFormat PixFmt = GetFormat();
	const EVideoDecoderPixelEncoding FmtEnc = GetFormatEncoding();

	// Find out if the image data is sRGB encoded (we also assume this by default)
	bool bOutputIsSRGB = true;
	if (auto Colorimetry = GetColorimetry())
	{
		if (auto MPEGDef = Colorimetry->GetMPEGDefinition())
		{
			bOutputIsSRGB = (MPEGDef->TransferCharacteristics == 2);
		}
	}

	// Check if we can use HW sRGB decode (we must avoid doing this if the data represents something else as true RGB(A) values)
	const bool bIsSRGB = bOutputIsSRGB && (FmtEnc == EVideoDecoderPixelEncoding::Native || FmtEnc == EVideoDecoderPixelEncoding::RGB || FmtEnc == EVideoDecoderPixelEncoding::RGBA);

	// note: this somewhat duplicates / bypasses the GPixelFormat array, which we COULD access here to get the DXGI formats.
	//		 BUT! The platform format is only inserted by RHI and we do NOT rely ion RHI being operational!
	uint32 BlockSizeX = 1;
	uint32 BlockSizeY = 1;
	DXGI_FORMAT DXGIFmt = DXGI_FORMAT_UNKNOWN;
	switch (PixFmt)
	{
		case PF_NV12:				DXGIFmt = DXGI_FORMAT_NV12;				break;
		case PF_P010:				DXGIFmt = DXGI_FORMAT_P010;				break;
		case PF_A16B16G16R16:		DXGIFmt = DXGI_FORMAT_R16G16B16A16_UNORM; break;
		case PF_R16G16B16A16_UNORM:	DXGIFmt = DXGI_FORMAT_R16G16B16A16_UNORM; break;
		case PF_A32B32G32R32F:		DXGIFmt = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
		case PF_B8G8R8A8:			DXGIFmt = bIsSRGB ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_TYPELESS;  break;
		case PF_R8G8B8A8:			DXGIFmt = bIsSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_TYPELESS;  break;
		case PF_A2B10G10R10:		DXGIFmt = DXGI_FORMAT_R10G10B10A2_UNORM;  break;
		case PF_DXT1:				DXGIFmt = bIsSRGB ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM; BlockSizeX = BlockSizeY = 4; break;
		case PF_DXT5:				DXGIFmt = bIsSRGB ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM; BlockSizeX = BlockSizeY = 4; break;
		case PF_BC4:				DXGIFmt = DXGI_FORMAT_BC4_UNORM; BlockSizeX = BlockSizeY = 4; break;
		default:
			check(!"Unexpected pixel format for D3D12 buffer resource!");
	}

	/*
	* - We need to copy this from the output buffers ASAP to not throttle buffer availability (and hence performance) for the decoder
	* - As RHI limits resource creation to the renderthread, which would delay creation, we need to use a platform API level texture we can create right here and now
	*/
	if (!TextureDX12.IsValid() || SampleDim != TextureDX12Dim)
	{
		// We get here only after the instance came back from the pool! Hence we can be sure any old texture is no longer actively used.
		TextureDX12 = nullptr;

		if (!D3D12ResourcePool.IsValid())
		{
			D3D12ResourcePool = MakeShared<FElectraMediaDecoderOutputBufferPool_DX12>(InD3D12Device, MaxOutputBuffers + kElectraDecoderPipelineExtraFrames, MaxWidth, MaxHeight, DXGIFmt, D3D12_HEAP_TYPE_DEFAULT);
			if (!D3D12ResourcePool.IsValid())
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("Could not allocate texture resource heap!"));
				return;
			}
			InOutD3D12ResourcePool = D3D12ResourcePool;
		}
		
		FElectraMediaDecoderOutputBufferPool_DX12::FOutputData OutputData;
		if (!D3D12ResourcePool->AllocateOutputDataAsTexture(OutputData, SampleDim.X, SampleDim.Y, DXGIFmt))
		{
			UE_LOG(LogElectraPlayer, Error, TEXT("Could not allocate texture resource from heap!"));
			return;
		}

		TextureDX12 = OutputData.Resource;
#if !UE_BUILD_SHIPPING
		TextureDX12->SetName(TEXT("ElectraVideoDecoderOutputFrame"));
#endif
		D3DFence = OutputData.Fence;
		FenceValue = OutputData.FenceValue;

		TextureDX12Dim = SampleDim;
	}
	else
	{
		// We reuse the texture, but we need to update the fence info...
		check(D3D12ResourcePool.IsValid());
		D3DFence = D3D12ResourcePool->GetUpdatedBufferFence(FenceValue);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	// Build command list to copy data from decoder buffer

	D3D12_RESOURCE_BARRIER PreCopyResourceBarriers[] = {
		{D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {{ InResource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE }}},
		{D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {{ TextureDX12, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST}}},
	};

	D3D12_RESOURCE_BARRIER PostCopyResourceBarriers[] = {
		{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {{ InResource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON }}},
		{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {{ TextureDX12, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON }}},
	};

	D3DCmdAllocator->Reset();
	D3DCmdList->Reset(D3DCmdAllocator, nullptr);

	D3DCmdList->ResourceBarrier(2, PreCopyResourceBarriers);

	if (bIsTexture)
	{
		// If we got a texture, we simply can copy the resource as a whole...
		D3DCmdList->CopyResource(TextureDX12, InResource);
	}
	else
	{
		// Source is a buffer. Build a properly configured footprint and copy from there...
		if (PixFmt == PF_NV12 || PixFmt == PF_P010)
		{
			//
			// Formats with 2 planes
			//

			D3D12_BOX SrcBox;
			SrcBox.left = 0;
			SrcBox.top = 0;
			SrcBox.right = Dim.X;
			SrcBox.bottom = Dim.Y;
			SrcBox.back = 1;
			SrcBox.front = 0;

			// Copy Y plane
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT SrcFootPrint{};
			SrcFootPrint.Offset = 0;
			SrcFootPrint.Footprint.Format = (PixFmt == PF_NV12) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R16_UNORM;
			SrcFootPrint.Footprint.Width = Dim.X;
			SrcFootPrint.Footprint.Height = Dim.Y;
			SrcFootPrint.Footprint.Depth = 1;
			SrcFootPrint.Footprint.RowPitch = ResourcePitch;
			CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(InResource, SrcFootPrint);
			CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(TextureDX12, 0);
			D3DCmdList->CopyTextureRegion(&DestCopyLocation, 0, 0, 0, &SourceCopyLocation, &SrcBox);

			// Copy CbCr plane
			SrcBox.right = Dim.X >> 1;
			SrcBox.bottom = Dim.Y >> 1;
			SourceCopyLocation.PlacedFootprint.Offset = ResourcePitch * Dim.Y;
			SourceCopyLocation.PlacedFootprint.Footprint.Format = (PixFmt == PF_NV12) ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R16G16_UNORM;
			SourceCopyLocation.PlacedFootprint.Footprint.Width = Dim.X >> 1;
			SourceCopyLocation.PlacedFootprint.Footprint.Height = Dim.Y >> 1;
			DestCopyLocation.SubresourceIndex = 1;
			D3DCmdList->CopyTextureRegion(&DestCopyLocation, 0, 0, 0, &SourceCopyLocation, &SrcBox);
		}
		else
		{
			//
			// Single plane formats
			//

			D3D12_BOX SrcBox;
			SrcBox.left = 0;
			SrcBox.top = 0;
			SrcBox.right = Dim.X;
			SrcBox.bottom = Dim.Y;
			SrcBox.back = 1;
			SrcBox.front = 0;

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT SrcFootPrint{};
			SrcFootPrint.Offset = 0;
			SrcFootPrint.Footprint.Format = DXGIFmt;
			SrcFootPrint.Footprint.Width = Align(Dim.X, BlockSizeX);
			SrcFootPrint.Footprint.Height = Align(Dim.Y, BlockSizeY);
			SrcFootPrint.Footprint.Depth = 1;
			SrcFootPrint.Footprint.RowPitch = ResourcePitch;
			CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(InResource, SrcFootPrint);
			CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(TextureDX12, 0);
			D3DCmdList->CopyTextureRegion(&DestCopyLocation, 0, 0, 0, &SourceCopyLocation, &SrcBox);
		}
	}

	D3DCmdList->ResourceBarrier(2, PostCopyResourceBarriers);

	D3DCmdList->Close();

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	// Can we wait for the decoder to have the data ready using an async CPU job?
	bool bTriggerOk = false;
	if (OutputSync.TaskSync.IsValid())
	{
		// Yes. Trigger the GPU copy once the CPU is ready, so we can avoid any stalls on the copy-queue waiting for decoder data...
		auto ElectraDecoderResourceDelegate = Electra::FElectraDecoderResourceManagerWindows::GetDelegate();

		// Note: we capture "this" as we ensure that this instance only dies once the copy triggered here is actually done, hence ensuring any reference to "this" is done
		bTriggerOk = ElectraDecoderResourceDelegate->RunCodeAsync([D3DCmdList=this->D3DCmdList, D3DFence=this->D3DFence, FenceValue=this->FenceValue, OutputSync, ResourceDelegate = InResourceDelegate.Pin()]()
			{
				TriggerDataCopy(D3DCmdList, D3DFence, FenceValue, OutputSync, ResourceDelegate.Get());
			}, OutputSync.TaskSync.Get());
	}

	if (!bTriggerOk)
	{
		// We could not run the trigger async. Schedule the copy right away. Any needed synchronization will be done in the copy-queue by the GPU
		TriggerDataCopy(D3DCmdList, D3DFence, FenceValue, OutputSync, InResourceDelegate.Pin().Get());
	}
}

void FElectraPlayerVideoDecoderOutputPC::TriggerDataCopy(TRefCountPtr<ID3D12GraphicsCommandList> D3DCmdList, TRefCountPtr<ID3D12Fence> D3DFence, uint64 FenceValue, const FElectraDecoderOutputSync& OutputSync, Electra::IVideoDecoderResourceDelegate* InResourceDelegate)
{
	// Trigger copy (this will eventually execute on the submission thread of RHI if running in UE)
	// (note: we pass in all of FElectraDecoderOutputSync to guarantee any references needed to make the decoder output sync work are passed along, too!)
	InResourceDelegate->ExecuteCodeWithCopyCommandQueueUsage([CmdList = D3DCmdList, DestFence = D3DFence, DestFenceValue = FenceValue, OutputSync](ID3D12CommandQueue* D3DCmdQueue)
		{
			TRefCountPtr<ID3D12Fence> ResourceFence;
	#if ALLOW_MFSAMPLE_WITH_DX12
			TRefCountPtr<IMFD3D12SynchronizationObjectCommands> DecoderSync;
	#endif

			if (OutputSync.Sync.IsValid())
			{
	#if ALLOW_MFSAMPLE_WITH_DX12
				if (OutputSync.Sync->QueryInterface(__uuidof(IMFD3D12SynchronizationObjectCommands), (void**)DecoderSync.GetInitReference()) != S_OK)
	#endif // ALLOW_MFSAMPLE_WITH_DX12
				{
					OutputSync.Sync->QueryInterface(__uuidof(ID3D12Fence), (void**)ResourceFence.GetInitReference());
				}
			}

	#if ALLOW_MFSAMPLE_WITH_DX12
			if (DecoderSync)
			{
				// Sync queue to make sure decoder output is ready for us (WMF case)
				DecoderSync->EnqueueResourceReadyWait(D3DCmdQueue);
			}
	#endif
			if (ResourceFence)
			{
				// Sync queue to make sure decoder output is ready for us
				D3DCmdQueue->Wait(ResourceFence, OutputSync.SyncValue);
			}

			// Execute copy
			ID3D12CommandList* CmdLists[1] = { CmdList.GetReference() };
			D3DCmdQueue->ExecuteCommandLists(1, CmdLists);

	#if ALLOW_MFSAMPLE_WITH_DX12
			if (DecoderSync)
			{
				// Sync to end of copy operation and release MFSample once reached
				DecoderSync->EnqueueResourceRelease(D3DCmdQueue);
			}
	#endif
			// Trigger optional notification back to the decoder, so it could reuse its buffer after the copy
			if (OutputSync.CopyDoneSync)
			{
				D3DCmdQueue->Signal(OutputSync.CopyDoneSync, OutputSync.CopyDoneSyncValue);
			}

			// Notify user of the output data we just copied of its arrival
			D3DCmdQueue->Signal(DestFence, DestFenceValue);
		});
}

void FElectraPlayerVideoDecoderOutputPC::InitializeWithSharedTexture(const TRefCountPtr<ID3D11Device>& InD3D11Device, const TRefCountPtr<ID3D11DeviceContext> InDeviceContext, const TRefCountPtr<IMFSample> InMFSample, const FIntPoint& OutputDim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict)
{
	// Get output texture from decoder...
	TRefCountPtr<IMFMediaBuffer> MediaBuffer;
	if (InMFSample->GetBufferByIndex(0, MediaBuffer.GetInitReference()) != S_OK)
	{
		return;
	}
	TRefCountPtr<IMFDXGIBuffer> DXGIBuffer;
	if (MediaBuffer->QueryInterface(__uuidof(IMFDXGIBuffer), (void**)DXGIBuffer.GetInitReference()) != S_OK)
	{
		return;
	}
	TRefCountPtr<ID3D11Texture2D> DecoderTexture;
	if (DXGIBuffer->GetResource(IID_PPV_ARGS(DecoderTexture.GetInitReference())) != S_OK)
	{
		return;
	}
	uint32 ViewIndex = 0;
	if (DXGIBuffer->GetSubresourceIndex(&ViewIndex) != S_OK)
	{
		return;
	}

	InitializeWithSharedTexture(InD3D11Device, InDeviceContext, DecoderTexture, ViewIndex, OutputDim, InParamDict);
}

void FElectraPlayerVideoDecoderOutputPC::InitializeWithSharedTexture(const TRefCountPtr<ID3D11Device>& InD3D11Device, const TRefCountPtr<ID3D11DeviceContext> InDeviceContext, const TRefCountPtr<ID3D11Texture2D> DecoderTexture, uint32 ViewIndex, const FIntPoint& OutputDim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict)
{
	FVideoDecoderOutput::Initialize(InParamDict);

	OutputType = EOutputType::HardwareWin8Plus;

	bool bNeedsNew = !Texture.IsValid() || (SampleDim.X != OutputDim.X || SampleDim.Y != OutputDim.Y);

	if (bNeedsNew)
	{
		SampleDim = OutputDim;

		D3D11_TEXTURE2D_DESC DecoderTextureDesc;
		DecoderTexture->GetDesc(&DecoderTextureDesc);

		// Make a texture we pass on as output
		// (note: memory is NOT tracked in UE - much as other decoder related memory!)
		D3D11_TEXTURE2D_DESC TextureDesc;
		TextureDesc.Width = SampleDim.X;
		TextureDesc.Height = SampleDim.Y;
		TextureDesc.MipLevels = 1;
		TextureDesc.ArraySize = 1;
		TextureDesc.Format = DecoderTextureDesc.Format;
		TextureDesc.SampleDesc.Count = 1;
		TextureDesc.SampleDesc.Quality = 0;
		TextureDesc.Usage = D3D11_USAGE_DEFAULT;
		TextureDesc.BindFlags = 0;
		TextureDesc.CPUAccessFlags = 0;
		TextureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

		if (InD3D11Device->CreateTexture2D(&TextureDesc, nullptr, Texture.GetInitReference()) != S_OK)
		{
			return;
		}

		D3D11Device = InD3D11Device;
	}

	// If we got a texture, copy the data from the decoder into it...
	if (Texture)
	{

		// Source data may be larger than desired output, but crop area will be aligned such that we can always copy from 0,0
		D3D11_BOX SrcBox;
		SrcBox.left = 0;
		SrcBox.top = 0;
		SrcBox.front = 0;
		SrcBox.right = OutputDim.X;
		SrcBox.bottom = OutputDim.Y;
		SrcBox.back = 1;

		TRefCountPtr<IDXGIKeyedMutex> KeyedMutex;
		HRESULT res = Texture->QueryInterface(_uuidof(IDXGIKeyedMutex), (void**)&KeyedMutex);

		if (KeyedMutex)
		{
			// No wait on acquire since sample is new and key is 0.
			res = KeyedMutex->AcquireSync(0, 0);
			if (res == S_OK)
			{
				// Copy texture using the decoder DX11 device... (and apply any cropping - see above note)
				InDeviceContext->CopySubresourceRegion(Texture, 0, 0, 0, 0, DecoderTexture, ViewIndex, &SrcBox);

				// Mark texture as updated with key of 1
				// Sample will be read in Convert() method of texture sample
				KeyedMutex->ReleaseSync(1);
			}
		}

		// Make sure texture is updated before giving access to the sample in the rendering thread.
		InDeviceContext->Flush();
	}
}

bool FElectraPlayerVideoDecoderOutputPC::IsReadyForReuse()
{
	// Make sure any async copies to any internal GPU resources are done (DX12 case)
	// (this also guards any decoder created resources we keep to read from)
	if (TextureDX12.IsValid() && D3DFence.IsValid())
	{
		return D3DFence->GetCompletedValue() >= FenceValue;
	}
	return true;
}

void FElectraPlayerVideoDecoderOutputPC::ShutdownPoolable()
{
	TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe> lockedVideoRenderer = OwningRenderer.Pin();
	if (lockedVideoRenderer.IsValid())
	{
		lockedVideoRenderer->SampleReleasedToPool(this);
	}

	// Drop fence (if any)
	D3DFence = nullptr;

	// Drop decoder output resource
	DecoderOutputResource = nullptr;

	if (OutputType != EOutputType::HardwareWin8Plus)
	{
		return;
	}

	// Correctly release the keyed mutex when the sample is returned to the pool
	TRefCountPtr<IDXGIResource> OtherResource(nullptr);
	if (Texture)
	{
		Texture->QueryInterface(__uuidof(IDXGIResource), (void**)&OtherResource);
	}

	// Make sure DX11 shared texture sync state is as expected
	if (OtherResource)
	{
		HANDLE SharedHandle = nullptr;
		if (OtherResource->GetSharedHandle(&SharedHandle) == S_OK)
		{
			TRefCountPtr<ID3D11Resource> SharedResource;
			D3D11Device->OpenSharedResource(SharedHandle, __uuidof(ID3D11Texture2D), (void**)&SharedResource);
			if (SharedResource)
			{
				TRefCountPtr<IDXGIKeyedMutex> KeyedMutex;
				OtherResource->QueryInterface(_uuidof(IDXGIKeyedMutex), (void**)&KeyedMutex);

				if (KeyedMutex)
				{
					// Reset keyed mutex
					if (KeyedMutex->AcquireSync(1, 0) == S_OK)
					{
						// Texture was never read
						KeyedMutex->ReleaseSync(0);
					}
					else if (KeyedMutex->AcquireSync(2, 0) == S_OK)
					{
						// Texture was read at least once
						KeyedMutex->ReleaseSync(0);
					}
				}
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

FVideoDecoderOutput* FElectraPlayerPlatformVideoDecoderOutputFactory::Create()
{
	return new FElectraPlayerVideoDecoderOutputPC();
}
