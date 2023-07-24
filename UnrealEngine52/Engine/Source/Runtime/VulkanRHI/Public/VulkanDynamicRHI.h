// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanRHI.h: Public Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "IVulkanDynamicRHI.h"

class FVulkanFramebuffer;
class FVulkanDevice;
class FVulkanQueue;
class IHeadMountedDisplayVulkanExtensions;
struct FRHITransientHeapAllocation;

struct FOptionalVulkanInstanceExtensions
{
	union
	{
		struct
		{
			uint32 HasKHRExternalMemoryCapabilities : 1;
			uint32 HasKHRGetPhysicalDeviceProperties2 : 1;
		};
		uint32 Packed;
	};

	FOptionalVulkanInstanceExtensions()
	{
		static_assert(sizeof(Packed) == sizeof(FOptionalVulkanInstanceExtensions), "More bits needed!");
		Packed = 0;
	}
};

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/** The interface which is implemented by the dynamically bound RHI. */
class FVulkanDynamicRHI : public IVulkanDynamicRHI
{
public:

	/** Initialization constructor. */
	FVulkanDynamicRHI();

	/** Destructor */
	~FVulkanDynamicRHI() {}

	// IVulkanDynamicRHI interface
	virtual uint32 RHIGetVulkanVersion() const final override;
	virtual VkInstance RHIGetVkInstance() const final override;
	virtual VkDevice RHIGetVkDevice() const final override;
	virtual const uint8* RHIGetVulkanDeviceUUID() const final override;
	virtual VkPhysicalDevice RHIGetVkPhysicalDevice() const final override;
	virtual const VkAllocationCallbacks* RHIGetVkAllocationCallbacks() final override;
	virtual VkQueue RHIGetGraphicsVkQueue() const final override;
	virtual uint32 RHIGetGraphicsQueueIndex() const final override;
	virtual uint32 RHIGetGraphicsQueueFamilyIndex() const final override;
	virtual VkCommandBuffer RHIGetActiveVkCommandBuffer() final override;
	virtual uint64 RHIGetGraphicsAdapterLUID(VkPhysicalDevice InPhysicalDevice) const final override;
	virtual bool RHIDoesAdapterMatchDevice(const void* InAdapterId) const final override;
	virtual void* RHIGetVkDeviceProcAddr(const char* InName) const final override;
	virtual VkFormat RHIGetSwapChainVkFormat(EPixelFormat InFormat) const final override;
	virtual bool RHISupportsEXTFragmentDensityMap2() const final override;
	virtual TArray<VkExtensionProperties> RHIGetAllInstanceExtensions() const final override;
	virtual TArray<VkExtensionProperties> RHIGetAllDeviceExtensions(VkPhysicalDevice InPhysicalDevice) const final override;
	virtual FTexture2DRHIRef RHICreateTexture2DFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, VkImage Resource, ETextureCreateFlags Flags, const FClearValueBinding& ClearValueBinding = FClearValueBinding::Transparent) final override;
	virtual FTexture2DArrayRHIRef RHICreateTexture2DArrayFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, VkImage Resource, ETextureCreateFlags Flags, const FClearValueBinding& ClearValueBinding = FClearValueBinding::Transparent) final override;
	virtual FTextureCubeRHIRef RHICreateTextureCubeFromResource(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, VkImage Resource, ETextureCreateFlags Flags, const FClearValueBinding& ClearValueBinding = FClearValueBinding::Transparent) final override;
	virtual VkImage RHIGetVkImage(FRHITexture* InTexture) const final override;
	virtual VkFormat RHIGetViewVkFormat(FRHITexture* InTexture) const final override;
	virtual FVulkanRHIAllocationInfo RHIGetAllocationInfo(FRHITexture* InTexture) const final override;
	virtual FVulkanRHIImageViewInfo RHIGetImageViewInfo(FRHITexture* InTexture) const final override;

	UE_DEPRECATED(5.2, "Altering image layout tracking directly is not permitted anymore.")
	virtual VkImageLayout& RHIFindOrAddLayoutRW(FRHITexture* InTexture, VkImageLayout LayoutIfNotFound) final override;

	virtual void RHISetImageLayout(VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange) final override;
	virtual void RHISetUploadImageLayout(VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange) final override;
	virtual void RHIFinishExternalComputeWork(VkCommandBuffer InCommandBuffer) final override;
	virtual void RHIRegisterWork(uint32 NumPrimitives) final override;
	virtual void RHISubmitUploadCommandBuffer() final override;
	virtual void RHIVerifyResult(VkResult Result, const ANSICHAR* VkFuntion, const ANSICHAR* Filename, uint32 Line) final override;

	// FDynamicRHI interface.
	virtual void Init() final override;
	virtual void PostInit() final override;
	virtual void Shutdown() final override;;
	virtual const TCHAR* GetName() final override { return TEXT("Vulkan"); }

	void InitInstance();

	virtual FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer) final override;
	virtual FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) final override;
	virtual FDepthStencilStateRHIRef RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer) final override;
	virtual FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer) final override;
	virtual FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements) final override;
	virtual FPixelShaderRHIRef RHICreatePixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FVertexShaderRHIRef RHICreateVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FGeometryShaderRHIRef RHICreateGeometryShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FComputeShaderRHIRef RHICreateComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FGPUFenceRHIRef RHICreateGPUFence(const FName &Name) final override;
	virtual void RHICreateTransition(FRHITransition* Transition, const FRHITransitionCreateInfo& CreateInfo) final override;
	virtual void RHIReleaseTransition(FRHITransition* Transition) final override;
	virtual FStagingBufferRHIRef RHICreateStagingBuffer() final override;
	virtual FBoundShaderStateRHIRef RHICreateBoundShaderState(FRHIVertexDeclaration* VertexDeclaration, FRHIVertexShader* VertexShader, FRHIPixelShader* PixelShader, FRHIGeometryShader* GeometryShader) final override;
	virtual FGraphicsPipelineStateRHIRef RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer) final override;
	virtual FComputePipelineStateRHIRef RHICreateComputePipelineState(FRHIComputeShader* ComputeShader) final override;
	virtual FUniformBufferRHIRef RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout* Layout, EUniformBufferUsage Usage, EUniformBufferValidation Validation) final override;
	virtual void RHIUpdateUniformBuffer(FRHICommandListBase& RHICmdList, FRHIUniformBuffer* UniformBufferRHI, const void* Contents) final override;
	virtual FBufferRHIRef RHICreateBuffer(FRHICommandListBase& RHICmdList, uint32 Size, EBufferUsageFlags Usage, uint32 Stride, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void RHICopyBuffer(FRHIBuffer* SourceBuffer, FRHIBuffer* DestBuffer) final override;
	virtual void RHITransferBufferUnderlyingResource(FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer) final override;
	virtual void* LockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode) final override;
	virtual void UnlockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer) final override;
	virtual void RHIUnlockBuffer(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer) final override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FRHIBuffer* Buffer, bool bUseUAVCounter, bool bAppendBuffer) final override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FRHITexture* Texture, uint32 MipLevel, uint16 FirstArraySlice, uint16 NumArraySlices) final override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FRHIBuffer* Buffer, uint8 Format) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FRHIBuffer* Buffer) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FRHIBuffer* Buffer, uint32 Stride, uint8 Format) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(const FShaderResourceViewInitializer& Initializer) final override;
	virtual void RHIUpdateShaderResourceView(FRHIShaderResourceView* SRV, FRHIBuffer* Buffer, uint32 Stride, uint8 Format) final override;
	virtual void RHIUpdateShaderResourceView(FRHIShaderResourceView* SRV, FRHIBuffer* Buffer) final override;
	virtual void RHIUpdateTextureReference(FRHITextureReference* TextureRef, FRHITexture* NewTexture) final override;
	virtual FRHICalcTextureSizeResult RHICalcTexturePlatformSize(FRHITextureDesc const& Desc, uint32 FirstMipIndex) final override;
	virtual void RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats) final override;
	virtual bool RHIGetTextureMemoryVisualizeData(FColor* TextureData, int32 SizeX, int32 SizeY, int32 Pitch, int32 PixelSize) final override;
	virtual FTextureRHIRef RHICreateTexture(const FRHITextureCreateDesc& CreateDesc) final override;
	virtual FTextureRHIRef RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ERHIAccess InResourceState, void** InitialMipData, uint32 NumInitialMips) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FRHITexture* Texture, const FRHITextureSRVCreateInfo& CreateInfo) final override;
	virtual uint32 RHIComputeMemorySize(FRHITexture* TextureRHI) final override;
	virtual FTexture2DRHIRef RHIAsyncReallocateTexture2D(FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus) final override;
	virtual ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override;
	virtual ETextureReallocationStatus FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override
	{
		return this->RHIFinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
	}

	virtual ETextureReallocationStatus RHICancelAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override;
	virtual void* RHILockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail) final override
	{
		InternalUnlockTexture2D(false, Texture, MipIndex, bLockWithinMiptail);
	}
	virtual void* RHILockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIUpdateTexture2D(FRHICommandListBase& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData) final override
	{
		InternalUpdateTexture2D(RHICmdList, Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
	}
	virtual void RHIUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData) final override
	{
		InternalUpdateTexture3D(RHICmdList, Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
	}
	virtual void* RHILockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIBindDebugLabelName(FRHITexture* Texture, const TCHAR* Name) final override;
	virtual void RHIBindDebugLabelName(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const TCHAR* Name) final override;
	virtual void RHIReadSurfaceData(FRHITexture* Texture, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIReadSurfaceData(FRHITexture* Texture, FIntRect Rect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIMapStagingSurface(FRHITexture* Texture, FRHIGPUFence* Fence, void*& OutData, int32& OutWidth, int32& OutHeight, uint32 GPUIndex = 0) final override;
	virtual void RHIUnmapStagingSurface(FRHITexture* Texture, uint32 GPUIndex = 0) final override;

	virtual void RHIReadSurfaceFloatData(FRHITexture* Texture, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace, int32 ArrayIndex, int32 MipIndex) final override;
	virtual void RHIRead3DSurfaceFloatData(FRHITexture* Texture, FIntRect Rect, FIntPoint ZMinMax, TArray<FFloat16Color>& OutData) final override;
	virtual FRenderQueryRHIRef RHICreateRenderQuery(ERenderQueryType QueryType) final override;
	virtual bool RHIGetRenderQueryResult(FRHIRenderQuery* RenderQuery, uint64& OutResult, bool bWait, uint32 GPUIndex = INDEX_NONE) final override;
	virtual FTexture2DRHIRef RHIGetViewportBackBuffer(FRHIViewport* Viewport) final override;
	virtual void RHIAliasTextureResources(FTextureRHIRef& DestTexture, FTextureRHIRef& SrcTexture) final override;
	virtual FTextureRHIRef RHICreateAliasedTexture(FTextureRHIRef& SourceTexture) final override;
	virtual void RHIAdvanceFrameForGetViewportBackBuffer(FRHIViewport* Viewport) final override;
	virtual void RHIAcquireThreadOwnership() final override;
	virtual void RHIReleaseThreadOwnership() final override;
	virtual void RHIFlushResources() final override;
	virtual uint32 RHIGetGPUFrameCycles(uint32 GPUIndex = 0) final override;
	virtual FViewportRHIRef RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) final override;
	virtual void RHIResizeViewport(FRHIViewport* Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen) final override;
	virtual void RHIResizeViewport(FRHIViewport* Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) final override;
	virtual void RHITick(float DeltaTime) final override;
	virtual void RHIBlockUntilGPUIdle() final override;
	virtual void RHISubmitCommandsAndFlushGPU() final override;
	virtual void RHISuspendRendering() final override;
	virtual void RHIResumeRendering() final override;
	virtual bool RHIIsRenderingSuspended() final override;
	virtual bool RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate) final override;
	virtual void RHIGetSupportedResolution(uint32& Width, uint32& Height) final override;
	virtual void RHIVirtualTextureSetFirstMipInMemory(FRHITexture2D* Texture, uint32 FirstMip) final override;
	virtual void RHIVirtualTextureSetFirstMipVisible(FRHITexture2D* Texture, uint32 FirstMip) final override;

	virtual void* RHIGetNativeDevice() final override;
	virtual void* RHIGetNativePhysicalDevice() final override;
	virtual void* RHIGetNativeGraphicsQueue() final override;
	virtual void* RHIGetNativeComputeQueue() final override;
	virtual void* RHIGetNativeInstance() final override;
	virtual class IRHICommandContext* RHIGetDefaultContext() final override;
	virtual class IRHIComputeContext* RHIGetDefaultAsyncComputeContext() final override;
	virtual IRHIComputeContext* RHIGetCommandContext(ERHIPipeline Pipeline, FRHIGPUMask GPUMask) final override;
	virtual IRHIPlatformCommandList* RHIFinalizeContext(IRHIComputeContext* Context) final override;
	virtual void RHISubmitCommandLists(TArrayView<IRHIPlatformCommandList*> CommandLists) final override;
	virtual uint64 RHIGetMinimumAlignmentForBufferBackedSRV(EPixelFormat Format) final override;

	// Transient Resource Functions and Helpers
	virtual IRHITransientResourceAllocator* RHICreateTransientResourceAllocator() final override;

	virtual FTexture2DRHIRef AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus) override final;
	//virtual ETextureReallocationStatus CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted) override final;
	
	virtual void* LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true) override final
	{
		return RHILockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}
	virtual void UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true) override final
	{
		InternalUnlockTexture2D(true, Texture, MipIndex, bLockWithinMiptail);
	}
	
	virtual FUpdateTexture3DData RHIBeginUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion) override final;
	virtual void RHIEndUpdateTexture3D(FRHICommandListBase& RHICmdList, FUpdateTexture3DData& UpdateData) override final;

	virtual FTextureRHIRef RHICreateTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, const FRHITextureCreateDesc& CreateDesc) final override
	{
		// this-> is required to avoid calling the global version of this function
		return this->RHICreateTexture(CreateDesc);
	}
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, bool bUseUAVCounter, bool bAppendBuffer) final override
	{
		// this-> is required to avoid calling the global version of this function
		return this->RHICreateUnorderedAccessView(Buffer, bUseUAVCounter, bAppendBuffer);
	}

	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, uint32 MipLevel, uint16 FirstArraySlice, uint16 NumArraySlices) final override
	{
		// this-> is required to avoid calling the global version of this function
		return this->RHICreateUnorderedAccessView(Texture, MipLevel, FirstArraySlice, NumArraySlices);
	}

	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, uint8 Format) final override
	{
		// this-> is required to avoid calling the global version of this function
		return this->RHICreateUnorderedAccessView(Buffer, Format);
	}

	virtual FShaderResourceViewRHIRef CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, uint32 Stride, uint8 Format) final override
	{
		return this->RHICreateShaderResourceView(Buffer, Stride, Format);
	}

	virtual FShaderResourceViewRHIRef CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, const FShaderResourceViewInitializer& Initializer) final override
	{
		return this->RHICreateShaderResourceView(Initializer);
	}

	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, const FRHITextureSRVCreateInfo& CreateInfo) final override
	{
		// this-> is required to avoid calling the global version of this function
		return this->RHICreateShaderResourceView(Texture, CreateInfo);
	}

	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, uint32 Stride, uint8 Format) final override
	{
		// this-> is required to avoid calling the global version of this function
		return this->RHICreateShaderResourceView(Buffer, Stride, Format);
	}

	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* StructuredBuffer) final override
	{
		// this-> is required to avoid calling the global version of this function
		return this->RHICreateShaderResourceView(StructuredBuffer);
	}

	void RHICalibrateTimers() override;

#if VULKAN_RHI_RAYTRACING
	virtual FRayTracingAccelerationStructureSize RHICalcRayTracingSceneSize(uint32 MaxInstances, ERayTracingAccelerationStructureFlags Flags) final override;
	virtual FRayTracingAccelerationStructureSize RHICalcRayTracingGeometrySize(const FRayTracingGeometryInitializer& Initializer) final override;
	virtual FRayTracingGeometryRHIRef RHICreateRayTracingGeometry(const FRayTracingGeometryInitializer& Initializer) final override;
	virtual FRayTracingSceneRHIRef RHICreateRayTracingScene(FRayTracingSceneInitializer2 Initializer) final override;
	virtual FRayTracingShaderRHIRef RHICreateRayTracingShader(TArrayView<const uint8> Code, const FSHAHash& Hash, EShaderFrequency ShaderFrequency) final override;
	virtual void RHITransferRayTracingGeometryUnderlyingResource(FRHIRayTracingGeometry* DestGeometry, FRHIRayTracingGeometry* SrcGeometry) final override;
#endif

	// Historical number of times we've presented any and all viewports
	uint32 TotalPresentCount = 0;

	inline const TArray<const ANSICHAR*>& GetInstanceExtensions() const
	{
		return InstanceExtensions;
	}

	inline const TArray<const ANSICHAR*>& GetInstanceLayers() const
	{
		return InstanceLayers;
	}

	static void DestroySwapChain();
	static void RecreateSwapChain(void* NewNativeWindow);

	inline VkInstance GetInstance() const
	{
		return Instance;
	}
	
	inline FVulkanDevice* GetDevice() const
	{
		return Device;
	}

	inline bool SupportsDebugUtilsExt() const
	{
		return (ActiveDebugLayerExtension == EActiveDebugLayerExtension::DebugUtilsExtension);
	}

	inline const FOptionalVulkanInstanceExtensions& GetOptionalExtensions() const
	{
		return OptionalInstanceExtensions;
	}

	void VulkanSetImageLayout( VkCommandBuffer CmdBuffer, VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange );

	virtual void* RHILockStagingBuffer(FRHIStagingBuffer* StagingBuffer, FRHIGPUFence* Fence, uint32 Offset, uint32 SizeRHI) final override;
	virtual void RHIUnlockStagingBuffer(FRHIStagingBuffer* StagingBuffer) final override;

	bool RHIRequiresComputeGenerateMips() const override { return true; };

	inline TArray<FVulkanViewport*>& GetViewports()
	{
		return Viewports;
	}

public:
	static void SavePipelineCache();

private:
	void PooledUniformBuffersBeginFrame();
	void ReleasePooledUniformBuffers();

protected:
	VkInstance Instance;
	TArray<const ANSICHAR*> InstanceExtensions;
	TArray<const ANSICHAR*> InstanceLayers;

	TArray<FVulkanDevice*> Devices;

	FVulkanDevice* Device;

	/** A list of all viewport RHIs that have been created. */
	TArray<FVulkanViewport*> Viewports;

	/** The viewport which is currently being drawn. */
	TRefCountPtr<FVulkanViewport> DrawingViewport;

	void CreateInstance();
	void SelectDevice();
	void InitGPU(FVulkanDevice* Device);
	void InitDevice(FVulkanDevice* Device);

	friend class FVulkanCommandListContext;

	friend class FVulkanViewport;

	/*
	static void WriteEndFrameTimestamp(void*);
	*/

	TArray<const ANSICHAR*> SetupInstanceLayers(FVulkanInstanceExtensionArray& UEExtensions);

	IConsoleObject* SavePipelineCacheCmd = nullptr;
	IConsoleObject* RebuildPipelineCacheCmd = nullptr;
#if VULKAN_SUPPORTS_VALIDATION_CACHE
	IConsoleObject* SaveValidationCacheCmd = nullptr;
#endif

	static void RebuildPipelineCache();
#if VULKAN_SUPPORTS_VALIDATION_CACHE
	static void SaveValidationCache();
#endif

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	IConsoleObject* DumpMemoryCmd = nullptr;
	IConsoleObject* DumpMemoryFullCmd = nullptr;
	IConsoleObject* DumpStagingMemoryCmd = nullptr;
	IConsoleObject* DumpLRUCmd = nullptr;
	IConsoleObject* TrimLRUCmd = nullptr;
public:
	static void DumpMemory();
	static void DumpMemoryFull();
	static void DumpStagingMemory();
	static void DumpLRU();
	static void TrimLRU();
#endif

public:
	enum class EActiveDebugLayerExtension
	{
		None,
		GfxReconstructLayer,
		VkTraceLayer,
		DebugUtilsExtension,
		DebugReportExtension
	};

protected:
	bool bIsStandaloneStereoDevice = false;

	EActiveDebugLayerExtension ActiveDebugLayerExtension = EActiveDebugLayerExtension::None;

#if VULKAN_HAS_DEBUGGING_ENABLED
#if VULKAN_SUPPORTS_DEBUG_UTILS
	VkDebugUtilsMessengerEXT Messenger = VK_NULL_HANDLE;
#endif
	VkDebugReportCallbackEXT MsgCallback = VK_NULL_HANDLE;

	void SetupDebugLayerCallback();
	void RemoveDebugLayerCallback();
#endif

	FCriticalSection LockBufferCS;

	void InternalUnlockTexture2D(bool bFromRenderingThread, FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail);
	void InternalUpdateTexture2D(FRHICommandListBase& RHICmdList, FRHITexture2D* TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData);
	void InternalUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData);

	void UpdateUniformBuffer(FRHICommandListBase& RHICmdList, FVulkanUniformBuffer* UniformBuffer, const void* Contents);

	static void SetupValidationRequests();

	FOptionalVulkanInstanceExtensions OptionalInstanceExtensions;

public:
	static TSharedPtr< IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe > HMDVulkanExtensions;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

/** Implements the Vulkan module as a dynamic RHI providing module. */
class FVulkanDynamicRHIModule : public IDynamicRHIModule
{
public:
	// IDynamicRHIModule
	virtual bool IsSupported() override;

	virtual FDynamicRHI* CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::Num) override;
};
