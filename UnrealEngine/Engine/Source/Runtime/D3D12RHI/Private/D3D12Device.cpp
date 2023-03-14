// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "D3D12RayTracing.h"

#if INTEL_EXTENSIONS
	#define INTC_IGDEXT_D3D12 1

	THIRD_PARTY_INCLUDES_START
	#include "igdext.h"
	THIRD_PARTY_INCLUDES_END
#endif

static TAutoConsoleVariable<int32> CVarD3D12GPUTimeout(
	TEXT("r.D3D12.GPUTimeout"),
	1,
	TEXT("0: Disable GPU Timeout; use with care as it could freeze your PC!\n")
	TEXT("1: Enable GPU Timeout; operation taking long on the GPU will fail(default)\n"),
	ECVF_ReadOnly
);

static uint32 GetQueryHeapPoolIndex(D3D12_QUERY_HEAP_TYPE HeapType)
{
	switch (HeapType)
	{
	default: checkNoEntry(); [[fallthrough]];
	case D3D12_QUERY_HEAP_TYPE_OCCLUSION:            return 0;
	case D3D12_QUERY_HEAP_TYPE_TIMESTAMP:            return 1;
	case D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP: return 2;
	}
}

FD3D12Queue::FD3D12Queue(FD3D12Device* Device, ED3D12QueueType QueueType)
	: Device(Device)
	, QueueType(QueueType)
	, BarrierTimestamps(Device, QueueType, D3D12_QUERY_TYPE_TIMESTAMP)
{
	FD3D12Adapter* Adapter = Device->GetParentAdapter();
	const bool bFullGPUCrashDebugging = (Adapter->GetGPUCrashDebuggingModes() == ED3D12GPUCrashDebuggingModes::All);

	D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
	CommandQueueDesc.Type = GetD3DCommandListType((ED3D12QueueType)QueueType);
	CommandQueueDesc.Priority = 0;
	CommandQueueDesc.NodeMask = Device->GetGPUMask().GetNative();
	CommandQueueDesc.Flags = (bFullGPUCrashDebugging || CVarD3D12GPUTimeout.GetValueOnAnyThread() == 0)
		? D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT
		: D3D12_COMMAND_QUEUE_FLAG_NONE;

	FD3D12DynamicRHI::GetD3DRHI()->CreateCommandQueue(Device, CommandQueueDesc, D3DCommandQueue);
	D3DCommandQueue->SetName(*FString::Printf(TEXT("%s Queue (GPU %d)"), GetD3DCommandQueueTypeName(QueueType), Device->GetGPUIndex()));

	VERIFYD3D12RESULT(Device->GetDevice()->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(Fence.D3DFence.GetInitReference())
	));
	Fence.D3DFence->SetName(*FString::Printf(TEXT("%s Queue Fence (GPU %d)"), GetD3DCommandQueueTypeName(QueueType), Device->GetGPUIndex()));
}

void FD3D12Queue::SetupAfterDeviceCreation()
{
	// setup the bread crumb data to track GPU progress on this command queue when GPU crash debugging is enabled
	if (EnumHasAnyFlags(Device->GetParentAdapter()->GetGPUCrashDebuggingModes(), ED3D12GPUCrashDebuggingModes::BreadCrumbs))
	{
		// QI for the ID3DDevice3 - manual buffer write from command line only supported on 1709+
		TRefCountPtr<ID3D12Device3> D3D12Device3;
		HRESULT hr = Device->GetDevice()->QueryInterface(IID_PPV_ARGS(D3D12Device3.GetInitReference()));
		if (SUCCEEDED(hr))
		{
			// find out how many entries we can much push in a single event (limit to MAX_GPU_BREADCRUMB_DEPTH)
			int32 GPUCrashDataDepth = Device->GetGPUProfiler().GPUCrashDataDepth;
			int32 MaxEventCount = GPUCrashDataDepth > 0 ? FMath::Min(GPUCrashDataDepth, MAX_GPU_BREADCRUMB_DEPTH) : MAX_GPU_BREADCRUMB_DEPTH;

			const uint32 ShaderDiagnosticBufferSize = sizeof(FD3D12DiagnosticBufferData);

			// Allocate persistent CPU readable memory which will still be valid after a device lost and wrap this data in a placed resource
			// so the GPU command list can write to it
			const uint32 EventBufferSize = MaxEventCount * sizeof(uint32);
			const uint32 TotalBufferSize = EventBufferSize + ShaderDiagnosticBufferSize;

			// Create the platform-specific diagnostic buffer
			FString Name = FString::Printf(TEXT("DiagnosticBuffer (%s)"), GetD3DCommandQueueTypeName(QueueType));

			const D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(TotalBufferSize, D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER);
			DiagnosticBuffer = Device->CreateDiagnosticBuffer(BufferDesc, *Name);

			if (DiagnosticBuffer)
			{
				// Diagnostic buffer is split between breadcrumb events and diagnostic messages.
				DiagnosticBuffer->BreadCrumbsOffset = 0;
				DiagnosticBuffer->BreadCrumbsSize = EventBufferSize;

				DiagnosticBuffer->DiagnosticsOffset = DiagnosticBuffer->BreadCrumbsOffset + DiagnosticBuffer->BreadCrumbsSize;
				DiagnosticBuffer->DiagnosticsSize = ShaderDiagnosticBufferSize;
			}
		}
	}
}

FD3D12Queue::~FD3D12Queue()
{
	check(PendingSubmission.IsEmpty());
	check(PendingInterrupt.IsEmpty());
}

FD3D12Device::FD3D12Device(FRHIGPUMask InGPUMask, FD3D12Adapter* InAdapter)
	: FD3D12SingleNodeGPUObject(InGPUMask)
	, FD3D12AdapterChild       (InAdapter)
	, GPUProfilingData         (this)
	, ResidencyManager         (*this)
	, DescriptorHeapManager    (this)
	, BindlessDescriptorManager(this)
	, GlobalSamplerHeap        (this)
	, OnlineDescriptorManager  (this)
	, DefaultBufferAllocator   (this, FRHIGPUMask::All()) //Note: Cross node buffers are possible 
	, DefaultFastAllocator     (this, FRHIGPUMask::All(), D3D12_HEAP_TYPE_UPLOAD, 1024 * 1024 * 4)
	, TextureAllocator         (this, FRHIGPUMask::All())
{
	check(IsInGameThread());

	for (uint32 HeapType = 0; HeapType < (uint32)ERHIDescriptorHeapType::count; ++HeapType)
	{
		OfflineDescriptorManagers.Emplace(this, (ERHIDescriptorHeapType)HeapType);
	}

	for (uint32 QueueType = 0; QueueType < (uint32)ED3D12QueueType::Count; ++QueueType)
	{
		Queues.Emplace(this, (ED3D12QueueType)QueueType);
	}
}

FD3D12Device::~FD3D12Device()
{
#if D3D12_RHI_RAYTRACING
	delete RayTracingCompactionRequestHandler;
	RayTracingCompactionRequestHandler = nullptr;

	DestroyRayTracingDescriptorCache(); // #dxr_todo UE-72158: unify RT descriptor cache with main FD3D12DescriptorCache
#endif

	// Cleanup the allocator near the end, as some resources may be returned to the allocator or references are shared by multiple GPUs
	DefaultBufferAllocator.FreeDefaultBufferPools();

	DefaultFastAllocator.Destroy();

	TextureAllocator.CleanUpAllocations();
	TextureAllocator.Destroy();

	SamplerMap.Empty();
}

FD3D12Device::FResidencyManager::FResidencyManager(FD3D12Device& Parent)
{
#if ENABLE_RESIDENCY_MANAGEMENT
	IDXGIAdapter3* DxgiAdapter3 = nullptr;
	VERIFYD3D12RESULT(Parent.GetParentAdapter()->GetAdapter()->QueryInterface(IID_PPV_ARGS(&DxgiAdapter3)));
	const uint32 ResidencyMangerGPUIndex = GVirtualMGPU ? 0 : Parent.GetGPUIndex(); // GPU node index is used by residency manager to query budget
	D3DX12Residency::InitializeResidencyManager(*this, Parent.GetDevice(), ResidencyMangerGPUIndex, DxgiAdapter3, RESIDENCY_PIPELINE_DEPTH);
#endif // ENABLE_RESIDENCY_MANAGEMENT
}

FD3D12Device::FResidencyManager::~FResidencyManager()
{
#if ENABLE_RESIDENCY_MANAGEMENT
	D3DX12Residency::DestroyResidencyManager(*this);
#endif
}

ID3D12Device* FD3D12Device::GetDevice()
{
	return GetParentAdapter()->GetD3DDevice();
}

#if D3D12_RHI_RAYTRACING
ID3D12Device5* FD3D12Device::GetDevice5()
{
	return GetParentAdapter()->GetD3DDevice5();
}

ID3D12Device7* FD3D12Device::GetDevice7()
{
	return GetParentAdapter()->GetD3DDevice7();
}
#endif // D3D12_RHI_RAYTRACING

#if (PLATFORM_WINDOWS || PLATFORM_HOLOLENS)
typedef HRESULT(WINAPI *FDXGIGetDebugInterface1)(UINT, REFIID, void **);
#endif

static D3D12_FEATURE_DATA_FORMAT_SUPPORT GetFormatSupport(ID3D12Device* InDevice, DXGI_FORMAT InFormat)
{
	D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport{};
	FormatSupport.Format = InFormat;

	InDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &FormatSupport, sizeof(FormatSupport));

	return FormatSupport;
}

void FD3D12Device::SetupAfterDeviceCreation()
{
	for (FD3D12Queue& Queue : Queues)
	{
		Queue.SetupAfterDeviceCreation();
	}

	ID3D12Device* Direct3DDevice = GetParentAdapter()->GetD3DDevice();

	for (uint32 FormatIndex = PF_Unknown; FormatIndex < PF_MAX; FormatIndex++)
	{
		FPixelFormatInfo& PixelFormatInfo = GPixelFormats[FormatIndex];
		const DXGI_FORMAT PlatformFormat = static_cast<DXGI_FORMAT>(PixelFormatInfo.PlatformFormat);

		EPixelFormatCapabilities Capabilities = EPixelFormatCapabilities::None;

		if (PlatformFormat != DXGI_FORMAT_UNKNOWN)
		{
			const D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport    = GetFormatSupport(Direct3DDevice, PlatformFormat);
			const D3D12_FEATURE_DATA_FORMAT_SUPPORT SRVFormatSupport = GetFormatSupport(Direct3DDevice, FindShaderResourceDXGIFormat(PlatformFormat, false));
			const D3D12_FEATURE_DATA_FORMAT_SUPPORT UAVFormatSupport = GetFormatSupport(Direct3DDevice, FindUnorderedAccessDXGIFormat(PlatformFormat));
			const D3D12_FEATURE_DATA_FORMAT_SUPPORT RTVFormatSupport = GetFormatSupport(Direct3DDevice, FindShaderResourceDXGIFormat(PlatformFormat, false));
			const D3D12_FEATURE_DATA_FORMAT_SUPPORT DSVFormatSupport = GetFormatSupport(Direct3DDevice, FindDepthStencilDXGIFormat(PlatformFormat));

			auto ConvertCap1 = [&Capabilities](const D3D12_FEATURE_DATA_FORMAT_SUPPORT& InSupport, EPixelFormatCapabilities UnrealCap, D3D12_FORMAT_SUPPORT1 InFlags)
			{
				if (EnumHasAnyFlags(InSupport.Support1, InFlags))
				{
					EnumAddFlags(Capabilities, UnrealCap);
				}
			};
			auto ConvertCap2 = [&Capabilities](const D3D12_FEATURE_DATA_FORMAT_SUPPORT& InSupport, EPixelFormatCapabilities UnrealCap, D3D12_FORMAT_SUPPORT2 InFlags)
			{
				if (EnumHasAnyFlags(InSupport.Support2, InFlags))
				{
					EnumAddFlags(Capabilities, UnrealCap);
				}
			};

			ConvertCap1(FormatSupport, EPixelFormatCapabilities::Texture1D,               D3D12_FORMAT_SUPPORT1_TEXTURE1D);
			ConvertCap1(FormatSupport, EPixelFormatCapabilities::Texture2D,               D3D12_FORMAT_SUPPORT1_TEXTURE2D);
			ConvertCap1(FormatSupport, EPixelFormatCapabilities::Texture3D,               D3D12_FORMAT_SUPPORT1_TEXTURE3D);
			ConvertCap1(FormatSupport, EPixelFormatCapabilities::TextureCube,             D3D12_FORMAT_SUPPORT1_TEXTURECUBE);
			ConvertCap1(FormatSupport, EPixelFormatCapabilities::Buffer,                  D3D12_FORMAT_SUPPORT1_BUFFER);
			ConvertCap1(FormatSupport, EPixelFormatCapabilities::VertexBuffer,            D3D12_FORMAT_SUPPORT1_IA_VERTEX_BUFFER);
			ConvertCap1(FormatSupport, EPixelFormatCapabilities::IndexBuffer,             D3D12_FORMAT_SUPPORT1_IA_INDEX_BUFFER);

			if (EnumHasAnyFlags(Capabilities, EPixelFormatCapabilities::AnyTexture))
			{
				ConvertCap1(FormatSupport, EPixelFormatCapabilities::RenderTarget,        D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
				ConvertCap1(FormatSupport, EPixelFormatCapabilities::DepthStencil,        D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
				ConvertCap1(FormatSupport, EPixelFormatCapabilities::TextureMipmaps,      D3D12_FORMAT_SUPPORT1_MIP);
				ConvertCap1(SRVFormatSupport, EPixelFormatCapabilities::TextureLoad,      D3D12_FORMAT_SUPPORT1_SHADER_LOAD);
				ConvertCap1(SRVFormatSupport, EPixelFormatCapabilities::TextureSample,    D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
				ConvertCap1(SRVFormatSupport, EPixelFormatCapabilities::TextureGather,    D3D12_FORMAT_SUPPORT1_SHADER_GATHER);
				ConvertCap2(UAVFormatSupport, EPixelFormatCapabilities::TextureAtomics,   D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE);
				ConvertCap1(SRVFormatSupport, EPixelFormatCapabilities::TextureBlendable, D3D12_FORMAT_SUPPORT1_BLENDABLE);
				ConvertCap2(UAVFormatSupport, EPixelFormatCapabilities::TextureStore,     D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
			}

			if (EnumHasAnyFlags(Capabilities, EPixelFormatCapabilities::Buffer))
			{
				ConvertCap1(SRVFormatSupport, EPixelFormatCapabilities::BufferLoad,       D3D12_FORMAT_SUPPORT1_BUFFER);
				ConvertCap2(UAVFormatSupport, EPixelFormatCapabilities::BufferStore,      D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
				ConvertCap2(UAVFormatSupport, EPixelFormatCapabilities::BufferAtomics,    D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE);
			}

			ConvertCap1(UAVFormatSupport, EPixelFormatCapabilities::UAV,                  D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW);
			ConvertCap2(UAVFormatSupport, EPixelFormatCapabilities::TypedUAVLoad,         D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD);
			ConvertCap2(UAVFormatSupport, EPixelFormatCapabilities::TypedUAVStore,        D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
		}

		PixelFormatInfo.Capabilities = Capabilities;
	}

	GRHISupportsArrayIndexFromAnyShader = true;
	GRHISupportsStencilRefFromPixelShader = false; // TODO: Sort out DXC shader database SM6.0 usage. DX12 supports this feature, but need to improve DXC support.

#if (PLATFORM_WINDOWS || PLATFORM_HOLOLENS)
	// Check if we're running under GPU capture
	bool bUnderGPUCapture = false;

	// RenderDoc
	if (D3D12RHI_IsRenderDocPresent(Direct3DDevice))
	{
		// Running under RenderDoc, so enable capturing mode
		bUnderGPUCapture = true;
	}

	// Intel GPA
	{
		TRefCountPtr<IUnknown> IntelGPA;
		static const IID IntelGPAID = { 0xCCFFEF16, 0x7B69, 0x468F, {0xBC, 0xE3, 0xCD, 0x95, 0x33, 0x69, 0xA3, 0x9A} };

		if (SUCCEEDED(Direct3DDevice->QueryInterface(IntelGPAID, (void**)(IntelGPA.GetInitReference()))))
		{
			// Running under Intel GPA, so enable capturing mode
			bUnderGPUCapture = true;
		}
	}

	// AMD RGP profiler
	if (GEmitRgpFrameMarkers && FD3D12DynamicRHI::GetD3DRHI()->GetAmdAgsContext())
	{
		// Running on AMD with RGP profiling enabled, so enable capturing mode
		bUnderGPUCapture = true;
	}
#if USE_PIX

	// Only check windows version on PLATFORM_WINDOWS - Hololens can assume windows > 10.0 so the condition would always be true.
#if PLATFORM_WINDOWS
	// PIX (note that DXGIGetDebugInterface1 requires Windows 8.1 and up)
	if (FPlatformMisc::VerifyWindowsVersion(6, 3))
#endif
	{
		FDXGIGetDebugInterface1 DXGIGetDebugInterface1FnPtr = nullptr;

#if PLATFORM_HOLOLENS
		DXGIGetDebugInterface1FnPtr = DXGIGetDebugInterface1;
#else
		// CreateDXGIFactory2 is only available on Win8.1+, find it if it exists
		HMODULE DxgiDLL = LoadLibraryA("dxgi.dll");
		if (DxgiDLL)
		{
#pragma warning(push)
#pragma warning(disable: 4191) // disable the "unsafe conversion from 'FARPROC' to 'blah'" warning
			DXGIGetDebugInterface1FnPtr = (FDXGIGetDebugInterface1)(GetProcAddress(DxgiDLL, "DXGIGetDebugInterface1"));
#pragma warning(pop)
			FreeLibrary(DxgiDLL);
		}
#endif
		
		if (DXGIGetDebugInterface1FnPtr)
		{
			IID GraphicsAnalysisID;
			if (SUCCEEDED(IIDFromString(L"{9F251514-9D4D-4902-9D60-18988AB7D4B5}", &GraphicsAnalysisID)))
			{
				TRefCountPtr<IUnknown> GraphicsAnalysis;
				if (SUCCEEDED(DXGIGetDebugInterface1FnPtr(0, GraphicsAnalysisID, (void**)GraphicsAnalysis.GetInitReference())))
				{
					// Running under PIX, so enable capturing mode
					bUnderGPUCapture = true;
				}
			}
		}
	}
#endif // USE_PIX

	if(bUnderGPUCapture)
	{
		GDynamicRHI->EnableIdealGPUCaptureOptions(true);
	}
#endif // (PLATFORM_WINDOWS || PLATFORM_HOLOLENS)


	const int32 MaximumResourceHeapSize = GetParentAdapter()->GetMaxDescriptorsForHeapType(ERHIDescriptorHeapType::Standard);
	const int32 MaximumSamplerHeapSize = GetParentAdapter()->GetMaxDescriptorsForHeapType(ERHIDescriptorHeapType::Sampler);

	// This value can be tuned on a per app basis. I.e. most apps will never run into descriptor heap pressure so
	// can make this global heap smaller
	check(GGlobalResourceDescriptorHeapSize <= MaximumResourceHeapSize || MaximumResourceHeapSize < 0);
	check(GGlobalSamplerDescriptorHeapSize <= MaximumSamplerHeapSize);

	check(GGlobalSamplerHeapSize <= MaximumSamplerHeapSize);

	check(GOnlineDescriptorHeapSize <= GGlobalResourceDescriptorHeapSize);
	check(GBindlessResourceDescriptorHeapSize <= GGlobalResourceDescriptorHeapSize);
	check(GBindlessSamplerDescriptorHeapSize <= GGlobalSamplerDescriptorHeapSize);

	DescriptorHeapManager.Init(GGlobalResourceDescriptorHeapSize, GGlobalSamplerDescriptorHeapSize);

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	const bool bBindlessResources = RHIGetBindlessResourcesConfiguration(GMaxRHIShaderPlatform) != ERHIBindlessConfiguration::Disabled;
	const bool bBindlessSamplers = RHIGetBindlessSamplersConfiguration(GMaxRHIShaderPlatform) != ERHIBindlessConfiguration::Disabled;
	if (bBindlessResources || bBindlessSamplers)
	{
		BindlessDescriptorManager.Init(bBindlessResources ? GBindlessResourceDescriptorHeapSize : 0, bBindlessSamplers ? GBindlessSamplerDescriptorHeapSize : 0);
	}
#endif

	GlobalSamplerHeap.Init(GGlobalSamplerHeapSize);

	OnlineDescriptorManager.Init(GOnlineDescriptorHeapSize, GOnlineDescriptorHeapBlockSize);

	// Make sure we create the default views before the first command context
	CreateDefaultViews();

	// Needs to be called before creating command contexts
	UpdateConstantBufferPageProperties();

	UpdateMSAASettings();

#if D3D12_RHI_RAYTRACING
	check(RayTracingCompactionRequestHandler == nullptr);
	RayTracingCompactionRequestHandler = new FD3D12RayTracingCompactionRequestHandler(this);

	D3D12_RESOURCE_DESC DispatchRaysDescBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(D3D12_DISPATCH_RAYS_DESC), D3D12RHI_RESOURCE_FLAG_ALLOW_INDIRECT_BUFFER);
	RayTracingDispatchRaysDescBuffer = GetParentAdapter()->CreateRHIBuffer(
		DispatchRaysDescBufferDesc, 256,
		0, DispatchRaysDescBufferDesc.Width, BUF_DrawIndirect,
		ED3D12ResourceStateMode::MultiState, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, false /*bInitialData*/,
		GetGPUMask(), nullptr /*ResourceAllocator*/, TEXT("DispatchRaysDescBuffer"));
#endif // D3D12_RHI_RAYTRACING

	check(!ImmediateCommandContext);
	ImmediateCommandContext = FD3D12DynamicRHI::GetD3DRHI()->CreateCommandContext(this, ED3D12QueueType::Direct, true);
}

void FD3D12Device::CreateDefaultViews()
{
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
		SRVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.Texture2D.MipLevels = 1;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		DefaultViews.NullSRV = new FD3D12ViewDescriptorHandle(this, ERHIDescriptorHeapType::Standard);
		DefaultViews.NullSRV->CreateView(SRVDesc, nullptr, ED3D12DescriptorCreateReason::InitialCreate);
	}

	{
		D3D12_RENDER_TARGET_VIEW_DESC RTVDesc{};
		RTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		RTVDesc.Texture2D.MipSlice = 0;

		DefaultViews.NullRTV = new FD3D12ViewDescriptorHandle(this, ERHIDescriptorHeapType::RenderTarget);
		DefaultViews.NullRTV->CreateView(RTVDesc, nullptr);
	}

	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};
		UAVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		UAVDesc.Texture2D.MipSlice = 0;

		DefaultViews.NullUAV = new FD3D12ViewDescriptorHandle(this, ERHIDescriptorHeapType::Standard);
		DefaultViews.NullUAV->CreateView(UAVDesc, nullptr, nullptr, ED3D12DescriptorCreateReason::InitialCreate);
	}

#if USE_STATIC_ROOT_SIGNATURE
	DefaultViews.NullCBV = new FD3D12ConstantBufferView(this);
#endif

	{
		const FSamplerStateInitializerRHI SamplerDesc(SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp);
		DefaultViews.DefaultSampler = CreateSampler(SamplerDesc);

		// The default sampler must have ID=0
		// FD3D12DescriptorCache::SetSamplers relies on this
		check(DefaultViews.DefaultSampler->ID == 0);
	}
}

void FD3D12Device::UpdateConstantBufferPageProperties()
{
	//In genera, constant buffers should use write-combine memory (i.e. upload heaps) for optimal performance
	bool bForceWriteBackConstantBuffers = false;

	if (bForceWriteBackConstantBuffers)
	{
		ConstantBufferPageProperties = GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
		ConstantBufferPageProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	}
	else
	{
		ConstantBufferPageProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	}
}

void FD3D12Device::UpdateMSAASettings()
{
	check(DX_MAX_MSAA_COUNT == 8);

	// quality levels are only needed for CSAA which we cannot use with custom resolves

	// 0xffffffff means not available
	AvailableMSAAQualities[0] = 0xffffffff;
	AvailableMSAAQualities[1] = 0xffffffff;
	AvailableMSAAQualities[2] = 0;
	AvailableMSAAQualities[3] = 0xffffffff;
	AvailableMSAAQualities[4] = 0;
	AvailableMSAAQualities[5] = 0xffffffff;
	AvailableMSAAQualities[6] = 0xffffffff;
	AvailableMSAAQualities[7] = 0xffffffff;
	AvailableMSAAQualities[8] = 0;
}

void FD3D12Device::RegisterGPUWork(uint32 NumPrimitives, uint32 NumVertices)
{
	GetGPUProfiler().RegisterGPUWork(NumPrimitives, NumVertices);
}

void FD3D12Device::RegisterGPUDispatch(FIntVector GroupCount)
{
	GetGPUProfiler().RegisterGPUDispatch(GroupCount);
}

void FD3D12Device::BlockUntilIdle()
{
	// Submit a new sync point to each queue
	TArray<FD3D12Payload*, TInlineAllocator<(uint32)ED3D12QueueType::Count>> Payloads;
	TArray<FD3D12SyncPointRef, TInlineAllocator<(uint32)ED3D12QueueType::Count>> SyncPoints;

	for (uint32 QueueTypeIndex = 0; QueueTypeIndex < (uint32)ED3D12QueueType::Count; ++QueueTypeIndex)
	{
		FD3D12SyncPointRef SyncPoint = FD3D12SyncPoint::Create(ED3D12SyncPointType::GPUAndCPU);

		FD3D12Payload* Payload = new FD3D12Payload(this, (ED3D12QueueType)QueueTypeIndex);
		Payload->SyncPointsToSignal.Add(SyncPoint);

		Payloads.Add(Payload);
		SyncPoints.Add(SyncPoint);
	}

	FD3D12DynamicRHI::GetD3DRHI()->SubmitPayloads(Payloads);

	// Block this thread until the sync points have signaled.
	for (FD3D12SyncPointRef& SyncPoint : SyncPoints)
	{
		SyncPoint->Wait();
	}
}

D3D12_RESOURCE_ALLOCATION_INFO FD3D12Device::GetResourceAllocationInfo(const FD3D12ResourceDesc& InDesc)
{
	uint64 Hash = CityHash64((const char*)&InDesc, sizeof(FD3D12ResourceDesc));

	// By default there'll be more threads trying to read this than to write it.
	ResourceAllocationInfoMapMutex.ReadLock();
	D3D12_RESOURCE_ALLOCATION_INFO* CachedInfo = ResourceAllocationInfoMap.Find(Hash);
	ResourceAllocationInfoMapMutex.ReadUnlock();

	if (CachedInfo)
	{
		return *CachedInfo;
	}
	else
	{
		D3D12_RESOURCE_ALLOCATION_INFO Result;
#if INTEL_EXTENSIONS
		if (InDesc.bRequires64BitAtomicSupport && IsRHIDeviceIntel() && GRHISupportsAtomicUInt64)
		{
			FD3D12ResourceDesc LocalDesc = InDesc;

			INTC_D3D12_RESOURCE_DESC_0001 IntelLocalDesc{};
			IntelLocalDesc.pD3D12Desc = &LocalDesc;
			IntelLocalDesc.EmulatedTyped64bitAtomics = true;

			Result = INTC_D3D12_GetResourceAllocationInfo(FD3D12DynamicRHI::GetD3DRHI()->GetIntelExtensionContext(), 0, 1, &IntelLocalDesc);
		}
		else
#endif
		{
			Result = GetDevice()->GetResourceAllocationInfo(0, 1, &InDesc);
		}

		ResourceAllocationInfoMapMutex.WriteLock();
		// Try search again with write lock because could have been added already
		CachedInfo = ResourceAllocationInfoMap.Find(Hash);
		if (CachedInfo == nullptr)
		{
			ResourceAllocationInfoMap.Add(Hash, Result);
		}
		ResourceAllocationInfoMapMutex.WriteUnlock();
		return Result;
	}
}

FD3D12ContextCommon* FD3D12Device::ObtainContext(ED3D12QueueType QueueType)
{
	FD3D12ContextCommon* Context = Queues[(uint32)QueueType].ObjectPool.Contexts.Pop();
	if (!Context)
	{
		switch (QueueType)
		{
		default: checkNoEntry(); // fallthrough
		case ED3D12QueueType::Direct: Context = new FD3D12CommandContext(this, QueueType, false); break;
		case ED3D12QueueType::Async : Context = new FD3D12CommandContext(this, QueueType, false); break;
		case ED3D12QueueType::Copy  : Context = new FD3D12ContextCopy   (this);                   break;
		}
	}

	check(Context);
	return Context;
}

void FD3D12Device::ReleaseContext(FD3D12ContextCommon* Context)
{
	check(Context && !Context->IsOpen());

	Queues[(uint32)Context->QueueType].ObjectPool.Contexts.Push(Context);
}

FD3D12CommandAllocator* FD3D12Device::ObtainCommandAllocator(ED3D12QueueType QueueType)
{
	FD3D12CommandAllocator* Allocator = Queues[(uint32)QueueType].ObjectPool.Allocators.Pop();
	if (!Allocator)
	{
		Allocator = new FD3D12CommandAllocator(this, QueueType);
	}

	check(Allocator);
	return Allocator;
}

void FD3D12Device::ReleaseCommandAllocator(FD3D12CommandAllocator* Allocator)
{
	check(Allocator);
	Allocator->Reset();
	Queues[(uint32)Allocator->QueueType].ObjectPool.Allocators.Push(Allocator);
}

FD3D12CommandList* FD3D12Device::ObtainCommandList(FD3D12CommandAllocator* CommandAllocator, FD3D12QueryAllocator* TimestampAllocator)
{
	check(CommandAllocator->Device == this);

	FD3D12CommandList* List = Queues[(uint32)CommandAllocator->QueueType].ObjectPool.Lists.Pop();
	if (!List)
	{
		List = new FD3D12CommandList(CommandAllocator, TimestampAllocator);
	}
	else
	{
		List->Reset(CommandAllocator, TimestampAllocator);
	}
	
	check(List);
	return List;
}

void FD3D12Device::ReleaseCommandList(FD3D12CommandList* CommandList)
{
	check(CommandList);
	Queues[(uint32)CommandList->QueueType].ObjectPool.Lists.Push(CommandList);
}

TRefCountPtr<FD3D12QueryHeap> FD3D12Device::ObtainQueryHeap(ED3D12QueueType QueueType, D3D12_QUERY_TYPE QueryType)
{
	D3D12_QUERY_HEAP_TYPE HeapType;
	switch (QueryType)
	{
		default:
			checkNoEntry();
			return nullptr;

		case D3D12_QUERY_TYPE_OCCLUSION:
			HeapType = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
			break;

		case D3D12_QUERY_TYPE_TIMESTAMP:
			if (QueueType == ED3D12QueueType::Copy)
			{
				// Support for copy queue timestamps is driver dependent.
				if (!GetParentAdapter()->AreCopyQueueTimestampQueriesSupported())
					return nullptr; // Not supported

				HeapType = D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP;
			}
			else
			{
				HeapType = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
			}
			break;
	}

	FD3D12QueryHeap* QueryHeap = QueryHeapPool[GetQueryHeapPoolIndex(HeapType)].Pop();
	if (!QueryHeap)
	{
		// Size the query heap to fill a 64KB page
		QueryHeap = new FD3D12QueryHeap(this, QueryType, HeapType, 65536 / FD3D12QueryHeap::ResultSize);
	}

	check(QueryHeap->QueryType == QueryType);
	check(QueryHeap->HeapType == HeapType);

	return QueryHeap;
}

void FD3D12Device::ReleaseQueryHeap(FD3D12QueryHeap* QueryHeap)
{
	check(QueryHeap);
	QueryHeapPool[GetQueryHeapPoolIndex(QueryHeap->HeapType)].Push(QueryHeap);
}

uint64 FD3D12Device::GetTimestampFrequency(ED3D12QueueType QueueType)
{
	uint64 Frequency;
	VERIFYD3D12RESULT(Queues[(uint32)QueueType].D3DCommandQueue->GetTimestampFrequency(&Frequency));
	return Frequency;
}

FGPUTimingCalibrationTimestamp FD3D12Device::GetCalibrationTimestamp(ED3D12QueueType QueueType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(D3D12GetCalibrationTimestamp);

	uint64 GPUTimestampFrequency = GetTimestampFrequency(QueueType);

	LARGE_INTEGER CPUTimestempFrequency;
	QueryPerformanceFrequency(&CPUTimestempFrequency);

	uint64 GPUTimestamp, CPUTimestamp;
	VERIFYD3D12RESULT(Queues[(uint32)QueueType].D3DCommandQueue->GetClockCalibration(&GPUTimestamp, &CPUTimestamp));

	FGPUTimingCalibrationTimestamp Result = {};

	Result.GPUMicroseconds = uint64(GPUTimestamp * (1e6 / GPUTimestampFrequency));
	Result.CPUMicroseconds = uint64(CPUTimestamp * (1e6 / CPUTimestempFrequency.QuadPart));

	return Result;
}
