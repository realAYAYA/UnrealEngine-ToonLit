// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTIDevice.hpp"
#include "MTIBuffer.hpp"
#include "MTITexture.hpp"
#include "MTICommandQueue.hpp"
#include "MTIArgumentEncoder.hpp"
#include "MTIHeap.hpp"
#include "MTILibrary.hpp"
#include "MTIRenderPipeline.hpp"
#include "MTIComputePipeline.hpp"
#include "MTISampler.hpp"
#include "MTIFence.hpp"
#include "MTIDepthStencil.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

#define DYLD_INTERPOSE(_replacment,_replacee) \
__attribute__((used)) struct{ const void* replacment; const void* replacee; } _interpose_##_replacee \
__attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacment, (const void*)(unsigned long)&_replacee };

INTERPOSE_PROTOCOL_REGISTER(MTIDeviceTrace, id<MTLDevice>);

struct MTITraceDefaultDeviceCommand : public MTITraceCommand
{
	uintptr_t Device;
};

std::fstream& operator>>(std::fstream& fs, MTITraceDefaultDeviceCommand& dt)
{
	fs >> dt.Device;
	return fs;
}

std::fstream& operator<<(std::fstream& fs, const MTITraceDefaultDeviceCommand& dt)
{
	fs << dt.Device;
	return fs;
}

struct MTITraceDefaultDeviceCommandHandler : public MTITraceCommandHandler
{
	MTITraceDefaultDeviceCommandHandler()
	: MTITraceCommandHandler("", "MTLCreateSystemDefaultDevice")
	{
		
	}
	
	id <MTLDevice> __nullable Trace(id <MTLDevice> __nullable Device)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, 0);
		MTITraceDefaultDeviceCommand Command;
		Command.Device = (uintptr_t)Device;
		fs << Command;
		MTITrace::Get().EndWrite();
		return Device;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTITraceDefaultDeviceCommand Command;
		fs >> Command;
		id Object = MTLCreateSystemDefaultDevice();
		MTITrace::Get().RegisterObject(Command.Device, Object);
	}
};
static MTITraceDefaultDeviceCommandHandler GMTITraceDefaultDeviceCommandHandler;

MTL_EXTERN id <MTLDevice> __nullable MTLTCreateSystemDefaultDevice(void)
{
	return GMTITraceDefaultDeviceCommandHandler.Trace(MTIDeviceTrace::Register(MTLCreateSystemDefaultDevice()));
}
DYLD_INTERPOSE(MTLTCreateSystemDefaultDevice, MTLCreateSystemDefaultDevice)

struct MTITraceCopyDeviceCommand : public MTITraceCommand
{
	unsigned Num;
	NSArray* Devices;
	std::vector<uintptr_t> Backing;
};

std::fstream& operator>>(std::fstream& fs, MTITraceCopyDeviceCommand& dt)
{
	fs >> dt.Num;
	if (dt.Num)
	{
		dt.Backing.resize(dt.Num);
		for (unsigned i = 0; i < dt.Num; i++)
		{
			fs >> dt.Backing[i];
		}
	}
	return fs;
}

std::fstream& operator<<(std::fstream& fs, const MTITraceCopyDeviceCommand& dt)
{
	fs << dt.Num;
	for (id Device in dt.Devices)
	{
		fs << (uintptr_t)Device;
	}
	return fs;
}

struct MTITraceCopyDeviceCommandHandler : public MTITraceCommandHandler
{
	MTITraceCopyDeviceCommandHandler()
	: MTITraceCommandHandler("", "MTLCopyAllDevices")
	{
		
	}
	
	NSArray <id<MTLDevice>> * Trace(NSArray <id<MTLDevice>> * Devices)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, 0);
		MTITraceCopyDeviceCommand Command;
		Command.Num = (unsigned)Devices.count;
		Command.Devices = Devices;
		fs << Command;
		MTITrace::Get().EndWrite();
		return Devices;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTITraceCopyDeviceCommand Command;
		fs >> Command;
		
		NSArray <id<MTLDevice>> * Devices = MTLCopyAllDevices();
		for (unsigned i = 0; i < Devices.count && i < Command.Num; i++)
		{
			MTITrace::Get().RegisterObject(Command.Backing[i], Devices[i]);
		}
	}
};
static MTITraceCopyDeviceCommandHandler GMTITraceCopyDeviceCommandHandler;

MTL_EXTERN NSArray <id<MTLDevice>> *MTLTCopyAllDevices(void)
{
	NSArray <id<MTLDevice>> * Devices = GMTITraceCopyDeviceCommandHandler.Trace(MTLCopyAllDevices());
	for (id<MTLDevice> Device in Devices)
	{
		MTIDeviceTrace::Register(Device);
	}
	return Devices;
}
DYLD_INTERPOSE(MTLTCopyAllDevices, MTLCopyAllDevices)

struct MTITraceNewCommandQueueHandler : public MTITraceCommandHandler
{
	MTITraceNewCommandQueueHandler()
	: MTITraceCommandHandler("MTLDevice", "newCommandQueue")
	{
		
	}
	
	id<MTLCommandQueue> Trace(id Object, id<MTLCommandQueue> Queue)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);

		fs << (uintptr_t)Queue;

		MTITrace::Get().EndWrite();
		return Queue;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Result;
		fs >> Result;
		
		id<MTLCommandQueue> Queue = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newCommandQueue];
		assert(Queue);
		
		MTITrace::Get().RegisterObject(Result, Queue);
	}
};
static MTITraceNewCommandQueueHandler GMTITraceNewCommandQueueHandler;

id<MTLCommandQueue> MTIDeviceTrace::NewCommandQueueImpl(id Object, SEL Selector, Super::NewCommandQueueType::DefinedIMP Original)
{
	return GMTITraceNewCommandQueueHandler.Trace(Object, MTICommandQueueTrace::Register(Original(Object, Selector)));
}

struct MTITraceNewCommandQueueWithMaxHandler : public MTITraceCommandHandler
{
	MTITraceNewCommandQueueWithMaxHandler()
	: MTITraceCommandHandler("MTLDevice", "newCommandQueueWithMaxCommandBufferCount")
	{
		
	}
	
	id<MTLCommandQueue> Trace(id Object, NSUInteger Num, id<MTLCommandQueue> Queue)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Num;
		fs << (uintptr_t)Queue;
		
		MTITrace::Get().EndWrite();
		return Queue;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger Num;
		fs >> Num;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLCommandQueue> Queue = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newCommandQueueWithMaxCommandBufferCount:Num];
		assert(Queue);
		
		MTITrace::Get().RegisterObject(Result, Queue);
	}
};
static MTITraceNewCommandQueueWithMaxHandler GMTITraceNewCommandQueueWithMaxHandler;

id<MTLCommandQueue> MTIDeviceTrace::NewCommandQueueWithMaxCommandBufferCountImpl(id Object, SEL Selector, Super::NewCommandQueueWithMaxCommandBufferCountType::DefinedIMP Original, NSUInteger Num)
{
	return GMTITraceNewCommandQueueWithMaxHandler.Trace(Object, Num, MTICommandQueueTrace::Register(Original(Object, Selector, Num)));
}



struct MTITraceNewHeapHandler : public MTITraceCommandHandler
{
	MTITraceNewHeapHandler()
	: MTITraceCommandHandler("MTLDevice", "newHeapWithDescriptor")
	{
		
	}
	
	id<MTLHeap> Trace(id Object, MTLHeapDescriptor* Desc, id<MTLHeap> Heap)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Desc.size;
		fs << Desc.storageMode;
		fs << Desc.cpuCacheMode;
		fs << (uintptr_t)Heap;
		
		MTITrace::Get().EndWrite();
		return Heap;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger size, storageMode, cpuCacheMode;
		fs >> size;
		fs >> storageMode;
		fs >> cpuCacheMode;

		MTLHeapDescriptor* Desc = [[MTLHeapDescriptor new] autorelease];
		Desc.size = size;
		Desc.storageMode = (MTLStorageMode)storageMode;
		Desc.cpuCacheMode = (MTLCPUCacheMode)cpuCacheMode;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLHeap> Heap = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newHeapWithDescriptor:Desc];
		assert(Heap);
		
		MTITrace::Get().RegisterObject(Result, Heap);
	}
};
static MTITraceNewHeapHandler GMTITraceNewHeapHandler;

id<MTLHeap> MTIDeviceTrace::NewHeapWithDescriptorImpl(id Object, SEL Selector, Super::NewHeapWithDescriptorType::DefinedIMP Original, MTLHeapDescriptor* Desc)
{
	return GMTITraceNewHeapHandler.Trace(Object, Desc, MTIHeapTrace::Register(Original(Object, Selector, Desc)));
}


struct MTITraceNewBufferHandler : public MTITraceCommandHandler
{
	MTITraceNewBufferHandler()
	: MTITraceCommandHandler("MTLDevice", "newBufferWithLength")
	{
		
	}
	
	id<MTLBuffer> Trace(id Object, NSUInteger Len, MTLResourceOptions Opt, id<MTLBuffer> Buffer)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (NSUInteger)Len;
		fs << (NSUInteger)Opt;
		fs << (uintptr_t)Buffer;
		
		MTITrace::Get().EndWrite();
		return Buffer;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger Len;
		fs >> Len;
		
		NSUInteger Opt;
		fs >> Opt;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLBuffer> Buffer = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newBufferWithLength: Len options:(MTLResourceOptions)Opt];
		assert(Buffer);
		
		MTITrace::Get().RegisterObject(Result, Buffer);
	}
};
static MTITraceNewBufferHandler GMTITraceNewBufferHandler;


id<MTLBuffer> MTIDeviceTrace::NewBufferWithLengthImpl(id Object, SEL Selector, Super::NewBufferWithLengthType::DefinedIMP Original, NSUInteger Len, MTLResourceOptions Opt)
{
	return GMTITraceNewBufferHandler.Trace(Object, Len, Opt, MTIBufferTrace::Register(Original(Object, Selector, Len, Opt)));
}
id<MTLBuffer> MTIDeviceTrace::NewBufferWithBytesImpl(id Object, SEL Selector, Super::NewBufferWithBytesType::DefinedIMP Original, const void* Ptr, NSUInteger Len, MTLResourceOptions Opt)
{
	return MTIBufferTrace::Register(Original(Object, Selector, Ptr, Len, Opt));
}
id<MTLBuffer> MTIDeviceTrace::NewBufferWithBytesNoCopyImpl(id Object, SEL Selector, Super::NewBufferWithBytesNoCopyType::DefinedIMP Original, const void* Ptr, NSUInteger Len, MTLResourceOptions Opt, void (^__nullable Block)(void *pointer, NSUInteger length))
{
	return MTIBufferTrace::Register(Original(Object, Selector, Ptr, Len, Opt, Block));
}


struct MTITraceNewDepthStencilDescHandler : public MTITraceCommandHandler
{
	MTITraceNewDepthStencilDescHandler()
	: MTITraceCommandHandler("", "newDepthStencilDescriptor")
	{
		
	}
	
	MTLDepthStencilDescriptor* Trace(MTLDepthStencilDescriptor* Desc)
	{
		if (!MTITrace::Get().FetchObject((uintptr_t)Desc))
		{
			std::fstream& fs = MTITrace::Get().BeginWrite();
			MTITraceCommandHandler::Trace(fs, (uintptr_t)Desc);
			
			fs << Desc.depthCompareFunction;
			fs << Desc.depthWriteEnabled;
			fs << Desc.frontFaceStencil.stencilCompareFunction;
			fs << Desc.frontFaceStencil.stencilFailureOperation;
			fs << Desc.frontFaceStencil.depthFailureOperation;
			fs << Desc.frontFaceStencil.depthStencilPassOperation;
			fs << Desc.frontFaceStencil.readMask;
			fs << Desc.frontFaceStencil.writeMask;
			fs << Desc.backFaceStencil.stencilCompareFunction;
			fs << Desc.backFaceStencil.stencilFailureOperation;
			fs << Desc.backFaceStencil.depthFailureOperation;
			fs << Desc.backFaceStencil.depthStencilPassOperation;
			fs << Desc.backFaceStencil.readMask;
			fs << Desc.backFaceStencil.writeMask;
			fs << MTIString(Desc.label ? [Desc.label UTF8String] : "");

			MTITrace::Get().RegisterObject((uintptr_t)Desc, Desc);
			MTITrace::Get().EndWrite();
		}
		return Desc;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger depthCompareFunction;
		BOOL depthWriteEnabled;
		NSUInteger frontFaceStencilstencilCompareFunction;
		NSUInteger frontFaceStencilstencilFailureOperation;
		NSUInteger frontFaceStencildepthFailureOperation;
		NSUInteger frontFaceStencildepthStencilPassOperation;
		uint32_t frontFaceStencilreadMask;
		uint32_t frontFaceStencilwriteMask;
		NSUInteger backFaceStencilstencilCompareFunction;
		NSUInteger backFaceStencilstencilFailureOperation;
		NSUInteger backFaceStencildepthFailureOperation;
		NSUInteger backFaceStencildepthStencilPassOperation;
		uint32_t backFaceStencilreadMask;
		uint32_t backFaceStencilwriteMask;
		MTIString label;
		
		MTLDepthStencilDescriptor* Desc = [MTLDepthStencilDescriptor new];
		fs >> depthCompareFunction;
		fs >> depthWriteEnabled;
		fs >> frontFaceStencilstencilCompareFunction;
		fs >> frontFaceStencilstencilFailureOperation;
		fs >> frontFaceStencildepthFailureOperation;
		fs >> frontFaceStencildepthStencilPassOperation;
		fs >> frontFaceStencilreadMask;
		fs >> frontFaceStencilwriteMask;
		fs >> backFaceStencilstencilCompareFunction;
		fs >> backFaceStencilstencilFailureOperation;
		fs >> backFaceStencildepthFailureOperation;
		fs >> backFaceStencildepthStencilPassOperation;
		fs >> backFaceStencilreadMask;
		fs >> backFaceStencilwriteMask;
		fs >> label;
		
		Desc.depthCompareFunction = 						(MTLCompareFunction)depthCompareFunction;
		Desc.depthWriteEnabled = 							depthWriteEnabled;
		Desc.frontFaceStencil.stencilCompareFunction = 		(MTLCompareFunction)frontFaceStencilstencilCompareFunction;
		Desc.frontFaceStencil.stencilFailureOperation = 	(MTLStencilOperation)frontFaceStencilstencilFailureOperation;
		Desc.frontFaceStencil.depthFailureOperation = 		(MTLStencilOperation)frontFaceStencildepthFailureOperation;
		Desc.frontFaceStencil.depthStencilPassOperation = 	(MTLStencilOperation)frontFaceStencildepthStencilPassOperation;
		Desc.frontFaceStencil.readMask = 					frontFaceStencilreadMask;
		Desc.frontFaceStencil.writeMask = 					frontFaceStencilwriteMask;
		Desc.backFaceStencil.stencilCompareFunction = 		(MTLCompareFunction)backFaceStencilstencilCompareFunction;
		Desc.backFaceStencil.stencilFailureOperation = 		(MTLStencilOperation)backFaceStencilstencilFailureOperation;
		Desc.backFaceStencil.depthFailureOperation = 		(MTLStencilOperation)backFaceStencildepthFailureOperation;
		Desc.backFaceStencil.depthStencilPassOperation = 	(MTLStencilOperation)backFaceStencildepthStencilPassOperation;
		Desc.backFaceStencil.readMask = 					backFaceStencilreadMask;
		Desc.backFaceStencil.writeMask = 					backFaceStencilwriteMask;
		Desc.label = [NSString stringWithUTF8String:label.c_str()];
		
		MTITrace::Get().RegisterObject(Header.Receiver, Desc);
	}
};
static MTITraceNewDepthStencilDescHandler GMTITraceNewDepthStencilDescHandler;

struct MTITraceNewDepthStencilStateHandler : public MTITraceCommandHandler
{
	MTITraceNewDepthStencilStateHandler()
	: MTITraceCommandHandler("MTLDevice", "newDepthStencilStateWithDescriptor")
	{
		
	}
	
	id<MTLDepthStencilState> Trace(id Object, MTLDepthStencilDescriptor* Desc, id<MTLDepthStencilState> State)
	{
		GMTITraceNewDepthStencilDescHandler.Trace(Desc);
		
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Desc;
		fs << (uintptr_t)State;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Desc;
		fs >> Desc;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLDepthStencilState> State = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newDepthStencilStateWithDescriptor:(MTLDepthStencilDescriptor*)MTITrace::Get().FetchObject(Desc)];
		assert(State);
		
		MTITrace::Get().RegisterObject(Result, State);
	}
};
static MTITraceNewDepthStencilStateHandler GMTITraceNewDepthStencilStateHandler;
id<MTLDepthStencilState> MTIDeviceTrace::NewDepthStencilStateWithDescriptorImpl(id Object, SEL Selector, Super::NewDepthStencilStateWithDescriptorType::DefinedIMP Original, MTLDepthStencilDescriptor* Desc)
{
	return GMTITraceNewDepthStencilStateHandler.Trace(Object, Desc, MTIDepthStencilStateTrace::Register(Original(Object, Selector, Desc)));
}


struct MTITraceNewTextureHandler : public MTITraceCommandHandler
{
	MTITraceNewTextureHandler()
	: MTITraceCommandHandler("MTLDevice", "newTextureWithDescriptor")
	{
		
	}
	
	id<MTLTexture> Trace(id Object, MTLTextureDescriptor* Desc, id<MTLTexture> Texture)
	{
		GMTITraceNewTextureDescHandler.Trace(Desc);
		
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Desc;
		fs << (uintptr_t)Texture;
		
		MTITrace::Get().EndWrite();
		return Texture;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Desc;
		fs >> Desc;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLTexture> Texture = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newTextureWithDescriptor:(MTLTextureDescriptor*)MTITrace::Get().FetchObject(Desc)];
		assert(Texture);
		
		MTITrace::Get().RegisterObject(Result, Texture);
	}
};
static MTITraceNewTextureHandler GMTITraceNewTextureHandler;

id<MTLTexture> MTIDeviceTrace::NewTextureWithDescriptorImpl(id Object, SEL Selector, Super::NewTextureWithDescriptorType::DefinedIMP Original, MTLTextureDescriptor* Desc)
{
	return GMTITraceNewTextureHandler.Trace(Object, Desc, MTITextureTrace::Register(Original(Object, Selector, Desc)));
}
id<MTLTexture> MTIDeviceTrace::NewTextureWithDescriptorIOSurfaceImpl(id Object, SEL Selector, Super::NewTextureWithDescriptorIOSurfaceType::DefinedIMP Original, MTLTextureDescriptor* Desc, IOSurfaceRef IO, NSUInteger Plane)
{
	return MTITextureTrace::Register(Original(Object, Selector, Desc, IO, Plane));
}


struct MTITraceNewSamplerDescHandler : public MTITraceCommandHandler
{
	MTITraceNewSamplerDescHandler()
	: MTITraceCommandHandler("", "newSampleDescriptor")
	{
		
	}
	
	MTLSamplerDescriptor* Trace(MTLSamplerDescriptor* Desc)
	{
		if (!MTITrace::Get().FetchObject((uintptr_t)Desc))
		{
			std::fstream& fs = MTITrace::Get().BeginWrite();
			MTITraceCommandHandler::Trace(fs, (uintptr_t)Desc);
			
			fs << Desc.minFilter;
			fs << Desc.magFilter;
			fs << Desc.mipFilter;
			fs << Desc.maxAnisotropy;
			fs << Desc.sAddressMode;
			fs << Desc.tAddressMode;
			fs << Desc.rAddressMode;
			fs << Desc.borderColor;
			fs << Desc.normalizedCoordinates;
			fs << Desc.lodMinClamp;
			fs << Desc.lodMaxClamp;
			fs << Desc.compareFunction;
			fs << Desc.supportArgumentBuffers;
			fs << MTIString(Desc.label ? [Desc.label UTF8String] : "");

			MTITrace::Get().RegisterObject((uintptr_t)Desc, Desc);
			MTITrace::Get().EndWrite();
		}
		return Desc;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger minFilter;
		NSUInteger magFilter;
		NSUInteger mipFilter;
		NSUInteger maxAnisotropy;
		NSUInteger sAddressMode;
		NSUInteger tAddressMode;
		NSUInteger rAddressMode;
		NSUInteger borderColor;
		BOOL normalizedCoordinates;
		float lodMinClamp;
		float lodMaxClamp;
		NSUInteger compareFunction;
		BOOL supportArgumentBuffers;
		MTIString label;
		
		
		MTLSamplerDescriptor* Desc = [MTLSamplerDescriptor new];
		fs >> minFilter;
		fs >> magFilter;
		fs >> mipFilter;
		fs >> maxAnisotropy;
		fs >> sAddressMode;
		fs >> tAddressMode;
		fs >> rAddressMode;
		fs >> borderColor;
		fs >> normalizedCoordinates;
		fs >> lodMinClamp;
		fs >> lodMaxClamp;
		fs >> compareFunction;
		fs >> supportArgumentBuffers;
		fs >> label;
		
		Desc.minFilter = (MTLSamplerMinMagFilter)minFilter;
		Desc.magFilter = (MTLSamplerMinMagFilter)magFilter;
		Desc.mipFilter = (MTLSamplerMipFilter)mipFilter;
		Desc.maxAnisotropy = maxAnisotropy;
		Desc.sAddressMode = (MTLSamplerAddressMode)sAddressMode;
		Desc.tAddressMode = (MTLSamplerAddressMode)tAddressMode;
		Desc.rAddressMode = (MTLSamplerAddressMode)rAddressMode;
		Desc.borderColor = (MTLSamplerBorderColor)borderColor;
		Desc.normalizedCoordinates = normalizedCoordinates;
		Desc.lodMinClamp = lodMinClamp;
		Desc.lodMaxClamp = lodMaxClamp;
		Desc.compareFunction = (MTLCompareFunction)compareFunction;
		Desc.supportArgumentBuffers = supportArgumentBuffers;
		Desc.label = [NSString stringWithUTF8String:label.c_str()];
		
		MTITrace::Get().RegisterObject(Header.Receiver, Desc);
	}
};
static MTITraceNewSamplerDescHandler GMTITraceNewSamplerDescHandler;

struct MTITraceNewSamplerStatHandler : public MTITraceCommandHandler
{
	MTITraceNewSamplerStatHandler()
	: MTITraceCommandHandler("MTLDevice", "newSamplerStateWithDescriptor")
	{
		
	}
	
	id<MTLSamplerState> Trace(id Object, MTLSamplerDescriptor* Desc, id<MTLSamplerState> Sampler)
	{
		GMTITraceNewSamplerDescHandler.Trace(Desc);
		
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Desc;
		fs << (uintptr_t)Sampler;
		
		MTITrace::Get().EndWrite();
		return Sampler;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Desc;
		fs >> Desc;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLSamplerState> Sampler = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newSamplerStateWithDescriptor:(MTLSamplerDescriptor*)MTITrace::Get().FetchObject(Desc)];
		assert(Sampler);
		
		MTITrace::Get().RegisterObject(Result, Sampler);
	}
};
static MTITraceNewSamplerStatHandler GMTITraceNewSamplerStatHandler;


id<MTLSamplerState> MTIDeviceTrace::NewSamplerStateWithDescriptorImpl(id Object, SEL Selector, Super::NewSamplerStateWithDescriptorType::DefinedIMP Original, MTLSamplerDescriptor* Desc)
{
	return GMTITraceNewSamplerStatHandler.Trace(Object, Desc, MTISamplerStateTrace::Register(Original(Object, Selector, Desc)));
}




id<MTLLibrary> MTIDeviceTrace::NewDefaultLibraryImpl(id Object, SEL Selector, Super::NewDefaultLibraryType::DefinedIMP Original)
{
	return MTILibraryTrace::Register(Original(Object, Selector));
}
id<MTLLibrary> MTIDeviceTrace::NewDefaultLibraryWithBundleImpl(id Object, SEL Selector, Super::NewDefaultLibraryWithBundleType::DefinedIMP Original, NSBundle* Bndl, __autoreleasing NSError ** Err)
{
	return MTILibraryTrace::Register(Original(Object, Selector, Bndl, Err));
}


struct MTITraceNewLibraryFromPathHandler : public MTITraceCommandHandler
{
	MTITraceNewLibraryFromPathHandler()
	: MTITraceCommandHandler("MTLDevice", "newLibraryWithFile")
	{
		
	}
	
	id<MTLLibrary> Trace(id Object, NSString* Url, id<MTLLibrary> Lib)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << MTIString([Url UTF8String]);
		fs << (uintptr_t)Lib;
		
		MTITrace::Get().EndWrite();
		return Lib;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTIString URL;
		fs >> URL;
		
		uintptr_t Result;
		fs >> Result;
		
		NSString* newURL = [NSString stringWithUTF8String: URL.c_str()];
		
		id<MTLLibrary> Library = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newLibraryWithFile:newURL error:nil];
		assert(Library);
		
		MTITrace::Get().RegisterObject(Result, Library);
	}
};
static MTITraceNewLibraryFromPathHandler GMTITraceNewLibraryFromPathHandler;

id<MTLLibrary> MTIDeviceTrace::NewLibraryWithFileImpl(id Object, SEL Selector, Super::NewLibraryWithFileType::DefinedIMP Original, NSString* Str, __autoreleasing NSError ** Err)
{
	return GMTITraceNewLibraryFromPathHandler.Trace(Object, Str, MTILibraryTrace::Register(Original(Object, Selector, Str, Err)));
}


struct MTITraceNewLibraryFromURLHandler : public MTITraceCommandHandler
{
	MTITraceNewLibraryFromURLHandler()
	: MTITraceCommandHandler("MTLDevice", "newLibraryWithURL")
	{
		
	}
	
	id<MTLLibrary> Trace(id Object, NSURL* Url, id<MTLLibrary> Lib)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << MTIString([[Url path] UTF8String]);
		fs << (uintptr_t)Lib;
		
		MTITrace::Get().EndWrite();
		return Lib;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTIString URL;
		fs >> URL;
		
		uintptr_t Result;
		fs >> Result;
		
		NSURL* newURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String: URL.c_str()]];
		
		id<MTLLibrary> Library = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newLibraryWithURL:newURL error:nil];
		assert(Library);
		
		MTITrace::Get().RegisterObject(Result, Library);
	}
};
static MTITraceNewLibraryFromURLHandler GMTITraceNewLibraryFromURLHandler;

id<MTLLibrary> MTIDeviceTrace::NewLibraryWithURLImpl(id Object, SEL Selector, Super::NewLibraryWithURLType::DefinedIMP Original, NSURL* Url, __autoreleasing NSError ** Err)
{
	return GMTITraceNewLibraryFromURLHandler.Trace(Object, Url, MTILibraryTrace::Register(Original(Object, Selector, Url, Err)));
}

struct MTITraceNewLibraryFromDataHandler : public MTITraceCommandHandler
{
	MTITraceNewLibraryFromDataHandler()
	: MTITraceCommandHandler("MTLDevice", "newLibraryWithData")
	{
		
	}
	
	id<MTLLibrary> Trace(id Object, dispatch_data_t Data, id<MTLLibrary> Lib)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		MTITraceArray<uint8> Array;
		dispatch_data_t Temp = dispatch_data_create_map(Data,
								 (const void**)&Array.Data,
								 (size_t*)&Array.Length);
		
		fs << Array;
		fs << (uintptr_t)Lib;
		
		dispatch_release(Temp);
		
		MTITrace::Get().EndWrite();
		return Lib;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTITraceArray<uint8> Data;
		fs >> Data;
		
		uintptr_t Result;
		fs >> Result;
		
		dispatch_data_t Obj = dispatch_data_create(Data.Backing.data(), Data.Length, nullptr, nullptr);
		
		id<MTLLibrary> Library = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newLibraryWithData:Obj error:nil];
		assert(Library);
		
		dispatch_release(Obj);
		
		MTITrace::Get().RegisterObject(Result, Library);
	}
};
static MTITraceNewLibraryFromDataHandler GMTITraceNewLibraryFromDataHandler;

id<MTLLibrary> MTIDeviceTrace::NewLibraryWithDataImpl(id Object, SEL Selector, Super::NewLibraryWithDataType::DefinedIMP Original, dispatch_data_t Data, __autoreleasing NSError ** Err)
{
	return GMTITraceNewLibraryFromDataHandler.Trace(Object, Data, MTILibraryTrace::Register(Original(Object, Selector, Data, Err)));
}
id<MTLLibrary> MTIDeviceTrace::NewLibraryWithSourceOptionsErrorImpl(id Object, SEL Selector, Super::NewLibraryWithSourceOptionsErrorType::DefinedIMP Original, NSString* Src, MTLCompileOptions* Opts, NSError** Err)
{
	return MTILibraryTrace::Register(Original(Object, Selector, Src, Opts, Err));
}
void MTIDeviceTrace::NewLibraryWithSourceOptionsCompletionHandlerImpl(id Object, SEL Selector, Super::NewLibraryWithSourceOptionsCompletionHandlerType::DefinedIMP Original, NSString* Src, MTLCompileOptions* Opts, MTLNewLibraryCompletionHandler Handler)
{
	Original(Object, Selector, Src, Opts, ^(id <MTLLibrary> __nullable library, NSError * __nullable error)
	{
		Handler(MTILibraryTrace::Register(library), error);
	});
}


struct MTITraceNewVertexDescHandler : public MTITraceCommandHandler
{
	MTITraceNewVertexDescHandler()
	: MTITraceCommandHandler("", "newVertexDescriptor")
	{
		
	}
	
	MTLVertexDescriptor* Trace(MTLVertexDescriptor* Desc)
	{
		if (!MTITrace::Get().FetchObject((uintptr_t)Desc))
		{
			std::fstream& fs = MTITrace::Get().BeginWrite();
			MTITraceCommandHandler::Trace(fs, (uintptr_t)Desc);
			
			for (unsigned i = 0; i < 31; i++)
			{
				MTLVertexBufferLayoutDescriptor* Buffer = [Desc.layouts objectAtIndexedSubscript:i];
				fs << Buffer.stride;
				fs << Buffer.stepFunction;
				fs << Buffer.stepRate;
			}
			
			for (unsigned i = 0; i < 16; i++)
			{
				MTLVertexAttributeDescriptor* Buffer = [Desc.attributes objectAtIndexedSubscript:i];
				fs << Buffer.format;
				fs << Buffer.offset;
				fs << Buffer.bufferIndex;
			}
			
			MTITrace::Get().RegisterObject((uintptr_t)Desc, Desc);
			MTITrace::Get().EndWrite();
		}
		return Desc;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTLVertexDescriptor* Desc = [MTLVertexDescriptor new];
		
		NSUInteger stride, stepFunction, stepRate;
		
		for (unsigned i = 0; i < 31; i++)
		{
			fs >> stride;
			fs >> stepFunction;
			fs >> stepRate;
			
			MTLVertexBufferLayoutDescriptor* Buffer = [Desc.layouts objectAtIndexedSubscript:i];
			Buffer.stride = stride;
			Buffer.stepFunction = (MTLVertexStepFunction)stepFunction;
			Buffer.stepRate = stepRate;
		}
		
		NSUInteger format, offset, bufferIndex;
		for (unsigned i = 0; i < 16; i++)
		{
			fs >> format;
			fs >> offset;
			fs >> bufferIndex;
			
			MTLVertexAttributeDescriptor* Buffer = [Desc.attributes objectAtIndexedSubscript:i];
			Buffer.format = (MTLVertexFormat)format;
			Buffer.offset = offset;
			Buffer.bufferIndex = bufferIndex;
		}
		
		MTITrace::Get().RegisterObject(Header.Receiver, Desc);
	}
};
static MTITraceNewVertexDescHandler GMTITraceNewVertexDescHandler;



struct MTITraceNewRenderPipelineWithDescriptorHandler : public MTITraceCommandHandler
{
	MTITraceNewRenderPipelineWithDescriptorHandler()
	: MTITraceCommandHandler("MTLDevice", "newRenderPipelineStateWithDescriptor")
	{
		
	}
	
	id<MTLRenderPipelineState> Trace(id Object, MTLRenderPipelineDescriptor* Desc, id<MTLRenderPipelineState> State)
	{
		GMTITraceNewVertexDescHandler.Trace(Desc.vertexDescriptor);
		
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << MTIString(Desc.label ? [Desc.label UTF8String] : "");
		fs << (uintptr_t)Desc.vertexFunction;
		fs << (uintptr_t)Desc.fragmentFunction;
		fs << (uintptr_t)Desc.vertexDescriptor;
		fs << Desc.rasterSampleCount;
		fs << Desc.alphaToCoverageEnabled;
		fs << Desc.alphaToOneEnabled;
		fs << Desc.rasterizationEnabled;
		for (uint i = 0; i < 8; i++)
		{
			MTLRenderPipelineColorAttachmentDescriptor* Attachment = [Desc.colorAttachments objectAtIndexedSubscript:i];
			fs << Attachment.pixelFormat;
			fs << Attachment.blendingEnabled;
			fs << Attachment.sourceRGBBlendFactor;
			fs << Attachment.destinationRGBBlendFactor;
			fs << Attachment.rgbBlendOperation;
			fs << Attachment.sourceAlphaBlendFactor;
			fs << Attachment.destinationAlphaBlendFactor;
			fs << Attachment.alphaBlendOperation;
			fs << Attachment.writeMask;
		}
		fs << Desc.depthAttachmentPixelFormat;
		fs << Desc.stencilAttachmentPixelFormat;
		fs << Desc.inputPrimitiveTopology;
		fs << Desc.tessellationPartitionMode;
		fs << Desc.maxTessellationFactor;
		fs << Desc.tessellationFactorScaleEnabled;
		fs << Desc.tessellationFactorFormat;
		fs << Desc.tessellationControlPointIndexType;
		fs << Desc.tessellationFactorStepFunction;
		fs << Desc.tessellationOutputWindingOrder;
		
		for (uint i = 0; i < 31; i++)
		{
			MTLPipelineBufferDescriptor* Buffer = [Desc.vertexBuffers objectAtIndexedSubscript:i];
			fs << Buffer.mutability;
		}
		for (uint i = 0; i < 31; i++)
		{
			MTLPipelineBufferDescriptor* Buffer = [Desc.fragmentBuffers objectAtIndexedSubscript:i];
			fs << Buffer.mutability;
		}

		fs << Desc.supportIndirectCommandBuffers;

		fs << (uintptr_t)State;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTLRenderPipelineDescriptor* Desc = [[MTLRenderPipelineDescriptor new] autorelease];
		
		MTIString label;
		uintptr_t vertexFunction;
		uintptr_t fragmentFunction;
		uintptr_t vertexDescriptor;
		NSUInteger rasterSampleCount;
		BOOL alphaToCoverageEnabled;
		BOOL alphaToOneEnabled;
		BOOL rasterizationEnabled;
		
		fs >> label;
		fs >> vertexFunction;
		fs >> fragmentFunction;
		fs >> vertexDescriptor;
		fs >> rasterSampleCount;
		fs >> alphaToCoverageEnabled;
		fs >> alphaToOneEnabled;
		fs >> rasterizationEnabled;
		
		NSUInteger pixelFormat;
		NSUInteger blendingEnabled;
		NSUInteger sourceRGBBlendFactor;
		NSUInteger destinationRGBBlendFactor;
		NSUInteger rgbBlendOperation;
		NSUInteger sourceAlphaBlendFactor;
		NSUInteger destinationAlphaBlendFactor;
		NSUInteger alphaBlendOperation;
		uint32_t   writeMask;
		for (uint i = 0; i < 8; i++)
		{
			fs >> pixelFormat;
			fs >> blendingEnabled;
			fs >> sourceRGBBlendFactor;
			fs >> destinationRGBBlendFactor;
			fs >> rgbBlendOperation;
			fs >> sourceAlphaBlendFactor;
			fs >> destinationAlphaBlendFactor;
			fs >> alphaBlendOperation;
			fs >> writeMask;
			
			MTLRenderPipelineColorAttachmentDescriptor* Attachment = [Desc.colorAttachments objectAtIndexedSubscript:i];
			Attachment.pixelFormat = (MTLPixelFormat)pixelFormat;
			Attachment.blendingEnabled = blendingEnabled;
			Attachment.sourceRGBBlendFactor = (MTLBlendFactor)sourceRGBBlendFactor;
			Attachment.destinationRGBBlendFactor = (MTLBlendFactor)destinationRGBBlendFactor;
			Attachment.rgbBlendOperation = (MTLBlendOperation)rgbBlendOperation;
			Attachment.sourceAlphaBlendFactor = (MTLBlendFactor)sourceAlphaBlendFactor;
			Attachment.destinationAlphaBlendFactor = (MTLBlendFactor)destinationAlphaBlendFactor;
			Attachment.alphaBlendOperation = (MTLBlendOperation)alphaBlendOperation;
			Attachment.writeMask =  writeMask;
		}
		
		NSUInteger depthAttachmentPixelFormat;
		NSUInteger stencilAttachmentPixelFormat;
		NSUInteger inputPrimitiveTopology;
		NSUInteger tessellationPartitionMode;
		NSUInteger maxTessellationFactor;
		BOOL tessellationFactorScaleEnabled;
		NSUInteger tessellationFactorFormat;
		NSUInteger tessellationControlPointIndexType;
		NSUInteger tessellationFactorStepFunction;
		NSUInteger tessellationOutputWindingOrder;
		
		fs >> depthAttachmentPixelFormat;
		fs >> stencilAttachmentPixelFormat;
		fs >> inputPrimitiveTopology;
		fs >> tessellationPartitionMode;
		fs >> maxTessellationFactor;
		fs >> tessellationFactorScaleEnabled;
		fs >> tessellationFactorFormat;
		fs >> tessellationControlPointIndexType;
		fs >> tessellationFactorStepFunction;
		fs >> tessellationOutputWindingOrder;
		
		Desc.depthAttachmentPixelFormat = (MTLPixelFormat)depthAttachmentPixelFormat;
		Desc.stencilAttachmentPixelFormat = (MTLPixelFormat)stencilAttachmentPixelFormat;
		Desc.inputPrimitiveTopology = (MTLPrimitiveTopologyClass)inputPrimitiveTopology;
		Desc.tessellationPartitionMode = (MTLTessellationPartitionMode)tessellationPartitionMode;
		Desc.maxTessellationFactor = maxTessellationFactor;
		Desc.tessellationFactorScaleEnabled = tessellationFactorScaleEnabled;
		Desc.tessellationFactorFormat = (MTLTessellationFactorFormat)tessellationFactorFormat;
		Desc.tessellationControlPointIndexType = (MTLTessellationControlPointIndexType)tessellationControlPointIndexType;
		Desc.tessellationFactorStepFunction = (MTLTessellationFactorStepFunction)tessellationFactorStepFunction;
		Desc.tessellationOutputWindingOrder = (MTLWinding)tessellationOutputWindingOrder;
		
		NSUInteger mutability;
		for (uint i = 0; i < 31; i++)
		{
			MTLPipelineBufferDescriptor* Buffer = [Desc.vertexBuffers objectAtIndexedSubscript:i];
			fs >> mutability;
			Buffer.mutability = (MTLMutability)mutability;
		}
		for (uint i = 0; i < 31; i++)
		{
			MTLPipelineBufferDescriptor* Buffer = [Desc.fragmentBuffers objectAtIndexedSubscript:i];
			fs >> mutability;
			Buffer.mutability = (MTLMutability)mutability;
		}
		
		BOOL supportIndirectCommandBuffers;
		fs >> supportIndirectCommandBuffers;
		Desc.supportIndirectCommandBuffers = supportIndirectCommandBuffers;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLRenderPipelineState> State = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newRenderPipelineStateWithDescriptor:Desc error:nil];
		assert(State);
		
		MTITrace::Get().RegisterObject(Result, State);
	}
};
static MTITraceNewRenderPipelineWithDescriptorHandler GMTITraceNewRenderPipelineWithDescriptorHandler;

id<MTLRenderPipelineState> MTIDeviceTrace::NewRenderPipelineStateWithDescriptorErrorImpl(id Object, SEL Selector, Super::NewRenderPipelineStateWithDescriptorErrorType::DefinedIMP Original, MTLRenderPipelineDescriptor* Desc, NSError** Err)
{
	return GMTITraceNewRenderPipelineWithDescriptorHandler.Trace(Object, Desc, MTIRenderPipelineStateTrace::Register(Original(Object, Selector, Desc, Err)));
}
id<MTLRenderPipelineState> MTIDeviceTrace::NewRenderPipelineStateWithDescriptorOptionsReflectionErrorImpl(id Object, SEL Selector, Super::NewRenderPipelineStateWithDescriptorOptionsReflectionErrorType::DefinedIMP Original, MTLRenderPipelineDescriptor* Desc, MTLPipelineOption Opts, MTLAutoreleasedRenderPipelineReflection* Refl, NSError** Err)
{
	return GMTITraceNewRenderPipelineWithDescriptorHandler.Trace(Object, Desc, MTIRenderPipelineStateTrace::Register(Original(Object, Selector, Desc, Opts, Refl, Err)));
}
void MTIDeviceTrace::NewRenderPipelineStateWithDescriptorCompletionHandlerImpl(id Object, SEL Selector, Super::NewRenderPipelineStateWithDescriptorCompletionHandlerType::DefinedIMP Original, MTLRenderPipelineDescriptor* Desc, MTLNewRenderPipelineStateCompletionHandler Handler)
{
	Original(Object, Selector, Desc, ^(id <MTLRenderPipelineState> __nullable renderPipelineState, NSError * __nullable error)
			 {
				 Handler(MTIRenderPipelineStateTrace::Register(renderPipelineState), error);
			 });
}
void MTIDeviceTrace::NewRenderPipelineStateWithDescriptorOptionsCompletionHandlerImpl(id Object, SEL Selector, Super::NewRenderPipelineStateWithDescriptorOptionsCompletionHandlerType::DefinedIMP Original, MTLRenderPipelineDescriptor* Desc, MTLPipelineOption Opts, MTLNewRenderPipelineStateWithReflectionCompletionHandler Handler)
{
	Original(Object, Selector, Desc, Opts, ^(id <MTLRenderPipelineState> __nullable renderPipelineState, MTLRenderPipelineReflection * __nullable reflection, NSError * __nullable error)
	{
		Handler(MTIRenderPipelineStateTrace::Register(renderPipelineState), reflection, error);
	});
}

struct MTITraceNewComputePipelineHandler : public MTITraceCommandHandler
{
	MTITraceNewComputePipelineHandler()
	: MTITraceCommandHandler("MTLDevice", "newComputePipelineStateWithFunction")
	{
		
	}
	
	id<MTLComputePipelineState> Trace(id Object, id<MTLFunction> Func, id<MTLComputePipelineState> State)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Func;
		fs << (uintptr_t)State;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Func;
		fs >> Func;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLComputePipelineState> State = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newComputePipelineStateWithFunction:(id<MTLFunction>)MTITrace::Get().FetchObject(Func) error:nil];
		assert(State);
		
		MTITrace::Get().RegisterObject(Result, State);
	}
};
static MTITraceNewComputePipelineHandler GMTITraceNewComputePipelineHandler;
id<MTLComputePipelineState> MTIDeviceTrace::NewComputePipelineStateWithFunctionErrorImpl(id Object, SEL Selector, Super::NewComputePipelineStateWithFunctionErrorType::DefinedIMP Original, id<MTLFunction> Func, NSError** Err)
{
	return GMTITraceNewComputePipelineHandler.Trace(Object, Func, MTIComputePipelineStateTrace::Register(Original(Object, Selector, Func, Err)));
}
id<MTLComputePipelineState> MTIDeviceTrace::NewComputePipelineStateWithFunctionOptionsReflectionErrorImpl(id Object, SEL Selector, Super::NewComputePipelineStateWithFunctionOptionsReflectionErrorType::DefinedIMP Original, id<MTLFunction> Func, MTLPipelineOption Opts, MTLAutoreleasedComputePipelineReflection * Refl, NSError** Err)
{
	return GMTITraceNewComputePipelineHandler.Trace(Object, Func, MTIComputePipelineStateTrace::Register(Original(Object, Selector, Func, Opts, Refl, Err)));
}
void MTIDeviceTrace::NewComputePipelineStateWithFunctionCompletionHandlerImpl(id Object, SEL Selector, Super::NewComputePipelineStateWithFunctionCompletionHandlerType::DefinedIMP Original, id<MTLFunction>  Func, MTLNewComputePipelineStateCompletionHandler Handler)
{
	Original(Object, Selector, Func, ^(id <MTLComputePipelineState> __nullable computePipelineState, NSError * __nullable error){
		Handler(MTIComputePipelineStateTrace::Register(computePipelineState ), error);
	});
}
void MTIDeviceTrace::NewComputePipelineStateWithFunctionOptionsCompletionHandlerImpl(id Object, SEL Selector, Super::NewComputePipelineStateWithFunctionOptionsCompletionHandlerType::DefinedIMP Original, id<MTLFunction> Func, MTLPipelineOption Opts, MTLNewComputePipelineStateWithReflectionCompletionHandler Handler)
{
	Original(Object, Selector, Func, Opts, ^(id <MTLComputePipelineState> __nullable computePipelineState, MTLComputePipelineReflection * __nullable reflection, NSError * __nullable error){
		Handler(MTIComputePipelineStateTrace::Register(computePipelineState ), reflection, error);
	});
}

struct MTITraceNewComputePipelineWithDescriptorHandler : public MTITraceCommandHandler
{
	MTITraceNewComputePipelineWithDescriptorHandler()
	: MTITraceCommandHandler("MTLDevice", "newComputePipelineStateWithDescriptor")
	{
		
	}
	
	id<MTLComputePipelineState> Trace(id Object, MTLComputePipelineDescriptor* Desc, id<MTLComputePipelineState> State)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << MTIString(Desc.label ? [Desc.label UTF8String] : "");
		fs << (uintptr_t)Desc.computeFunction;
		fs << Desc.threadGroupSizeIsMultipleOfThreadExecutionWidth;
		fs << Desc.maxTotalThreadsPerThreadgroup;
		
		for (unsigned i = 0; i < 31; i++)
		{
			MTLBufferLayoutDescriptor* Buffer = [Desc.stageInputDescriptor.layouts objectAtIndexedSubscript:i];
			fs << Buffer.stride;
			fs << Buffer.stepFunction;
			fs << Buffer.stepRate;
		}
		
		for (unsigned i = 0; i < 16; i++)
		{
			MTLAttributeDescriptor* Buffer = [Desc.stageInputDescriptor.attributes objectAtIndexedSubscript:i];
			fs << Buffer.format;
			fs << Buffer.offset;
			fs << Buffer.bufferIndex;
		}
		fs << Desc.stageInputDescriptor.indexType;
		fs << Desc.stageInputDescriptor.indexBufferIndex;
		for (uint i = 0; i < 31; i++)
		{
			MTLPipelineBufferDescriptor* Buffer = [Desc.buffers objectAtIndexedSubscript:i];
			fs << Buffer.mutability;
		}
		
		fs << (uintptr_t)State;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTIString label;
		uintptr_t Func;
		BOOL threadGroupSizeIsMultipleOfThreadExecutionWidth;
		NSUInteger maxTotalThreadsPerThreadgroup;
		MTLComputePipelineDescriptor* Desc = [[MTLComputePipelineDescriptor new] autorelease];
		
		fs >> label;
		fs >> Func;
		fs >> threadGroupSizeIsMultipleOfThreadExecutionWidth;
		fs >> maxTotalThreadsPerThreadgroup;
		
		Desc.label = [NSString stringWithUTF8String:label.c_str()];
		Desc.computeFunction = MTITrace::Get().FetchObject(Func);
		Desc.threadGroupSizeIsMultipleOfThreadExecutionWidth = threadGroupSizeIsMultipleOfThreadExecutionWidth;
		Desc.maxTotalThreadsPerThreadgroup = maxTotalThreadsPerThreadgroup;

		NSUInteger stride, stepFunction, stepRate;
		for (unsigned i = 0; i < 31; i++)
		{
			fs >> stride;
			fs >> stepFunction;
			fs >> stepRate;
			
			MTLBufferLayoutDescriptor* Buffer = [Desc.stageInputDescriptor.layouts objectAtIndexedSubscript:i];
			Buffer.stride = stride;
			Buffer.stepFunction = (MTLStepFunction)stepFunction;
			Buffer.stepRate = stepRate;
		}
		
		NSUInteger format, offset, bufferIndex;
		for (unsigned i = 0; i < 16; i++)
		{
			fs >> format;
			fs >> offset;
			fs >> bufferIndex;
			
			MTLAttributeDescriptor* Buffer = [Desc.stageInputDescriptor.attributes objectAtIndexedSubscript:i];
			Buffer.format = (MTLAttributeFormat)format;
			Buffer.offset = offset;
			Buffer.bufferIndex = bufferIndex;
		}
		
		NSUInteger indexType;
		NSUInteger indexBufferIndex;
		fs >> indexType;
		fs >> indexBufferIndex;
		
		Desc.stageInputDescriptor.indexType = (MTLIndexType)indexType;
		Desc.stageInputDescriptor.indexBufferIndex = indexBufferIndex;
		
		NSUInteger mutability;
		for (uint i = 0; i < 31; i++)
		{
			MTLPipelineBufferDescriptor* Buffer = [Desc.buffers objectAtIndexedSubscript:i];
			fs >> mutability;
			Buffer.mutability = (MTLMutability)mutability;
		}
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLComputePipelineState> State = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newComputePipelineStateWithDescriptor:Desc options:MTLPipelineOptionNone reflection:nil error:nil];
		assert(State);
		
		MTITrace::Get().RegisterObject(Result, State);
	}
};
static MTITraceNewComputePipelineWithDescriptorHandler GMTITraceNewComputePipelineWithDescriptorHandler;
id<MTLComputePipelineState> MTIDeviceTrace::NewComputePipelineStateWithDescriptorOptionsReflectionErrorImpl(id Object, SEL Selector, Super::NewComputePipelineStateWithDescriptorOptionsReflectionErrorType::DefinedIMP Original, MTLComputePipelineDescriptor* Desc, MTLPipelineOption Opts, MTLAutoreleasedComputePipelineReflection * Refl, NSError** Err)
{
	return GMTITraceNewComputePipelineWithDescriptorHandler.Trace(Object, Desc, MTIComputePipelineStateTrace::Register(Original(Object, Selector, Desc, Opts, Refl, Err)));
}
void MTIDeviceTrace::NewComputePipelineStateWithDescriptorOptionsCompletionHandlerImpl(id Object, SEL Selector, Super::NewComputePipelineStateWithDescriptorOptionsCompletionHandlerType::DefinedIMP Original, MTLComputePipelineDescriptor* Desc, MTLPipelineOption Opts, MTLNewComputePipelineStateWithReflectionCompletionHandler Handler)
{
	Original(Object, Selector, Desc, Opts, ^(id <MTLComputePipelineState> __nullable computePipelineState, MTLComputePipelineReflection * __nullable reflection, NSError * __nullable error){
		Handler(MTIComputePipelineStateTrace::Register(computePipelineState ), reflection, error);
	});
}




struct MTITraceNewFenceHandler : public MTITraceCommandHandler
{
	MTITraceNewFenceHandler()
	: MTITraceCommandHandler("MTLDevice", "newFence")
	{
		
	}
	
	id<MTLFence> Trace(id Object, id<MTLFence> Fence)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Fence;
		
		MTITrace::Get().EndWrite();
		return Fence;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Result;
		fs >> Result;
		
		id<MTLFence> Fence = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newFence];
		assert(Fence);
		
		MTITrace::Get().RegisterObject(Result, Fence);
	}
};
static MTITraceNewFenceHandler GMTITraceNewFenceHandler;
id<MTLFence> MTIDeviceTrace::NewFenceImpl(id Object, SEL Selector, Super::NewFenceType::DefinedIMP Original)
{
	return GMTITraceNewFenceHandler.Trace(Object, MTIFenceTrace::Register(Original(Object, Selector)));
}

struct MTITraceNewArgumentEncoderHandler : public MTITraceCommandHandler
{
	MTITraceNewArgumentEncoderHandler()
	: MTITraceCommandHandler("MTLDevice", "newComputePipelineStateWithDescriptor")
	{
		
	}
	
	id<MTLArgumentEncoder> Trace(id Object, NSArray <MTLArgumentDescriptor *> * Args, id<MTLArgumentEncoder> State)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Args.count;
		for (MTLArgumentDescriptor * Arg : Args)
		{
			fs << Arg.dataType;
			fs << Arg.index;
			fs << Arg.arrayLength;
			fs << Arg.access;
			fs << Arg.textureType;
			fs << Arg.constantBlockAlignment;
		}
		
		fs << (uintptr_t)State;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger count;
		fs >> count;
		
		NSUInteger dataType;
		NSUInteger index;
		NSUInteger arrayLength;
		NSUInteger access;
		NSUInteger textureType;
		NSUInteger constantBlockAlignment;
		
		NSMutableArray* Array = [[NSMutableArray new] autorelease];
		for (unsigned i = 0; i < count; i++)
		{
			fs >> dataType;
			fs >> index;
			fs >> arrayLength;
			fs >> access;
			fs >> textureType;
			fs >> constantBlockAlignment;
			
			MTLArgumentDescriptor* Arg = [[MTLArgumentDescriptor new] autorelease];
			Arg.dataType = (MTLDataType)dataType;
			Arg.index = index;
			Arg.arrayLength = arrayLength;
			Arg.access = (MTLArgumentAccess)access;
			Arg.textureType = (MTLTextureType)textureType;
			Arg.constantBlockAlignment = constantBlockAlignment;
			[Array addObject:Arg];
		}
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLArgumentEncoder> State = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newArgumentEncoderWithArguments:Array];
		assert(State);
		
		MTITrace::Get().RegisterObject(Result, State);
	}
};
static MTITraceNewArgumentEncoderHandler GMTITraceNewArgumentEncoderHandler;
id<MTLArgumentEncoder> MTIDeviceTrace::NewArgumentEncoderWithArgumentsImpl(id Object, SEL Selector, Super::NewArgumentEncoderWithArgumentsType::DefinedIMP Original, NSArray <MTLArgumentDescriptor *> * Args)
{
	return GMTITraceNewArgumentEncoderHandler.Trace(Object, Args, MTIArgumentEncoderTrace::Register(Original(Object, Selector, Args)));
}
INTERPOSE_DEFINITION(MTIDeviceTrace, getDefaultSamplePositionscount, void, MTLPPSamplePosition* s, NSUInteger c)
{
	Original(Obj, Cmd, s, c);
}

INTERPOSE_DEFINITION(MTIDeviceTrace, newRenderPipelineStateWithTileDescriptoroptionsreflectionerror, id <MTLRenderPipelineState>, MTLTileRenderPipelineDescriptor* d, MTLPipelineOption o, MTLAutoreleasedRenderPipelineReflection* r, NSError** e)
{
	return Original(Obj, Cmd, d, o, r, e);
}

INTERPOSE_DEFINITION(MTIDeviceTrace,  newRenderPipelineStateWithTileDescriptoroptionscompletionHandler, void, MTLTileRenderPipelineDescriptor* d, MTLPipelineOption o, MTLNewRenderPipelineStateWithReflectionCompletionHandler h)
{
	Original(Obj, Cmd, d, o, h);
}

MTLPP_END
