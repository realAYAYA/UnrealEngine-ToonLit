// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIDevice_hpp
#define MTIDevice_hpp

#include "imp_Device.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTIDeviceTrace : public IMPTable<id<MTLDevice>, MTIDeviceTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLDevice>, MTIDeviceTrace> Super;
	
	MTIDeviceTrace()
	{
	}
	
	MTIDeviceTrace(id<MTLDevice> C)
	: IMPTable<id<MTLDevice>, MTIDeviceTrace>(object_getClass(C))
	{
	}
	
	static id<MTLDevice> Register(id<MTLDevice> d);
	
	static id<MTLCommandQueue> NewCommandQueueImpl(id, SEL, Super::NewCommandQueueType::DefinedIMP);
	static id<MTLCommandQueue> NewCommandQueueWithMaxCommandBufferCountImpl(id, SEL, Super::NewCommandQueueWithMaxCommandBufferCountType::DefinedIMP, NSUInteger);
	static id<MTLHeap> NewHeapWithDescriptorImpl(id, SEL, Super::NewHeapWithDescriptorType::DefinedIMP, MTLHeapDescriptor*);
	static id<MTLBuffer> NewBufferWithLengthImpl(id, SEL, Super::NewBufferWithLengthType::DefinedIMP, NSUInteger, MTLResourceOptions);
	static id<MTLBuffer> NewBufferWithBytesImpl(id, SEL, Super::NewBufferWithBytesType::DefinedIMP, const void*, NSUInteger, MTLResourceOptions);
	static id<MTLBuffer> NewBufferWithBytesNoCopyImpl(id, SEL, Super::NewBufferWithBytesNoCopyType::DefinedIMP, const void*, NSUInteger, MTLResourceOptions, void (^ __nullable)(void *pointer, NSUInteger length));
	static id<MTLDepthStencilState> NewDepthStencilStateWithDescriptorImpl(id, SEL, Super::NewDepthStencilStateWithDescriptorType::DefinedIMP, MTLDepthStencilDescriptor*);
	static id<MTLTexture> NewTextureWithDescriptorImpl(id, SEL, Super::NewTextureWithDescriptorType::DefinedIMP, MTLTextureDescriptor*);
	static id<MTLTexture> NewTextureWithDescriptorIOSurfaceImpl(id, SEL, Super::NewTextureWithDescriptorIOSurfaceType::DefinedIMP, MTLTextureDescriptor*, IOSurfaceRef, NSUInteger);
	static id<MTLSamplerState> NewSamplerStateWithDescriptorImpl(id, SEL, Super::NewSamplerStateWithDescriptorType::DefinedIMP, MTLSamplerDescriptor*);
	static id<MTLLibrary> NewDefaultLibraryImpl(id, SEL, Super::NewDefaultLibraryType::DefinedIMP);
	static id<MTLLibrary> NewDefaultLibraryWithBundleImpl(id, SEL, Super::NewDefaultLibraryWithBundleType::DefinedIMP, NSBundle*, __autoreleasing NSError **);
	static id<MTLLibrary> NewLibraryWithFileImpl(id, SEL, Super::NewLibraryWithFileType::DefinedIMP, NSString*, __autoreleasing NSError **);
	static id<MTLLibrary> NewLibraryWithURLImpl(id, SEL, Super::NewLibraryWithURLType::DefinedIMP, NSURL*, __autoreleasing NSError **);
	static id<MTLLibrary> NewLibraryWithDataImpl(id, SEL, Super::NewLibraryWithDataType::DefinedIMP, dispatch_data_t, __autoreleasing NSError **);
	static id<MTLLibrary> NewLibraryWithSourceOptionsErrorImpl(id, SEL, Super::NewLibraryWithSourceOptionsErrorType::DefinedIMP, NSString*, MTLCompileOptions*, NSError**);
	static void NewLibraryWithSourceOptionsCompletionHandlerImpl(id, SEL, Super::NewLibraryWithSourceOptionsCompletionHandlerType::DefinedIMP, NSString*, MTLCompileOptions*, MTLNewLibraryCompletionHandler);
	static id<MTLRenderPipelineState> NewRenderPipelineStateWithDescriptorErrorImpl(id, SEL, Super::NewRenderPipelineStateWithDescriptorErrorType::DefinedIMP, MTLRenderPipelineDescriptor*, NSError**);
	static id<MTLRenderPipelineState> NewRenderPipelineStateWithDescriptorOptionsReflectionErrorImpl(id, SEL, Super::NewRenderPipelineStateWithDescriptorOptionsReflectionErrorType::DefinedIMP, MTLRenderPipelineDescriptor*, MTLPipelineOption, MTLAutoreleasedRenderPipelineReflection*, NSError**);
	static void NewRenderPipelineStateWithDescriptorCompletionHandlerImpl(id, SEL, Super::NewRenderPipelineStateWithDescriptorCompletionHandlerType::DefinedIMP, MTLRenderPipelineDescriptor* , MTLNewRenderPipelineStateCompletionHandler);
	static void NewRenderPipelineStateWithDescriptorOptionsCompletionHandlerImpl(id, SEL, Super::NewRenderPipelineStateWithDescriptorOptionsCompletionHandlerType::DefinedIMP, MTLRenderPipelineDescriptor*, MTLPipelineOption, MTLNewRenderPipelineStateWithReflectionCompletionHandler);
	static id<MTLComputePipelineState> NewComputePipelineStateWithFunctionErrorImpl(id, SEL, Super::NewComputePipelineStateWithFunctionErrorType::DefinedIMP, id<MTLFunction>, NSError**);
	static id<MTLComputePipelineState> NewComputePipelineStateWithFunctionOptionsReflectionErrorImpl(id, SEL, Super::NewComputePipelineStateWithFunctionOptionsReflectionErrorType::DefinedIMP, id<MTLFunction>, MTLPipelineOption, MTLAutoreleasedComputePipelineReflection *, NSError**);
	static void NewComputePipelineStateWithFunctionCompletionHandlerImpl(id, SEL, Super::NewComputePipelineStateWithFunctionCompletionHandlerType::DefinedIMP, id<MTLFunction> , MTLNewComputePipelineStateCompletionHandler);
	static void NewComputePipelineStateWithFunctionOptionsCompletionHandlerImpl(id, SEL, Super::NewComputePipelineStateWithFunctionOptionsCompletionHandlerType::DefinedIMP, id<MTLFunction> , MTLPipelineOption, MTLNewComputePipelineStateWithReflectionCompletionHandler);
	static id<MTLComputePipelineState> NewComputePipelineStateWithDescriptorOptionsReflectionErrorImpl(id, SEL, Super::NewComputePipelineStateWithDescriptorOptionsReflectionErrorType::DefinedIMP, MTLComputePipelineDescriptor*, MTLPipelineOption, MTLAutoreleasedComputePipelineReflection *, NSError**);
	static void NewComputePipelineStateWithDescriptorOptionsCompletionHandlerImpl(id, SEL, Super::NewComputePipelineStateWithDescriptorOptionsCompletionHandlerType::DefinedIMP, MTLComputePipelineDescriptor*, MTLPipelineOption, MTLNewComputePipelineStateWithReflectionCompletionHandler);
	static id<MTLFence> NewFenceImpl(id, SEL, Super::NewFenceType::DefinedIMP);
	static id<MTLArgumentEncoder> NewArgumentEncoderWithArgumentsImpl(id, SEL, Super::NewArgumentEncoderWithArgumentsType::DefinedIMP, NSArray <MTLArgumentDescriptor *> *);

	INTERPOSE_DECLARATION(getDefaultSamplePositionscount, void, MTLPPSamplePosition*, NSUInteger);
	
	INTERPOSE_DECLARATION(newRenderPipelineStateWithTileDescriptoroptionsreflectionerror, id <MTLRenderPipelineState>, MTLTileRenderPipelineDescriptor*, MTLPipelineOption, MTLAutoreleasedRenderPipelineReflection*, NSError**);
	INTERPOSE_DECLARATION( newRenderPipelineStateWithTileDescriptoroptionscompletionHandler, void, MTLTileRenderPipelineDescriptor*, MTLPipelineOption, MTLNewRenderPipelineStateWithReflectionCompletionHandler);
};

MTLPP_END

#endif /* MTIDevice_hpp */

