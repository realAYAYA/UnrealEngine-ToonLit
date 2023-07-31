// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTIComputeCommandEncoder.hpp"
#include "MTIRenderCommandEncoder.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

INTERPOSE_PROTOCOL_REGISTER(MTIComputeCommandEncoderTrace, id<MTLComputeCommandEncoder>);

struct MTITraceComputeEncoderSetcomputepipelinestateHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderSetcomputepipelinestateHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Setcomputepipelinestate")
	{
		
	}
	
	void Trace(id Object, id <MTLComputePipelineState> state)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)state;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Res;
		fs >> Res;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setComputePipelineState:MTITrace::Get().FetchObject(Res)];
	}
};
static MTITraceComputeEncoderSetcomputepipelinestateHandler GMTITraceComputeEncoderSetcomputepipelinestateHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Setcomputepipelinestate, void, id <MTLComputePipelineState> state)
{
	GMTITraceComputeEncoderSetcomputepipelinestateHandler.Trace(Obj, state);
	Original(Obj, Cmd, state);
}

struct MTITraceComputeEncoderSetbyteslengthatindexHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderSetbyteslengthatindexHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Setbyteslengthatindex")
	{
		
	}
	
	void Trace(id Object, const void * bytes, NSUInteger length, NSUInteger index)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << length;
		fs << index;
		MTITraceArray<uint8> Data;
		Data.Length = length;
		Data.Data = (const uint8*)bytes;
		fs << Data;

		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTITraceArray<uint8> Data;
		NSUInteger length;
		NSUInteger index;
		fs >> length;
		fs >> index;
		fs >> Data;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setBytes:Data.Backing.data() length:length atIndex:index];
	}
};
static MTITraceComputeEncoderSetbyteslengthatindexHandler GMTITraceComputeEncoderSetbyteslengthatindexHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Setbyteslengthatindex, void, const void * bytes, NSUInteger length, NSUInteger index)
{
	GMTITraceComputeEncoderSetbyteslengthatindexHandler.Trace(Obj, bytes, length, index);
	Original(Obj, Cmd, bytes, length, index);
}

struct MTITraceComputeEncoderSetbufferoffsetatindexHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderSetbufferoffsetatindexHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Setbufferoffsetatindex")
	{
		
	}
	
	void Trace(id Object, id <MTLBuffer> buffer, NSUInteger offset, NSUInteger index)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)buffer;
		fs << offset;
		fs << index;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Buffer;
		NSUInteger offset;
		NSUInteger index;
		fs >> Buffer;
		fs >> offset;
		fs >> index;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setBuffer:MTITrace::Get().FetchObject(Buffer) offset:offset atIndex:index];
	}
};
static MTITraceComputeEncoderSetbufferoffsetatindexHandler GMTITraceComputeEncoderSetbufferoffsetatindexHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Setbufferoffsetatindex, void,  id <MTLBuffer> buffer, NSUInteger offset, NSUInteger index)
{
	GMTITraceComputeEncoderSetbufferoffsetatindexHandler.Trace(Obj, buffer, offset, index);
	Original(Obj, Cmd, buffer, offset, index);
}

struct MTITraceComputeEncoderSetBufferOffsetatindex2Handler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderSetBufferOffsetatindex2Handler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "SetBufferOffsetatindex2")
	{
		
	}
	
	void Trace(id Object, NSUInteger offset, NSUInteger index)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << offset;
		fs << index;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger offset;
		NSUInteger index;
		fs >> offset;
		fs >> index;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setBufferOffset:offset atIndex:index];
	}
};
static MTITraceComputeEncoderSetBufferOffsetatindex2Handler GMTITraceComputeEncoderSetBufferOffsetatindex2Handler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, SetBufferOffsetatindex, void, NSUInteger offset, NSUInteger index)
{
	GMTITraceComputeEncoderSetBufferOffsetatindex2Handler.Trace(Obj, offset, index);
	Original(Obj, Cmd, offset, index);
}

INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Setbuffersoffsetswithrange, void, const id <MTLBuffer>  * buffers, const NSUInteger * offsets, NSRange range)
{
	Original(Obj, Cmd, buffers, offsets, range);
}

struct MTITraceComputeEncoderSettextureatindexHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderSettextureatindexHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Settextureatindex")
	{
		
	}
	
	void Trace(id Object, id <MTLTexture> texture, NSUInteger index)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)texture;
		fs << index;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t texture;
		NSUInteger index;
		fs >> texture;
		fs >> index;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setTexture:MTITrace::Get().FetchObject(texture) atIndex:index];
	}
};
static MTITraceComputeEncoderSettextureatindexHandler GMTITraceComputeEncoderSettextureatindexHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Settextureatindex, void,  id <MTLTexture> texture, NSUInteger index)
{
	GMTITraceComputeEncoderSettextureatindexHandler.Trace(Obj, texture, index);
	Original(Obj, Cmd, texture, index);
}

INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Settextureswithrange, void, const id <MTLTexture>  * textures, NSRange range)
{
	Original(Obj, Cmd, textures, range);
}

struct MTITraceComputeEncoderSetsamplerstateatindexHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderSetsamplerstateatindexHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Setsamplerstateatindex")
	{
		
	}
	
	void Trace(id Object, id <MTLSamplerState> sampler, NSUInteger index)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)sampler;
		fs << index;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t sampler;
		NSUInteger index;
		fs >> sampler;
		fs >> index;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setSamplerState:MTITrace::Get().FetchObject(sampler) atIndex:index];
	}
};
static MTITraceComputeEncoderSetsamplerstateatindexHandler GMTITraceComputeEncoderSetsamplerstateatindexHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Setsamplerstateatindex, void,  id <MTLSamplerState> sampler, NSUInteger index)
{
	GMTITraceComputeEncoderSetsamplerstateatindexHandler.Trace(Obj, sampler, index);
	Original(Obj, Cmd, sampler, index);
}

INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Setsamplerstateswithrange, void, const id <MTLSamplerState>  * samplers, NSRange range)
{
	Original(Obj, Cmd, samplers, range);
}

struct MTITraceComputeEncoderSetsamplerstatelodminclamplodmaxclampatindexHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderSetsamplerstatelodminclamplodmaxclampatindexHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Setsamplerstatelodminclamplodmaxclampatindex")
	{
		
	}
	
	void Trace(id Object, id <MTLSamplerState> sampler, float lodMinClamp, float lodMaxClamp, NSUInteger index)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)sampler;
		fs << lodMinClamp;
		fs << lodMaxClamp;
		fs << index;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t sampler;
		float lodMinClamp;
		float lodMaxClamp;
		NSUInteger index;
		fs >> sampler;
		fs >> lodMinClamp;
		fs >> lodMaxClamp;
		fs >> index;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setSamplerState:MTITrace::Get().FetchObject(sampler) lodMinClamp:lodMinClamp lodMaxClamp:lodMaxClamp atIndex:index];
	}
};
static MTITraceComputeEncoderSetsamplerstatelodminclamplodmaxclampatindexHandler GMTITraceComputeEncoderSetsamplerstatelodminclamplodmaxclampatindexHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Setsamplerstatelodminclamplodmaxclampatindex, void,  id <MTLSamplerState> sampler, float lodMinClamp, float lodMaxClamp, NSUInteger index)
{
	GMTITraceComputeEncoderSetsamplerstatelodminclamplodmaxclampatindexHandler.Trace(Obj, sampler, lodMinClamp, lodMaxClamp, index);
	Original(Obj, Cmd, sampler, lodMinClamp, lodMaxClamp, index);
}

INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Setsamplerstateslodminclampslodmaxclampswithrange, void, const id <MTLSamplerState>  * samplers, const float * lodMinClamps, const float * lodMaxClamps, NSRange range)
{
	Original(Obj, Cmd, samplers, lodMinClamps, lodMaxClamps, range);
}

INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Setthreadgroupmemorylengthatindex, void, NSUInteger length, NSUInteger index)
{
	Original(Obj, Cmd, length, index);
}

struct MTITraceComputeEncoderSetstageinregionHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderSetstageinregionHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Setstageinregion")
	{
		
	}
	
	void Trace(id Object, MTLPPRegion region)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << region.origin.x;
		fs << region.origin.y;
		fs << region.origin.z;
		fs << region.size.width;
		fs << region.size.height;
		fs << region.size.depth;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTLRegion region;
		fs >> region.origin.x;
		fs >> region.origin.y;
		fs >> region.origin.z;
		fs >> region.size.width;
		fs >> region.size.height;
		fs >> region.size.depth;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setStageInRegion:region];
	}
};
static MTITraceComputeEncoderSetstageinregionHandler GMTITraceComputeEncoderSetstageinregionHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Setstageinregion, void, MTLPPRegion region)
{
	GMTITraceComputeEncoderSetstageinregionHandler.Trace(Obj, region);
	Original(Obj, Cmd, region);
}

struct MTITraceComputeEncoderDispatchthreadgroupsthreadsperthreadgroupHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderDispatchthreadgroupsthreadsperthreadgroupHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Dispatchthreadgroupsthreadsperthreadgroup")
	{
		
	}
	
	void Trace(id Object, MTLPPSize threadgroupsPerGrid, MTLPPSize threadsPerThreadgroup)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << threadgroupsPerGrid.width;
		fs << threadgroupsPerGrid.height;
		fs << threadgroupsPerGrid.depth;
		fs << threadsPerThreadgroup.width;
		fs << threadsPerThreadgroup.height;
		fs << threadsPerThreadgroup.depth;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTLSize threadgroupsPerGrid;
		MTLSize threadsPerThreadgroup;
		fs >> threadgroupsPerGrid.width;
		fs >> threadgroupsPerGrid.height;
		fs >> threadgroupsPerGrid.depth;
		fs >> threadsPerThreadgroup.width;
		fs >> threadsPerThreadgroup.height;
		fs >> threadsPerThreadgroup.depth;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) dispatchThreadgroups:threadgroupsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];
	}
};
static MTITraceComputeEncoderDispatchthreadgroupsthreadsperthreadgroupHandler GMTITraceComputeEncoderDispatchthreadgroupsthreadsperthreadgroupHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Dispatchthreadgroupsthreadsperthreadgroup, void, MTLPPSize threadgroupsPerGrid, MTLPPSize threadsPerThreadgroup)
{
	GMTITraceComputeEncoderDispatchthreadgroupsthreadsperthreadgroupHandler.Trace(Obj, threadgroupsPerGrid, threadsPerThreadgroup);
	Original(Obj, Cmd, threadgroupsPerGrid, threadsPerThreadgroup);
}

struct MTITraceComputeEncoderDispatchIndirectthreadgroupsthreadsperthreadgroupHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderDispatchIndirectthreadgroupsthreadsperthreadgroupHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Dispatchthreadgroupswithindirectbufferindirectbufferoffsetthreadsperthreadgroup")
	{
		
	}
	
	void Trace(id Object, id <MTLBuffer> indirectBuffer, NSUInteger indirectBufferOffset, MTLPPSize threadsPerThreadgroup)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)indirectBuffer;
		fs << indirectBufferOffset;
		fs << threadsPerThreadgroup.width;
		fs << threadsPerThreadgroup.height;
		fs << threadsPerThreadgroup.depth;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t indirectBuffer;
		NSUInteger indirectBufferOffset;
		MTLSize threadsPerThreadgroup;
		fs >> indirectBuffer;
		fs >> indirectBufferOffset;
		fs >> threadsPerThreadgroup.width;
		fs >> threadsPerThreadgroup.height;
		fs >> threadsPerThreadgroup.depth;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) dispatchThreadgroupsWithIndirectBuffer:MTITrace::Get().FetchObject(indirectBuffer) indirectBufferOffset:indirectBufferOffset threadsPerThreadgroup:threadsPerThreadgroup];
	}
};
static MTITraceComputeEncoderDispatchIndirectthreadgroupsthreadsperthreadgroupHandler GMTITraceComputeEncoderDispatchIndirectthreadgroupsthreadsperthreadgroupHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Dispatchthreadgroupswithindirectbufferindirectbufferoffsetthreadsperthreadgroup, void, id <MTLBuffer> indirectBuffer, NSUInteger indirectBufferOffset, MTLPPSize threadsPerThreadgroup)
{
	GMTITraceComputeEncoderDispatchIndirectthreadgroupsthreadsperthreadgroupHandler.Trace(Obj, indirectBuffer, indirectBufferOffset, threadsPerThreadgroup);
	Original(Obj, Cmd, indirectBuffer, indirectBufferOffset, threadsPerThreadgroup);
}

struct MTITraceComputeEncoderDispatchthreadsthreadsperthreadgroupHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderDispatchthreadsthreadsperthreadgroupHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Dispatchthreadsthreadsperthreadgroup")
	{
		
	}
	
	void Trace(id Object, MTLPPSize threadsPerGrid, MTLPPSize threadsPerThreadgroup)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << threadsPerGrid.width;
		fs << threadsPerGrid.height;
		fs << threadsPerGrid.depth;
		fs << threadsPerThreadgroup.width;
		fs << threadsPerThreadgroup.height;
		fs << threadsPerThreadgroup.depth;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTLSize threadsPerGrid;
		MTLSize threadsPerThreadgroup;
		fs >> threadsPerGrid.width;
		fs >> threadsPerGrid.height;
		fs >> threadsPerGrid.depth;
		fs >> threadsPerThreadgroup.width;
		fs >> threadsPerThreadgroup.height;
		fs >> threadsPerThreadgroup.depth;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];
	}
};
static MTITraceComputeEncoderDispatchthreadsthreadsperthreadgroupHandler GMTITraceComputeEncoderDispatchthreadsthreadsperthreadgroupHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Dispatchthreadsthreadsperthreadgroup, void, MTLPPSize threadsPerGrid, MTLPPSize threadsPerThreadgroup)
{
	GMTITraceComputeEncoderDispatchthreadsthreadsperthreadgroupHandler.Trace(Obj, threadsPerGrid, threadsPerThreadgroup);
	Original(Obj, Cmd, threadsPerGrid, threadsPerThreadgroup);
}

struct MTITraceComputeEncoderUpdateFenceHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderUpdateFenceHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "UpdateFence")
	{
		
	}
	
	void Trace(id Object, id<MTLFence> Res)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Res;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Res;
		fs >> Res;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) updateFence:MTITrace::Get().FetchObject(Res)];
	}
};
static MTITraceComputeEncoderUpdateFenceHandler GMTITraceComputeEncoderUpdateFenceHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Updatefence, void, id <MTLFence> fence)
{
	GMTITraceComputeEncoderUpdateFenceHandler.Trace(Obj, fence);
	Original(Obj, Cmd, fence);
}

struct MTITraceComputeEncoderWaitForFenceHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderWaitForFenceHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "WaitForFence")
	{
		
	}
	
	void Trace(id Object, id<MTLFence> Res)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Res;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Res;
		fs >> Res;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) waitForFence:MTITrace::Get().FetchObject(Res)];
	}
};
static MTITraceComputeEncoderWaitForFenceHandler GMTITraceComputeEncoderWaitForFenceHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Waitforfence, void, id <MTLFence> fence)
{
	GMTITraceComputeEncoderWaitForFenceHandler.Trace(Obj, fence);
	Original(Obj, Cmd, fence);
}

struct MTITraceComputeEncoderUseresourceusageHandler : public MTITraceCommandHandler
{
	MTITraceComputeEncoderUseresourceusageHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Useresourceusage")
	{
		
	}
	
	void Trace(id Object, id <MTLResource> resource, MTLResourceUsage usage)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)resource;
		fs << usage;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Res;
		fs >> Res;
		
		NSUInteger usage;
		fs >> usage;
		
		[(id<MTLComputeCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) useResource:MTITrace::Get().FetchObject(Res) usage:(MTLResourceUsage)usage];
	}
};
static MTITraceComputeEncoderUseresourceusageHandler GMTITraceComputeEncoderUseresourceusageHandler;
INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Useresourceusage, void, id <MTLResource> resource, MTLResourceUsage usage)
{
	GMTITraceComputeEncoderUseresourceusageHandler.Trace(Obj, resource, usage);
	Original(Obj, Cmd, resource, usage);
}

INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Useresourcescountusage, void, const id <MTLResource> * resources, NSUInteger count, MTLResourceUsage usage)
{
	Original(Obj, Cmd, resources, count, usage);
}

INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Useheap, void, id <MTLHeap> heap)
{
	Original(Obj, Cmd, heap);
}

INTERPOSE_DEFINITION( MTIComputeCommandEncoderTrace, Useheapscount, void, const id <MTLHeap> * heaps, NSUInteger count)
{
	Original(Obj, Cmd, heaps, count);
}

INTERPOSE_DEFINITION(MTIComputeCommandEncoderTrace, SetImageblockWidthHeight, void, NSUInteger width, NSUInteger height)
{
	Original(Obj, Cmd, width, height);
}

MTLPP_END
