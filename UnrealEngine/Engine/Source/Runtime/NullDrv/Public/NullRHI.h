// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ResourceArray.h"
#include "Serialization/LargeMemoryData.h"
#include "RHI.h"
#include "RHITypes.h"
#include "Async/TaskGraphInterfaces.h"

struct Rect;

/** A null implementation of the dynamically bound RHI. */
class FNullDynamicRHI : public FDynamicRHIPSOFallback, public IRHICommandContextPSOFallback
{
public:

	FNullDynamicRHI();

	// FDynamicRHI interface.
	virtual void Init();
	virtual void Shutdown();
	virtual const TCHAR* GetName() override { return TEXT("Null"); }
	virtual ERHIInterfaceType GetInterfaceType() const override { return ERHIInterfaceType::Null; }

	virtual FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer) final override
	{ 
		return new FRHISamplerState(); 
	}
	virtual FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) final override
	{ 
		return new FRHIRasterizerState(); 
	}
	virtual FDepthStencilStateRHIRef RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer) final override
	{ 
		return new FRHIDepthStencilState(); 
	}
	virtual FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer) final override
	{ 
		return new FRHIBlendState(); 
	}
	virtual FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements) final override
	{ 
		return new FRHIVertexDeclaration(); 
	}

	virtual FPixelShaderRHIRef RHICreatePixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override
	{ 
		return new FRHIPixelShader(); 
	}

	virtual FVertexShaderRHIRef RHICreateVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override
	{ 
		return new FRHIVertexShader(); 
	}

	virtual FGeometryShaderRHIRef RHICreateGeometryShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override
	{ 
		return new FRHIGeometryShader(); 
	}

	virtual FComputeShaderRHIRef RHICreateComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override
	{ 
		return new FRHIComputeShader(); 
	}


	virtual FBoundShaderStateRHIRef RHICreateBoundShaderState(FRHIVertexDeclaration* VertexDeclaration, FRHIVertexShader* VertexShader, FRHIPixelShader* PixelShader, FRHIGeometryShader* GeometryShader) final override
	{ 
		return new FRHIBoundShaderState(); 
	}

	virtual void RHISetComputeShader(FRHIComputeShader* ComputeShader) final override
	{

	}

	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override
	{

	}

	virtual void RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override
	{

	}

	virtual void RHICreateTransition(FRHITransition* Transition, const FRHITransitionCreateInfo& CreateInfo) final override
	{
	}

	virtual void RHIReleaseTransition(FRHITransition* Transition) final override
	{
	}

	virtual void RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions) final override
	{
	}

	virtual void RHIEndTransitions(TArrayView<const FRHITransition*> Transitions) final override
	{
	}

	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) final override
	{

	}

	virtual FUniformBufferRHIRef RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout* Layout, EUniformBufferUsage Usage, EUniformBufferValidation Validation) final override
	{ 
		return new FRHIUniformBuffer(Layout); 
	}

	virtual void RHIUpdateUniformBuffer(FRHICommandListBase& RHICmdList, FRHIUniformBuffer* UniformBufferRHI, const void* Contents) final override
	{

	}

	virtual FBufferRHIRef RHICreateBuffer(FRHICommandListBase& RHICmdList, FRHIBufferDesc const& Desc, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo) final override
	{ 
		if(CreateInfo.ResourceArray) 
		{ 
			CreateInfo.ResourceArray->Discard(); 
		} 
		return new FRHIBuffer(Desc);
	}

	virtual void RHICopyBuffer(FRHIBuffer* SourceBuffer, FRHIBuffer* DestBuffer) final override
	{

	}

	virtual void* LockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode) final override
	{
		return GetStaticBuffer(Buffer->GetSize());
	}

	virtual void UnlockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer) final override
	{

	}

	virtual void RHITransferBufferUnderlyingResource(FRHICommandListBase& RHICmdList, FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer) final override
	{

	}

	virtual void RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values) final override
	{

	}

	virtual void RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values) final override
	{

	}

	virtual FRHICalcTextureSizeResult RHICalcTexturePlatformSize(FRHITextureDesc const& Desc, uint32 FirstMipIndex) override final
	{
		return {};
	}

	virtual void RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats) final override
	{

	}

	virtual bool RHIGetTextureMemoryVisualizeData(FColor* TextureData,int32 SizeX,int32 SizeY,int32 Pitch,int32 PixelSize) final override
	{ 
		return false; 
	}

	class FNullTexture : public FRHITexture
	{
	public:
		FNullTexture(const FRHITextureCreateDesc& InDesc)
			: FRHITexture(InDesc)
		{}
	};

	virtual FTextureRHIRef RHICreateTexture(FRHICommandListBase&, const FRHITextureCreateDesc& CreateDesc) final override
	{
		return new FNullTexture(CreateDesc);
	}

	virtual FTextureRHIRef RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ERHIAccess InResourceState, void** InitialMipData, uint32 NumInitialMips, const TCHAR* DebugName, FGraphEventRef& OutCompletionEvent) final override
	{ 
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(DebugName, SizeX, SizeY, (EPixelFormat)Format)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(Flags)
			.SetNumMips(NumMips)
			.SetInitialState(InResourceState);
		OutCompletionEvent = nullptr;
		return new FNullTexture(Desc);
	}

	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(class FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc)
	{
		return new FRHIShaderResourceView(Resource, ViewDesc);
	}

	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(class FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc)
	{
		return new FRHIUnorderedAccessView(Resource, ViewDesc);
	}

	virtual void RHIGenerateMips(FRHITexture* Texture) final override
	{

	}
	virtual uint32 RHIComputeMemorySize(FRHITexture* TextureRHI) final override
	{ 
		return 0; 
	}
	virtual FTexture2DRHIRef RHIAsyncReallocateTexture2D(FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus) final override
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("FNullDynamicRHI::RHIAsyncReallocateTexture2D"), NewSizeX, NewSizeY, Texture2D->GetFormat())
			.SetClearValue(Texture2D->GetClearBinding())
			.SetFlags(Texture2D->GetFlags())
			.SetNumMips(NewMipCount)
			.SetNumSamples(Texture2D->GetNumSamples());

		return new FNullTexture(Desc);
	}
	virtual ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override
	{ 
		return TexRealloc_Succeeded; 
	}
	virtual ETextureReallocationStatus RHICancelAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override
	{ 
		return TexRealloc_Succeeded; 
	}
	virtual void* RHILockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, uint64* OutLockedByteCount) final override
	{ 
		DestStride = 0; 
		return GetStaticTextureBuffer(Texture->GetSizeX(), Texture->GetSizeY(), Texture->GetFormat(), DestStride, OutLockedByteCount);
	}
	virtual void RHIUnlockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail) final override
	{

	}
	virtual void* RHILockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override
	{ 
		DestStride = 0; 
		return GetStaticTextureBuffer(Texture->GetSizeX(), Texture->GetSizeY(), Texture->GetFormat(), DestStride);
	}
	virtual void RHIUnlockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail) final override
	{

	}
	virtual void RHIUpdateTexture2D(FRHICommandListBase&, FRHITexture2D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData) final override
	{

	}
	virtual void RHIUpdateTexture3D(FRHICommandListBase&, FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData) final override
	{

	}

	virtual void* RHILockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override
	{ 
		DestStride = 0; 
		return GetStaticTextureBuffer(Texture->GetSize(), Texture->GetSize(), Texture->GetFormat(), DestStride);
	}
	virtual void RHIUnlockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) final override
	{
	}

	virtual void RHICopyTexture(FRHITexture* SourceTexture, FRHITexture* DestTexture, const FRHICopyTextureInfo& CopyInfo) final override
	{

	}

	virtual void RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHITexture* Texture, const TCHAR* Name) final override
	{

	}

	virtual void RHIReadSurfaceData(FRHITexture* Texture,FIntRect Rect,TArray<FColor>& OutData,FReadSurfaceDataFlags InFlags) final override
	{ 
		OutData.AddZeroed(Rect.Width() * Rect.Height()); 
	}


	virtual void RHIMapStagingSurface(FRHITexture* Texture, FRHIGPUFence* Fence, void*& OutData, int32& OutWidth, int32& OutHeight, uint32 GPUIndex) final override
	{

	}


	virtual void RHIUnmapStagingSurface(FRHITexture* Texture, uint32 GPUIndex) final override
	{

	}

	virtual void RHIReadSurfaceFloatData(FRHITexture* Texture,FIntRect Rect,TArray<FFloat16Color>& OutData,ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex) final override
	{

	}

	virtual void RHIRead3DSurfaceFloatData(FRHITexture* Texture,FIntRect Rect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData) final override
	{

	}



	virtual FRenderQueryRHIRef RHICreateRenderQuery(ERenderQueryType QueryType) final override
	{ 
		return new FRHIRenderQuery(); 
	}

	virtual void RHIBeginRenderQuery(FRHIRenderQuery* RenderQuery) final override
	{

	}
	virtual void RHIEndRenderQuery(FRHIRenderQuery* RenderQuery) final override
	{

	}


	virtual bool RHIGetRenderQueryResult(FRHIRenderQuery* RenderQuery, uint64& OutResult, bool bWait, uint32 GPUIndex = INDEX_NONE) final override
	{ 
		return true; 
	}

	virtual void RHISubmitCommandsHint() final override
	{
	}


	virtual void RHIBeginDrawingViewport(FRHIViewport* Viewport, FRHITexture* RenderTargetRHI) final override
	{
	}

	virtual void RHIEndDrawingViewport(FRHIViewport* Viewport, bool bPresent, bool bLockToVsync) final override
	{
	}

	virtual FTexture2DRHIRef RHIGetViewportBackBuffer(FRHIViewport* Viewport) final override
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("FNullDynamicRHI::RHIGetViewportBackBuffer"), 1, 1, PF_B8G8R8A8)
			.SetFlags(ETextureCreateFlags::RenderTargetable);

		return new FNullTexture(Desc);
	}

	using FDynamicRHI::RHIBeginFrame;
	virtual void RHIBeginFrame() final override
	{

	}


	virtual void RHIEndFrame() final override
	{

	}
	virtual void RHIBeginScene() final override
	{

	}
	virtual void RHIEndScene() final override
	{

	}
	virtual void RHIAliasTextureResources(FTextureRHIRef& DestTexture, FTextureRHIRef& SrcTexture) final override
	{

	}
	virtual void RHIAdvanceFrameForGetViewportBackBuffer(FRHIViewport* Viewport) final override
	{

	}
	virtual void RHIAcquireThreadOwnership() final override
	{

	}
	virtual void RHIReleaseThreadOwnership() final override
	{

	}


	virtual void RHIFlushResources() final override
	{

	}

	virtual uint32 RHIGetGPUFrameCycles(uint32 GPUIndex = 0) final override
	{ 
		return 0; 
	}

	virtual FViewportRHIRef RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) final override
	{ 
		return new FRHIViewport(); 
	}
	virtual void RHIResizeViewport(FRHIViewport* Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen) final override
	{

	}

	virtual EColorSpaceAndEOTF RHIGetColorSpace(FRHIViewport* Viewport ) final override
	{
		return EColorSpaceAndEOTF::EColorSpace_Rec709;
	}

	virtual void RHICheckViewportHDRStatus(FRHIViewport* Viewport) final override
	{
	}

	virtual void RHITick(float DeltaTime) final override
	{

	}

	virtual void RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, uint32 Offset) final override
	{
	}

	virtual void RHISetRasterizerState(FRHIRasterizerState* NewState) final override
	{

	}

	virtual void RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ) final override
	{

	}

	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) final override
	{
	}

	virtual void RHISetBoundShaderState(FRHIBoundShaderState* BoundShaderState) final override
	{
	}

	virtual void RHISetShaderParameters(FRHIGraphicsShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters) final override
	{
	}

	virtual void RHISetShaderParameters(FRHIComputeShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters) final override
	{
	}

	virtual void RHISetDepthStencilState(FRHIDepthStencilState* NewState, uint32 StencilRef) final override
	{

	}

	virtual void RHISetBlendState(FRHIBlendState* NewState, const FLinearColor& BlendFactor) final override
	{

	}

	virtual void RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
	{
	}

	virtual void RHIEndRenderPass()
	{
	}

	virtual void RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) final override
	{

	}
	virtual void RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override
	{

	}

	virtual void RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) final override
	{

	}


	virtual void RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override
	{

	}

	virtual void RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBuffer, FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override
	{

	}

	virtual void RHIMultiDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBuffer, FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset, FRHIBuffer* CountBuffer, uint32 CountBufferOffset, uint32 MaxDrawArguments) final override
	{

	}

	virtual void RHIBlockUntilGPUIdle() final override
	{
	}
	virtual bool RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate) final override
	{ 
		return false; 
	}
	virtual void RHIGetSupportedResolution(uint32& Width, uint32& Height) final override
	{

	}
	virtual void RHIEnableDepthBoundsTest(bool bEnable) final override
	{
	}
	virtual void RHISetDepthBounds(float MinDepth, float MaxDepth) final override
	{
	}
	virtual void RHISetShadingRate(EVRSShadingRate ShadingRate, EVRSRateCombiner Combiner) final override
	{
	}
	virtual void* RHIGetNativeDevice() final override
	{ 
		return 0; 
	}
	virtual void* RHIGetNativeInstance() final override
	{
		return 0;
	}
	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) final override
	{
	}
	virtual void RHIPopEvent()
	{
	}
	virtual class IRHICommandContext* RHIGetDefaultContext() final override
	{ 
		return this; 
	}

	virtual IRHIComputeContext* RHIGetCommandContext(ERHIPipeline Pipeline, FRHIGPUMask GPUMask) final override
	{
		return nullptr;
	}

	virtual IRHIPlatformCommandList* RHIFinalizeContext(IRHIComputeContext* Context) final override
	{
		return nullptr;
	}

	virtual void RHISubmitCommandLists(TArrayView<IRHIPlatformCommandList*> CommandLists, bool bFlushResources) final override
	{
	}

private:
	FLargeMemoryData MemoryBuffer;

	/** Allocates a static buffer for RHI functions to return as a write destination. */
	void* GetStaticBuffer(size_t Size);
	void* GetStaticTextureBuffer(int32 SizeX, int32 SizeY, EPixelFormat Format, uint32& DestStride, uint64* OutLockedByteCount = nullptr);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_3
#include "CoreMinimal.h"
#endif
