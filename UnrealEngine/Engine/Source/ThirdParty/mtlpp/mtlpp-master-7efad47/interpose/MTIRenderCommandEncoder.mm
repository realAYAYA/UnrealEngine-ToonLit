// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTIRenderCommandEncoder.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

INTERPOSE_PROTOCOL_REGISTER(MTIRenderCommandEncoderTrace, id<MTLRenderCommandEncoder>);

struct MTITraceRenderEncoderSetrenderpipelinestateHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetrenderpipelinestateHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setrenderpipelinestate")
	{
		
	}
	
	void Trace(id Object, id <MTLRenderPipelineState> state)
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setRenderPipelineState:MTITrace::Get().FetchObject(Res)];
	}
};
static MTITraceRenderEncoderSetrenderpipelinestateHandler GMTITraceRenderEncoderSetrenderpipelinestateHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setrenderpipelinestate,  void,   id <MTLRenderPipelineState> pipelineState)
{
	GMTITraceRenderEncoderSetrenderpipelinestateHandler.Trace(Obj, pipelineState);
	Original(Obj, Cmd, pipelineState);
}

struct MTITraceRenderEncoderSetVertexbyteslengthatindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetVertexbyteslengthatindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetvertexbytesLengthAtindex")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setVertexBytes:Data.Backing.data() length:length atIndex:index];
	}
};
static MTITraceRenderEncoderSetVertexbyteslengthatindexHandler GMTITraceRenderEncoderSetVertexbyteslengthatindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetvertexbytesLengthAtindex,  void,   const void * bytes,NSUInteger length,NSUInteger index)
{
	GMTITraceRenderEncoderSetVertexbyteslengthatindexHandler.Trace(Obj, bytes, length, index);
	Original(Obj, Cmd, bytes, length, index);
}

struct MTITraceRenderEncoderSetvertexbufferoffsetatindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetvertexbufferoffsetatindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setvertexbufferoffsetatindex")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setVertexBuffer:MTITrace::Get().FetchObject(Buffer) offset:offset atIndex:index];
	}
};
static MTITraceRenderEncoderSetvertexbufferoffsetatindexHandler GMTITraceRenderEncoderSetvertexbufferoffsetatindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetvertexbufferOffsetAtindex,  void,    id <MTLBuffer> buffer,NSUInteger offset,NSUInteger index)
{
	GMTITraceRenderEncoderSetvertexbufferoffsetatindexHandler.Trace(Obj, buffer, offset, index);
	Original(Obj, Cmd, buffer, offset, index);
}

struct MTITraceRenderEncoderSetVertexBufferOffsetatindex2Handler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetVertexBufferOffsetatindex2Handler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetVertexBufferOffsetatindex2")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setVertexBufferOffset:offset atIndex:index];
	}
};
static MTITraceRenderEncoderSetVertexBufferOffsetatindex2Handler GMTITraceRenderEncoderSetVertexBufferOffsetatindex2Handler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetvertexbufferoffsetAtindex,  void,   NSUInteger offset,NSUInteger index)
{
	GMTITraceRenderEncoderSetVertexBufferOffsetatindex2Handler.Trace(Obj, offset, index);
	Original(Obj, Cmd, offset, index);
}

INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetvertexbuffersOffsetsWithrange,  void,   const id <MTLBuffer>  * buffers,const NSUInteger * offsets,NSRange range)
{
	Original(Obj, Cmd, buffers, offsets, range);
}

struct MTITraceRenderEncoderSetvertextextureatindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetvertextextureatindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setvertextextureatindex")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setVertexTexture:MTITrace::Get().FetchObject(texture) atIndex:index];
	}
};
static MTITraceRenderEncoderSetvertextextureatindexHandler GMTITraceRenderEncoderSetvertextextureatindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetvertextextureAtindex,  void,    id <MTLTexture> texture,NSUInteger index)
{
	GMTITraceRenderEncoderSetvertextextureatindexHandler.Trace(Obj, texture, index);
	Original(Obj, Cmd, texture, index);
}

INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetvertextexturesWithrange,  void,   const id <MTLTexture>  * textures,NSRange range)
{
	Original(Obj, Cmd, textures, range);
}

struct MTITraceRenderEncoderSetvertexsamplerstateatindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetvertexsamplerstateatindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setsamplerstateatindex")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setVertexSamplerState:MTITrace::Get().FetchObject(sampler) atIndex:index];
	}
};
static MTITraceRenderEncoderSetvertexsamplerstateatindexHandler GMTITraceRenderEncoderSetvertexsamplerstateatindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetvertexsamplerstateAtindex,  void,    id <MTLSamplerState> sampler,NSUInteger index)
{
	GMTITraceRenderEncoderSetvertexsamplerstateatindexHandler.Trace(Obj, sampler, index);
	Original(Obj, Cmd, sampler, index);
}

INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetvertexsamplerstatesWithrange,  void,   const id <MTLSamplerState>  * samplers,NSRange range)
{
	Original(Obj, Cmd, samplers, range);
}

struct MTITraceRenderEncoderSetvertexsamplerstatelodminclamplodmaxclampatindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetvertexsamplerstatelodminclamplodmaxclampatindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setvertexsamplerstatelodminclamplodmaxclampatindex")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setVertexSamplerState:MTITrace::Get().FetchObject(sampler) lodMinClamp:lodMinClamp lodMaxClamp:lodMaxClamp atIndex:index];
	}
};
static MTITraceRenderEncoderSetvertexsamplerstatelodminclamplodmaxclampatindexHandler GMTITraceRenderEncoderSetvertexsamplerstatelodminclamplodmaxclampatindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetvertexsamplerstateLodminclampLodmaxclampAtindex,  void,    id <MTLSamplerState> sampler,float lodMinClamp,float lodMaxClamp,NSUInteger index)
{
	GMTITraceRenderEncoderSetvertexsamplerstatelodminclamplodmaxclampatindexHandler.Trace(Obj, sampler, lodMinClamp, lodMaxClamp, index);
	Original(Obj, Cmd, sampler, lodMinClamp, lodMaxClamp, index);
}

INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetvertexsamplerstatesLodminclampsLodmaxclampsWithrange,  void,   const id <MTLSamplerState>  * samplers,const float * lodMinClamps,const float * lodMaxClamps,NSRange range)
{
	Original(Obj, Cmd, samplers, lodMinClamps, lodMaxClamps, range);
}


struct MTITraceRenderEncoderSetviewportHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetviewportHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setviewport")
	{
		
	}
	
	void Trace(id Object, MTLPPViewport viewport)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << viewport.originX;
		fs << viewport.originY;
		fs << viewport.width;
		fs << viewport.height;
		fs << viewport.znear;
		fs << viewport.zfar;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTLViewport viewport;
		fs >> viewport.originX;
		fs >> viewport.originY;
		fs >> viewport.width;
		fs >> viewport.height;
		fs >> viewport.znear;
		fs >> viewport.zfar;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setViewport:viewport];
	}
};
static MTITraceRenderEncoderSetviewportHandler GMTITraceRenderEncoderSetviewportHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setviewport,  void,   MTLPPViewport viewport)
{
	GMTITraceRenderEncoderSetviewportHandler.Trace(Obj, viewport);
	Original(Obj, Cmd, viewport);
}

struct MTITraceRenderEncoderSetviewportsHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetviewportsHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setviewports")
	{
		
	}
	
	void Trace(id Object, const MTLPPViewport * viewports,NSUInteger count)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << count;
		for (unsigned i = 0; i < count; i++)
		{
			fs << viewports[i].originX;
			fs << viewports[i].originY;
			fs << viewports[i].width;
			fs << viewports[i].height;
			fs << viewports[i].znear;
			fs << viewports[i].zfar;
		}
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger count;
		fs >> count;
		
		std::vector<MTLViewport> viewports;
		for (unsigned i = 0; i < count; i++)
		{
			MTLViewport viewport;
			fs >> viewport.originX;
			fs >> viewport.originY;
			fs >> viewport.width;
			fs >> viewport.height;
			fs >> viewport.znear;
			fs >> viewport.zfar;
			viewports.push_back(viewport);
		}
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setViewports:viewports.data() count:count];
	}
};
static MTITraceRenderEncoderSetviewportsHandler GMTITraceRenderEncoderSetviewportsHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetviewportsCount,  void,   const MTLPPViewport * viewports,NSUInteger count)
{
	GMTITraceRenderEncoderSetviewportsHandler.Trace(Obj, viewports, count);
	Original(Obj, Cmd, viewports, count);
}

struct MTITraceRenderEncoderSetfrontfacingwindingHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetfrontfacingwindingHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setfrontfacingwinding")
	{
		
	}
	
	void Trace(id Object, MTLWinding frontFacingWinding)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << frontFacingWinding;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger frontFacingWinding;
		fs >> frontFacingWinding;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setFrontFacingWinding:(MTLWinding)frontFacingWinding];
	}
};
static MTITraceRenderEncoderSetfrontfacingwindingHandler GMTITraceRenderEncoderSetfrontfacingwindingHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setfrontfacingwinding,  void,   MTLWinding frontFacingWinding)
{
	GMTITraceRenderEncoderSetfrontfacingwindingHandler.Trace(Obj, frontFacingWinding);
	Original(Obj, Cmd, frontFacingWinding);
}

struct MTITraceRenderEncoderSetcullmodeHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetcullmodeHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setcullmode")
	{
		
	}
	
	void Trace(id Object, MTLCullMode cullMode)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << cullMode;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger cullMode;
		fs >> cullMode;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setCullMode:(MTLCullMode)cullMode];
	}
};
static MTITraceRenderEncoderSetcullmodeHandler GMTITraceRenderEncoderSetcullmodeHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setcullmode,  void,   MTLCullMode cullMode)
{
	GMTITraceRenderEncoderSetcullmodeHandler.Trace(Obj, cullMode);
	Original(Obj, Cmd, cullMode);
}

struct MTITraceRenderEncoderSetdepthclipmodeHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetdepthclipmodeHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setdepthclipmode")
	{
		
	}
	
	void Trace(id Object, MTLDepthClipMode depthClipMode)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << depthClipMode;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger depthClipMode;
		fs >> depthClipMode;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setDepthClipMode:(MTLDepthClipMode)depthClipMode];
	}
};
static MTITraceRenderEncoderSetdepthclipmodeHandler GMTITraceRenderEncoderSetdepthclipmodeHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setdepthclipmode,  void,   MTLDepthClipMode depthClipMode)
{
	GMTITraceRenderEncoderSetdepthclipmodeHandler.Trace(Obj, depthClipMode);
	Original(Obj, Cmd, depthClipMode);
}

struct MTITraceRenderEncoderSetdepthbiasSlopescaleClampHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetdepthbiasSlopescaleClampHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetdepthbiasSlopescaleClamp")
	{
		
	}
	
	void Trace(id Object, float depthBias,float slopeScale,float clamp)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << depthBias;
		fs << slopeScale;
		fs << clamp;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		float depthBias;
		float slopeScale;
		float clamp;
		fs >> depthBias;
		fs >> slopeScale;
		fs >> clamp;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setDepthBias:depthBias slopeScale:slopeScale clamp:clamp];
	}
};
static MTITraceRenderEncoderSetdepthbiasSlopescaleClampHandler GMTITraceRenderEncoderSetdepthbiasSlopescaleClampHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetdepthbiasSlopescaleClamp,  void,   float depthBias,float slopeScale,float clamp)
{
	GMTITraceRenderEncoderSetdepthbiasSlopescaleClampHandler.Trace(Obj, depthBias, slopeScale, clamp);
	Original(Obj, Cmd, depthBias, slopeScale, clamp);
}

struct MTITraceRenderEncoderSetscissorrectHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetscissorrectHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setscissorrect")
	{
		
	}
	
	void Trace(id Object, MTLPPScissorRect rect)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << rect.x;
		fs << rect.y;
		fs << rect.width;
		fs << rect.height;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTLScissorRect rect;
		fs >> rect.x;
		fs >> rect.y;
		fs >> rect.width;
		fs >> rect.height;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setScissorRect:rect];
	}
};
static MTITraceRenderEncoderSetscissorrectHandler GMTITraceRenderEncoderSetscissorrectHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setscissorrect,  void,   MTLPPScissorRect rect)
{
	GMTITraceRenderEncoderSetscissorrectHandler.Trace(Obj, rect);
	Original(Obj, Cmd, rect);
}

struct MTITraceRenderEncoderSetscissorrectsCountHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetscissorrectsCountHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetscissorrectsCount")
	{
		
	}
	
	void Trace(id Object, const MTLPPScissorRect * scissorRects,NSUInteger count)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << count;
		
		for (unsigned i = 0; i < count; i++)
		{
			fs << scissorRects[i].x;
			fs << scissorRects[i].y;
			fs << scissorRects[i].width;
			fs << scissorRects[i].height;
		}
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger count;
		fs >> count;
		
		std::vector<MTLScissorRect> Rects;
		for (unsigned i = 0; i < count; i++)
		{
			MTLScissorRect rect;
			fs >> rect.x;
			fs >> rect.y;
			fs >> rect.width;
			fs >> rect.height;
			Rects.push_back(rect);
		}
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setScissorRects:Rects.data() count:count];
	}
};
static MTITraceRenderEncoderSetscissorrectsCountHandler GMTITraceRenderEncoderSetscissorrectsCountHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetscissorrectsCount,  void,   const MTLPPScissorRect * scissorRects,NSUInteger count)
{
	GMTITraceRenderEncoderSetscissorrectsCountHandler.Trace(Obj, scissorRects, count);
	Original(Obj, Cmd, scissorRects, count);
}

struct MTITraceRenderEncoderSettrianglefillmodeHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSettrianglefillmodeHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Settrianglefillmode")
	{
		
	}
	
	void Trace(id Object, MTLTriangleFillMode fillMode)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << fillMode;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger fillMode;
		fs >> fillMode;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setTriangleFillMode:(MTLTriangleFillMode)fillMode];
	}
};
static MTITraceRenderEncoderSettrianglefillmodeHandler GMTITraceRenderEncoderSettrianglefillmodeHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Settrianglefillmode,  void,   MTLTriangleFillMode fillMode)
{
	GMTITraceRenderEncoderSettrianglefillmodeHandler.Trace(Obj, fillMode);
	Original(Obj, Cmd, fillMode);
}

struct MTITraceRenderEncoderSetFragmentbyteslengthatindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetFragmentbyteslengthatindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetfragmentbytesLengthAtindex")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setFragmentBytes:Data.Backing.data() length:length atIndex:index];
	}
};
static MTITraceRenderEncoderSetFragmentbyteslengthatindexHandler GMTITraceRenderEncoderSetFragmentbyteslengthatindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetfragmentbytesLengthAtindex,  void,   const void * bytes,NSUInteger length,NSUInteger index)
{
	GMTITraceRenderEncoderSetFragmentbyteslengthatindexHandler.Trace(Obj, bytes, length, index);
	Original(Obj, Cmd, bytes, length, index);
}

struct MTITraceRenderEncoderSetfragmentbufferoffsetatindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetfragmentbufferoffsetatindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetfragmentbufferOffsetAtindex")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setFragmentBuffer:MTITrace::Get().FetchObject(Buffer) offset:offset atIndex:index];
	}
};
static MTITraceRenderEncoderSetfragmentbufferoffsetatindexHandler GMTITraceRenderEncoderSetfragmentbufferoffsetatindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetfragmentbufferOffsetAtindex,  void,    id <MTLBuffer> buffer,NSUInteger offset,NSUInteger index)
{
	GMTITraceRenderEncoderSetfragmentbufferoffsetatindexHandler.Trace(Obj, buffer, offset, index);
	Original(Obj, Cmd, buffer, offset,index);
}

struct MTITraceRenderEncoderSetfragmentbufferoffsetAtindex2Handler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetfragmentbufferoffsetAtindex2Handler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetfragmentbufferoffsetAtindex2")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setFragmentBufferOffset:offset atIndex:index];
	}
};
static MTITraceRenderEncoderSetfragmentbufferoffsetAtindex2Handler GMTITraceRenderEncoderSetfragmentbufferoffsetAtindex2Handler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetfragmentbufferoffsetAtindex,  void,   NSUInteger offset,NSUInteger index)
{
	GMTITraceRenderEncoderSetfragmentbufferoffsetAtindex2Handler.Trace(Obj, offset, index);
	Original(Obj, Cmd, offset, index);
}

INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetfragmentbuffersOffsetsWithrange,  void,   const id <MTLBuffer>  * buffers,const NSUInteger * offsets,NSRange range)
{
	Original(Obj, Cmd, buffers, offsets, range);
}

struct MTITraceRenderEncoderSetfragmenttextureAtindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetfragmenttextureAtindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetfragmenttextureAtindex")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setFragmentTexture:MTITrace::Get().FetchObject(texture) atIndex:index];
	}
};
static MTITraceRenderEncoderSetfragmenttextureAtindexHandler GMTITraceRenderEncoderSetfragmenttextureAtindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetfragmenttextureAtindex,  void,    id <MTLTexture> texture,NSUInteger index)
{
	GMTITraceRenderEncoderSetfragmenttextureAtindexHandler.Trace(Obj, texture, index);
	Original(Obj, Cmd, texture, index);
}

INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetfragmenttexturesWithrange,  void,   const id <MTLTexture>  * textures,NSRange range)
{
	Original(Obj, Cmd, textures, range);
}

struct MTITraceRenderEncoderSetfragmentsamplerstateAtindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetfragmentsamplerstateAtindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetfragmentsamplerstateAtindex")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setFragmentSamplerState:MTITrace::Get().FetchObject(sampler) atIndex:index];
	}
};
static MTITraceRenderEncoderSetfragmentsamplerstateAtindexHandler GMTITraceRenderEncoderSetfragmentsamplerstateAtindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetfragmentsamplerstateAtindex,  void,    id <MTLSamplerState> sampler,NSUInteger index)
{
	GMTITraceRenderEncoderSetfragmentsamplerstateAtindexHandler.Trace(Obj, sampler, index);
	Original(Obj, Cmd, sampler, index);
}

INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetfragmentsamplerstatesWithrange,  void,   const id <MTLSamplerState>  * samplers,NSRange range)
{
	Original(Obj, Cmd, samplers, range);
}

struct MTITraceRenderEncoderSetfragmentsamplerstateLodminclampLodmaxclampAtindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetfragmentsamplerstateLodminclampLodmaxclampAtindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetfragmentsamplerstateLodminclampLodmaxclampAtindex")
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setFragmentSamplerState:MTITrace::Get().FetchObject(sampler) lodMinClamp:lodMinClamp lodMaxClamp:lodMaxClamp atIndex:index];
	}
};
static MTITraceRenderEncoderSetfragmentsamplerstateLodminclampLodmaxclampAtindexHandler GMTITraceRenderEncoderSetfragmentsamplerstateLodminclampLodmaxclampAtindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetfragmentsamplerstateLodminclampLodmaxclampAtindex,  void,    id <MTLSamplerState> sampler,float lodMinClamp,float lodMaxClamp,NSUInteger index)
{
	GMTITraceRenderEncoderSetfragmentsamplerstateLodminclampLodmaxclampAtindexHandler.Trace(Obj, sampler, lodMinClamp, lodMaxClamp, index);
	Original(Obj, Cmd, sampler, lodMinClamp, lodMaxClamp, index);
}

INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetfragmentsamplerstatesLodminclampsLodmaxclampsWithrange,  void,   const id <MTLSamplerState>  * samplers,const float * lodMinClamps,const float * lodMaxClamps,NSRange range)
{
	Original(Obj, Cmd, samplers, lodMinClamps, lodMaxClamps, range);
}

struct MTITraceRenderEncoderSetblendcolorredGreenBlueAlphaHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetblendcolorredGreenBlueAlphaHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetblendcolorredGreenBlueAlpha")
	{
		
	}
	
	void Trace(id Object, float red,float green,float blue,float alpha)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << red;
		fs << green;
		fs << blue;
		fs << alpha;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		float red;
		float green;
		float blue;
		float alpha;
		fs >> red;
		fs >> green;
		fs >> blue;
		fs >> alpha;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setBlendColorRed:red green:green blue:blue alpha:alpha];
	}
};
static MTITraceRenderEncoderSetblendcolorredGreenBlueAlphaHandler GMTITraceRenderEncoderSetblendcolorredGreenBlueAlphaHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetblendcolorredGreenBlueAlpha,  void,   float red,float green,float blue,float alpha)
{
	GMTITraceRenderEncoderSetblendcolorredGreenBlueAlphaHandler.Trace(Obj, red, green, blue, alpha);
	Original(Obj, Cmd, red, green, blue, alpha);
}

struct MTITraceRenderEncoderSetdepthstencilstateHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetdepthstencilstateHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setdepthstencilstate")
	{
		
	}
	
	void Trace(id Object, id <MTLDepthStencilState> depthStencilState)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)depthStencilState;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t depthStencilState;
		fs >> depthStencilState;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setDepthStencilState:MTITrace::Get().FetchObject(depthStencilState)];
	}
};
static MTITraceRenderEncoderSetdepthstencilstateHandler GMTITraceRenderEncoderSetdepthstencilstateHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setdepthstencilstate,  void,    id <MTLDepthStencilState> depthStencilState)
{
	GMTITraceRenderEncoderSetdepthstencilstateHandler.Trace(Obj, depthStencilState);
	Original(Obj, Cmd, depthStencilState);
}

struct MTITraceRenderEncoderSetstencilreferencevalueHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetstencilreferencevalueHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setstencilreferencevalue")
	{
		
	}
	
	void Trace(id Object, uint32_t referenceValue)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << referenceValue;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uint32_t referenceValue;
		fs >> referenceValue;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setStencilReferenceValue:referenceValue];
	}
};
static MTITraceRenderEncoderSetstencilreferencevalueHandler GMTITraceRenderEncoderSetstencilreferencevalueHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setstencilreferencevalue,  void,   uint32_t referenceValue)
{
	GMTITraceRenderEncoderSetstencilreferencevalueHandler.Trace(Obj, referenceValue);
	Original(Obj, Cmd, referenceValue);
}

struct MTITraceRenderEncoderSetstencilfrontreferencevalueBackreferencevalueHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetstencilfrontreferencevalueBackreferencevalueHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetstencilfrontreferencevalueBackreferencevalue")
	{
		
	}
	
	void Trace(id Object, uint32_t frontReferenceValue,uint32_t backReferenceValue)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << frontReferenceValue;
		fs << backReferenceValue;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uint32_t frontReferenceValue;
		uint32_t backReferenceValue;
		fs >> frontReferenceValue;
		fs >> backReferenceValue;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setStencilFrontReferenceValue:frontReferenceValue backReferenceValue:backReferenceValue];
	}
};
static MTITraceRenderEncoderSetstencilfrontreferencevalueBackreferencevalueHandler GMTITraceRenderEncoderSetstencilfrontreferencevalueBackreferencevalueHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetstencilfrontreferencevalueBackreferencevalue,  void,   uint32_t frontReferenceValue,uint32_t backReferenceValue)
{
	GMTITraceRenderEncoderSetstencilfrontreferencevalueBackreferencevalueHandler.Trace(Obj, frontReferenceValue, backReferenceValue);
	Original(Obj, Cmd, frontReferenceValue, backReferenceValue);
}

struct MTITraceRenderEncoderSetvisibilityresultmodeOffsetHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderSetvisibilityresultmodeOffsetHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetvisibilityresultmodeOffset")
	{
		
	}
	
	void Trace(id Object, MTLVisibilityResultMode mode,NSUInteger offset)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << mode;
		fs << offset;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger mode;
		NSUInteger offset;
		fs >> mode;
		fs >> offset;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setVisibilityResultMode:(MTLVisibilityResultMode)mode offset:offset];
	}
};
static MTITraceRenderEncoderSetvisibilityresultmodeOffsetHandler GMTITraceRenderEncoderSetvisibilityresultmodeOffsetHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetvisibilityresultmodeOffset,  void,   MTLVisibilityResultMode mode,NSUInteger offset)
{
	GMTITraceRenderEncoderSetvisibilityresultmodeOffsetHandler.Trace(Obj, mode, offset);
	Original(Obj, Cmd, mode, offset);
}

struct MTITraceRenderCommandEncoderSetcolorstoreactionAtindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderCommandEncoderSetcolorstoreactionAtindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetcolorstoreactionAtindex")
	{
		
	}
	
	void Trace(id Object, MTLStoreAction storeAction,NSUInteger colorAttachmentIndex)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeAction;
		fs << colorAttachmentIndex;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeAction;
		NSUInteger colorAttachmentIndex;
		fs >> storeAction;
		fs >> colorAttachmentIndex;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setColorStoreAction:(MTLStoreAction)storeAction atIndex:colorAttachmentIndex];
	}
};
static MTITraceRenderCommandEncoderSetcolorstoreactionAtindexHandler GMTITraceRenderCommandEncoderSetcolorstoreactionAtindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetcolorstoreactionAtindex,  void,   MTLStoreAction storeAction,NSUInteger colorAttachmentIndex)
{
	GMTITraceRenderCommandEncoderSetcolorstoreactionAtindexHandler.Trace(Obj, storeAction, colorAttachmentIndex);
	Original(Obj, Cmd, storeAction, colorAttachmentIndex);
}

struct MTITraceRenderCommandEncoderSetdepthstoreactionHandler : public MTITraceCommandHandler
{
	MTITraceRenderCommandEncoderSetdepthstoreactionHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setdepthstoreaction")
	{
		
	}
	
	void Trace(id Object, MTLStoreAction storeAction)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeAction;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeAction;
		fs >> storeAction;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setDepthStoreAction:(MTLStoreAction)storeAction];
	}
};
static MTITraceRenderCommandEncoderSetdepthstoreactionHandler GMTITraceRenderCommandEncoderSetdepthstoreactionHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setdepthstoreaction,  void,   MTLStoreAction storeAction)
{
	GMTITraceRenderCommandEncoderSetdepthstoreactionHandler.Trace(Obj, storeAction);
	Original(Obj, Cmd, storeAction);
}

struct MTITraceRenderCommandEncoderSetstencilstoreactionHandler : public MTITraceCommandHandler
{
	MTITraceRenderCommandEncoderSetstencilstoreactionHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setstencilstoreaction")
	{
		
	}
	
	void Trace(id Object, MTLStoreAction storeAction)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeAction;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeAction;
		fs >> storeAction;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setStencilStoreAction:(MTLStoreAction)storeAction];
	}
};
static MTITraceRenderCommandEncoderSetstencilstoreactionHandler GMTITraceRenderCommandEncoderSetstencilstoreactionHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setstencilstoreaction,  void,   MTLStoreAction storeAction)
{
	GMTITraceRenderCommandEncoderSetstencilstoreactionHandler.Trace(Obj, storeAction);
	Original(Obj, Cmd, storeAction);
}

struct MTITraceRenderCommandEncoderSetcolorstoreactionoptionsAtindexHandler : public MTITraceCommandHandler
{
	MTITraceRenderCommandEncoderSetcolorstoreactionoptionsAtindexHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "SetcolorstoreactionoptionsAtindex")
	{
		
	}
	
	void Trace(id Object, MTLStoreActionOptions storeActionOptions,NSUInteger colorAttachmentIndex)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeActionOptions;
		fs << colorAttachmentIndex;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeActionOptions;
		NSUInteger colorAttachmentIndex;
		fs >> storeActionOptions;
		fs >> colorAttachmentIndex;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setColorStoreActionOptions:(MTLStoreActionOptions)storeActionOptions atIndex:colorAttachmentIndex];
	}
};
static MTITraceRenderCommandEncoderSetcolorstoreactionoptionsAtindexHandler GMTITraceRenderCommandEncoderSetcolorstoreactionoptionsAtindexHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SetcolorstoreactionoptionsAtindex,  void,   MTLStoreActionOptions storeActionOptions,NSUInteger colorAttachmentIndex)
{
	GMTITraceRenderCommandEncoderSetcolorstoreactionoptionsAtindexHandler.Trace(Obj, storeActionOptions, colorAttachmentIndex);
	Original(Obj, Cmd, storeActionOptions,colorAttachmentIndex);
}

struct MTITraceRenderCommandEncoderSetdepthstoreactionoptionsHandler : public MTITraceCommandHandler
{
	MTITraceRenderCommandEncoderSetdepthstoreactionoptionsHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setdepthstoreactionoptions")
	{
		
	}
	
	void Trace(id Object, MTLStoreActionOptions storeAction)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeAction;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeAction;
		fs >> storeAction;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setDepthStoreActionOptions:(MTLStoreActionOptions)storeAction];
	}
};
static MTITraceRenderCommandEncoderSetdepthstoreactionoptionsHandler GMTITraceRenderCommandEncoderSetdepthstoreactionoptionsHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setdepthstoreactionoptions,  void,   MTLStoreActionOptions storeActionOptions)
{
	GMTITraceRenderCommandEncoderSetdepthstoreactionoptionsHandler.Trace(Obj, storeActionOptions);
	Original(Obj, Cmd, storeActionOptions);
}

struct MTITraceRenderCommandEncoderSetstencilstoreactionoptionsHandler : public MTITraceCommandHandler
{
	MTITraceRenderCommandEncoderSetstencilstoreactionoptionsHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Setstencilstoreactionoptions")
	{
		
	}
	
	void Trace(id Object, MTLStoreActionOptions storeAction)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeAction;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeAction;
		fs >> storeAction;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setStencilStoreActionOptions:(MTLStoreActionOptions)storeAction];
	}
};
static MTITraceRenderCommandEncoderSetstencilstoreactionoptionsHandler GMTITraceRenderCommandEncoderSetstencilstoreactionoptionsHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Setstencilstoreactionoptions,  void,   MTLStoreActionOptions storeActionOptions)
{
	GMTITraceRenderCommandEncoderSetstencilstoreactionoptionsHandler.Trace(Obj, storeActionOptions);
	Original(Obj, Cmd, storeActionOptions);
}

struct MTITraceRenderEncoderDrawprimitivesVertexstartVertexcountInstancecountHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawprimitivesVertexstartVertexcountInstancecountHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawprimitivesVertexstartVertexcountInstancecount")
	{
		
	}
	
	void Trace(id Object, MTLPrimitiveType primitiveType,NSUInteger vertexStart,NSUInteger vertexCount,NSUInteger instanceCount)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << primitiveType;
		fs << vertexStart;
		fs << vertexCount;
		fs << instanceCount;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger primitiveType;
		NSUInteger vertexStart;
		NSUInteger vertexCount;
		NSUInteger instanceCount;
		fs >> primitiveType;
		fs >> vertexStart;
		fs >> vertexCount;
		fs >> instanceCount;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:vertexStart vertexCount:vertexCount instanceCount:instanceCount];
	}
};
static MTITraceRenderEncoderDrawprimitivesVertexstartVertexcountInstancecountHandler GMTITraceRenderEncoderDrawprimitivesVertexstartVertexcountInstancecountHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawprimitivesVertexstartVertexcountInstancecount,  void,   MTLPrimitiveType primitiveType,NSUInteger vertexStart,NSUInteger vertexCount,NSUInteger instanceCount)
{
	GMTITraceRenderEncoderDrawprimitivesVertexstartVertexcountInstancecountHandler.Trace(Obj, primitiveType, vertexStart, vertexCount, instanceCount);
	Original(Obj, Cmd, primitiveType,vertexStart,vertexCount,instanceCount);
}

struct MTITraceRenderEncoderDrawprimitivesVertexstartVertexcountHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawprimitivesVertexstartVertexcountHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawprimitivesVertexstartVertexcount")
	{
		
	}
	
	void Trace(id Object, MTLPrimitiveType primitiveType,NSUInteger vertexStart,NSUInteger vertexCount)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << primitiveType;
		fs << vertexStart;
		fs << vertexCount;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger primitiveType;
		NSUInteger vertexStart;
		NSUInteger vertexCount;
		fs >> primitiveType;
		fs >> vertexStart;
		fs >> vertexCount;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:vertexStart vertexCount:vertexCount];
	}
};
static MTITraceRenderEncoderDrawprimitivesVertexstartVertexcountHandler GMTITraceRenderEncoderDrawprimitivesVertexstartVertexcountHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawprimitivesVertexstartVertexcount,  void,   MTLPrimitiveType primitiveType,NSUInteger vertexStart,NSUInteger vertexCount)
{
	GMTITraceRenderEncoderDrawprimitivesVertexstartVertexcountHandler.Trace(Obj, primitiveType,vertexStart,vertexCount);
	Original(Obj, Cmd, primitiveType,vertexStart,vertexCount);
}

struct MTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecount")
	{
		
	}
	
	void Trace(id Object, MTLPrimitiveType primitiveType,NSUInteger indexCount,MTLIndexType indexType,id <MTLBuffer> indexBuffer,NSUInteger indexBufferOffset,NSUInteger instanceCount)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << primitiveType;
		fs << indexCount;
		fs << indexType;
		fs << (uintptr_t)indexBuffer;
		fs << indexBufferOffset;
		fs << instanceCount;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger primitiveType;
		NSUInteger indexCount;
		NSUInteger indexType;
		uintptr_t indexBuffer;
		NSUInteger indexBufferOffset;
		NSUInteger instanceCount;
		fs >> primitiveType;
		fs >> indexCount;
		fs >> indexType;
		fs >> indexBuffer;
		fs >> indexBufferOffset;
		fs >> instanceCount;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:indexCount indexType:(MTLIndexType)indexType indexBuffer:MTITrace::Get().FetchObject(indexBuffer) indexBufferOffset:indexBufferOffset instanceCount:instanceCount];
	}
};
static MTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountHandler GMTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecount,  void,   MTLPrimitiveType primitiveType,NSUInteger indexCount,MTLIndexType indexType,id <MTLBuffer> indexBuffer,NSUInteger indexBufferOffset,NSUInteger instanceCount)
{
	GMTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountHandler.Trace(Obj, primitiveType, indexCount, indexType, indexBuffer, indexBufferOffset, instanceCount);
	Original(Obj, Cmd, primitiveType,indexCount,indexType,indexBuffer,indexBufferOffset,instanceCount);
}

struct MTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffset")
	{
		
	}
	
	void Trace(id Object, MTLPrimitiveType primitiveType,NSUInteger indexCount,MTLIndexType indexType,id <MTLBuffer> indexBuffer,NSUInteger indexBufferOffset)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << primitiveType;
		fs << indexCount;
		fs << indexType;
		fs << (uintptr_t)indexBuffer;
		fs << indexBufferOffset;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger primitiveType;
		NSUInteger indexCount;
		NSUInteger indexType;
		uintptr_t indexBuffer;
		NSUInteger indexBufferOffset;
		fs >> primitiveType;
		fs >> indexCount;
		fs >> indexType;
		fs >> indexBuffer;
		fs >> indexBufferOffset;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:indexCount indexType:(MTLIndexType)indexType indexBuffer:MTITrace::Get().FetchObject(indexBuffer) indexBufferOffset:indexBufferOffset];
	}
};
static MTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetHandler GMTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffset,  void,   MTLPrimitiveType primitiveType,NSUInteger indexCount,MTLIndexType indexType,id <MTLBuffer> indexBuffer,NSUInteger indexBufferOffset)
{
	GMTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetHandler.Trace(Obj, primitiveType, indexCount, indexType, indexBuffer, indexBufferOffset);
	Original(Obj, Cmd, primitiveType,indexCount,indexType,indexBuffer,indexBufferOffset);
}

struct MTITraceRenderEncoderDrawprimitivesVertexstartVertexcountInstancecountBaseinstanceHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawprimitivesVertexstartVertexcountInstancecountBaseinstanceHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawprimitivesVertexstartVertexcountInstancecountBaseinstance")
	{
		
	}
	
	void Trace(id Object, MTLPrimitiveType primitiveType,NSUInteger vertexStart,NSUInteger vertexCount,NSUInteger instanceCount,NSUInteger baseInstance)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << primitiveType;
		fs << vertexStart;
		fs << vertexCount;
		fs << instanceCount;
		fs << baseInstance;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger primitiveType;
		NSUInteger vertexStart;
		NSUInteger vertexCount;
		NSUInteger instanceCount;
		NSUInteger baseInstance;
		fs >> primitiveType;
		fs >> vertexStart;
		fs >> vertexCount;
		fs >> instanceCount;
		fs >> baseInstance;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:vertexStart vertexCount:vertexCount instanceCount:instanceCount baseInstance:baseInstance];
	}
};
static MTITraceRenderEncoderDrawprimitivesVertexstartVertexcountInstancecountBaseinstanceHandler GMTITraceRenderEncoderDrawprimitivesVertexstartVertexcountInstancecountBaseinstanceHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawprimitivesVertexstartVertexcountInstancecountBaseinstance,  void,   MTLPrimitiveType primitiveType,NSUInteger vertexStart,NSUInteger vertexCount,NSUInteger instanceCount,NSUInteger baseInstance)
{
	GMTITraceRenderEncoderDrawprimitivesVertexstartVertexcountInstancecountBaseinstanceHandler.Trace(Obj, primitiveType, vertexStart, vertexCount, instanceCount, baseInstance);
	Original(Obj, Cmd, primitiveType,vertexStart,vertexCount,instanceCount,baseInstance);
}

struct MTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountBasevertexBaseinstanceHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountBasevertexBaseinstanceHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountBasevertexBaseinstance")
	{
		
	}
	
	void Trace(id Object, MTLPrimitiveType primitiveType,NSUInteger indexCount,MTLIndexType indexType,id <MTLBuffer> indexBuffer,NSUInteger indexBufferOffset,NSUInteger instanceCount,NSInteger baseVertex,NSUInteger baseInstance)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << primitiveType;
		fs << indexCount;
		fs << indexType;
		fs << (uintptr_t)indexBuffer;
		fs << indexBufferOffset;
		fs << instanceCount;
		fs << baseVertex;
		fs << baseInstance;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger primitiveType;
		NSUInteger indexCount;
		NSUInteger indexType;
		uintptr_t indexBuffer;
		NSUInteger indexBufferOffset;
		NSUInteger instanceCount;
		NSInteger baseVertex;
		NSUInteger baseInstance;
		fs >> primitiveType;
		fs >> indexCount;
		fs >> indexType;
		fs >> indexBuffer;
		fs >> indexBufferOffset;
		fs >> instanceCount;
		fs >> baseVertex;
		fs >> baseInstance;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:indexCount indexType:(MTLIndexType)indexType indexBuffer:MTITrace::Get().FetchObject(indexBuffer) indexBufferOffset:indexBufferOffset instanceCount:instanceCount baseVertex:baseVertex baseInstance:baseInstance];
	}
};
static MTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountBasevertexBaseinstanceHandler GMTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountBasevertexBaseinstanceHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountBasevertexBaseinstance,  void,   MTLPrimitiveType primitiveType,NSUInteger indexCount,MTLIndexType indexType,id <MTLBuffer> indexBuffer,NSUInteger indexBufferOffset,NSUInteger instanceCount,NSInteger baseVertex,NSUInteger baseInstance)
{
	GMTITraceRenderEncoderDrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountBasevertexBaseinstanceHandler.Trace(Obj, primitiveType, indexCount, indexType, indexBuffer, indexBufferOffset, instanceCount, baseVertex, baseInstance);
	Original(Obj, Cmd, primitiveType,indexCount,indexType,indexBuffer,indexBufferOffset,instanceCount,baseVertex,baseInstance);
}

struct MTITraceRenderEncoderDrawprimitivesIndirectbufferIndirectbufferoffsetHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawprimitivesIndirectbufferIndirectbufferoffsetHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawprimitivesIndirectbufferIndirectbufferoffset")
	{
		
	}
	
	void Trace(id Object, MTLPrimitiveType primitiveType,id <MTLBuffer> indirectBuffer,NSUInteger indirectBufferOffset)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << primitiveType;
		fs << (uintptr_t)indirectBuffer;
		fs << indirectBufferOffset;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger primitiveType;
		uintptr_t indirectBuffer;
		NSUInteger indirectBufferOffset;
		fs >> primitiveType;
		fs >> indirectBuffer;
		fs >> indirectBufferOffset;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawPrimitives:(MTLPrimitiveType)primitiveType indirectBuffer:MTITrace::Get().FetchObject(indirectBuffer) indirectBufferOffset:indirectBufferOffset];
	}
};
static MTITraceRenderEncoderDrawprimitivesIndirectbufferIndirectbufferoffsetHandler GMTITraceRenderEncoderDrawprimitivesIndirectbufferIndirectbufferoffsetHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawprimitivesIndirectbufferIndirectbufferoffset,  void,   MTLPrimitiveType primitiveType,id <MTLBuffer> indirectBuffer,NSUInteger indirectBufferOffset)
{
	GMTITraceRenderEncoderDrawprimitivesIndirectbufferIndirectbufferoffsetHandler.Trace(Obj, primitiveType, indirectBuffer, indirectBufferOffset);
	Original(Obj, Cmd, primitiveType,indirectBuffer,indirectBufferOffset);
}

struct MTITraceRenderEncoderDrawindexedprimitivesIndextypeIndexbufferIndexbufferoffsetIndirectbufferIndirectbufferoffsetHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawindexedprimitivesIndextypeIndexbufferIndexbufferoffsetIndirectbufferIndirectbufferoffsetHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawindexedprimitivesIndextypeIndexbufferIndexbufferoffsetIndirectbufferIndirectbufferoffset")
	{
		
	}
	
	void Trace(id Object, MTLPrimitiveType primitiveType,MTLIndexType indexType,id <MTLBuffer> indexBuffer,NSUInteger indexBufferOffset,id <MTLBuffer> indirectBuffer,NSUInteger indirectBufferOffset)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << primitiveType;
		fs << indexType;
		fs << (uintptr_t)indexBuffer;
		fs << indexBufferOffset;
		fs << (uintptr_t)indirectBuffer;
		fs << indirectBufferOffset;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger primitiveType;
		NSUInteger indexType;
		uintptr_t indexBuffer;
		NSUInteger indexBufferOffset;
		uintptr_t indirectBuffer;
		NSUInteger indirectBufferOffset;
		fs >> primitiveType;
		fs >> indexType;
		fs >> indexBuffer;
		fs >> indexBufferOffset;
		fs >> indirectBuffer;
		fs >> indirectBufferOffset;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexType:(MTLIndexType)indexType indexBuffer:MTITrace::Get().FetchObject(indexBuffer) indexBufferOffset:indexBufferOffset indirectBuffer:MTITrace::Get().FetchObject(indirectBuffer) indirectBufferOffset:indirectBufferOffset];
	}
};
static MTITraceRenderEncoderDrawindexedprimitivesIndextypeIndexbufferIndexbufferoffsetIndirectbufferIndirectbufferoffsetHandler GMTITraceRenderEncoderDrawindexedprimitivesIndextypeIndexbufferIndexbufferoffsetIndirectbufferIndirectbufferoffsetHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawindexedprimitivesIndextypeIndexbufferIndexbufferoffsetIndirectbufferIndirectbufferoffset,  void,   MTLPrimitiveType primitiveType,MTLIndexType indexType,id <MTLBuffer> indexBuffer,NSUInteger indexBufferOffset,id <MTLBuffer> indirectBuffer,NSUInteger indirectBufferOffset)
{
	GMTITraceRenderEncoderDrawindexedprimitivesIndextypeIndexbufferIndexbufferoffsetIndirectbufferIndirectbufferoffsetHandler.Trace(Obj, primitiveType, indexType, indexBuffer, indexBufferOffset, indirectBuffer, indirectBufferOffset);
	Original(Obj, Cmd, primitiveType,indexType,indexBuffer,indexBufferOffset,indirectBuffer,indirectBufferOffset);
}

struct MTITraceRenderCommandEncoderTexturebarrierHandler : public MTITraceCommandHandler
{
	MTITraceRenderCommandEncoderTexturebarrierHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "Texturebarrier")
	{
		
	}
	
	void Trace(id Object)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeAction;
		fs >> storeAction;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) textureBarrier];
	}
};
static MTITraceRenderCommandEncoderTexturebarrierHandler GMTITraceRenderCommandEncoderTexturebarrierHandler;
INTERPOSE_DEFINITION_VOID( MTIRenderCommandEncoderTrace, Texturebarrier,  void)
{
	GMTITraceRenderCommandEncoderTexturebarrierHandler.Trace(Obj);
	Original(Obj, Cmd);
}

struct MTITraceRenderEncoderUpdateFenceHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderUpdateFenceHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "UpdatefenceAfterstages")
	{
		
	}
	
	void Trace(id Object, id<MTLFence> Res,MTLRenderStages stages)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Res;
		fs << stages;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Res;
		NSUInteger stages;
		fs >> Res;
		fs >> stages;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) updateFence:MTITrace::Get().FetchObject(Res) afterStages:(MTLRenderStages)stages];
	}
};
static MTITraceRenderEncoderUpdateFenceHandler GMTITraceRenderEncoderUpdateFenceHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, UpdatefenceAfterstages,  void,   id <MTLFence> fence,MTLRenderStages stages)
{
	GMTITraceRenderEncoderUpdateFenceHandler.Trace(Obj, fence, stages);
	Original(Obj, Cmd, fence,stages);
}

struct MTITraceRenderEncoderWaitForFenceHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderWaitForFenceHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "WaitForFence")
	{
		
	}
	
	void Trace(id Object, id<MTLFence> Res,MTLRenderStages stages)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Res;
		fs << stages;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Res;
		NSUInteger stages;
		fs >> Res;
		fs >> stages;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) waitForFence:MTITrace::Get().FetchObject(Res) beforeStages:(MTLRenderStages)stages];
	}
};
static MTITraceRenderEncoderWaitForFenceHandler GMTITraceRenderEncoderWaitForFenceHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, WaitforfenceBeforestages,  void,   id <MTLFence> fence,MTLRenderStages stages)
{
	GMTITraceRenderEncoderWaitForFenceHandler.Trace(Obj, fence, stages);
	Original(Obj, Cmd, fence,stages);
}

struct MTITraceRendererEncoderSettessellationfactorbufferOffsetInstancestrideHandler : public MTITraceCommandHandler
{
	MTITraceRendererEncoderSettessellationfactorbufferOffsetInstancestrideHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "SettessellationfactorbufferOffsetInstancestride")
	{
		
	}
	
	void Trace(id Object, id <MTLBuffer> buffer,NSUInteger offset,NSUInteger instanceStride)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)buffer;
		fs << offset;
		fs << instanceStride;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t buffer;
		NSUInteger offset;
		NSUInteger instanceStride;
		fs >> buffer;
		fs >> offset;
		fs >> instanceStride;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setTessellationFactorBuffer:MTITrace::Get().FetchObject(buffer) offset:offset instanceStride:instanceStride];
	}
};
static MTITraceRendererEncoderSettessellationfactorbufferOffsetInstancestrideHandler GMTITraceRendererEncoderSettessellationfactorbufferOffsetInstancestrideHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, SettessellationfactorbufferOffsetInstancestride,  void,    id <MTLBuffer> buffer,NSUInteger offset,NSUInteger instanceStride)
{
	GMTITraceRendererEncoderSettessellationfactorbufferOffsetInstancestrideHandler.Trace(Obj, buffer, offset, instanceStride);
	Original(Obj, Cmd, buffer,offset,instanceStride);
}

struct MTITraceRendererEncoderSettessellationfactorscaleHandler : public MTITraceCommandHandler
{
	MTITraceRendererEncoderSettessellationfactorscaleHandler()
	: MTITraceCommandHandler("MTLComputeCommandEncoder", "Settessellationfactorscale")
	{
		
	}
	
	void Trace(id Object, float scale)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << scale;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		float scale;
		fs >> scale;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setTessellationFactorScale:scale];
	}
};
static MTITraceRendererEncoderSettessellationfactorscaleHandler GMTITraceRendererEncoderSettessellationfactorscaleHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Settessellationfactorscale,  void,   float scale)
{
	GMTITraceRendererEncoderSettessellationfactorscaleHandler.Trace(Obj, scale);
	Original(Obj, Cmd, scale);
}

struct MTITraceRenderEncoderDrawpatchesHandler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawpatchesHandler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawpatchesPatchstartPatchcountPatchindexbufferPatchindexbufferoffsetInstancecountBaseinstance")
	{
		
	}
	
	void Trace(id Object, NSUInteger numberOfPatchControlPoints,NSUInteger patchStart,NSUInteger patchCount, id <MTLBuffer> patchIndexBuffer,NSUInteger patchIndexBufferOffset,NSUInteger instanceCount,NSUInteger baseInstance)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << numberOfPatchControlPoints;
		fs << patchStart;
		fs << patchCount;
		fs << (uintptr_t)patchIndexBuffer;
		fs << patchIndexBufferOffset;
		fs << instanceCount;
		fs << baseInstance;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger numberOfPatchControlPoints;
		NSUInteger patchStart;
		NSUInteger patchCount;
		uintptr_t patchIndexBuffer;
		NSUInteger patchIndexBufferOffset;
		NSUInteger instanceCount;
		NSUInteger baseInstance;
		fs >> numberOfPatchControlPoints;
		fs >> patchStart;
		fs >> patchCount;
		fs >> patchIndexBuffer;
		fs >> patchIndexBufferOffset;
		fs >> instanceCount;
		fs >> baseInstance;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawPatches:numberOfPatchControlPoints patchStart:patchStart patchCount:patchCount patchIndexBuffer:MTITrace::Get().FetchObject(patchIndexBuffer) patchIndexBufferOffset:patchIndexBufferOffset instanceCount:instanceCount baseInstance:baseInstance];
	}
};
static MTITraceRenderEncoderDrawpatchesHandler GMTITraceRenderEncoderDrawpatchesHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawpatchesPatchstartPatchcountPatchindexbufferPatchindexbufferoffsetInstancecountBaseinstance,  void,   NSUInteger numberOfPatchControlPoints,NSUInteger patchStart,NSUInteger patchCount, id <MTLBuffer> patchIndexBuffer,NSUInteger patchIndexBufferOffset,NSUInteger instanceCount,NSUInteger baseInstance)
{
	GMTITraceRenderEncoderDrawpatchesHandler.Trace(Obj, numberOfPatchControlPoints, patchStart, patchCount, patchIndexBuffer, patchIndexBufferOffset, instanceCount, baseInstance);
	Original(Obj, Cmd, numberOfPatchControlPoints,patchStart,patchCount,patchIndexBuffer,patchIndexBufferOffset,instanceCount,baseInstance);
}

struct MTITraceRenderEncoderDrawpatches2Handler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawpatches2Handler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawpatchesPatchindexbufferPatchindexbufferoffsetIndirectbufferIndirectbufferoffset")
	{
		
	}
	
	void Trace(id Object, NSUInteger numberOfPatchControlPoints, id <MTLBuffer> patchIndexBuffer,NSUInteger patchIndexBufferOffset,id <MTLBuffer> indirectBuffer,NSUInteger indirectBufferOffset)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << numberOfPatchControlPoints;
		fs << (uintptr_t)patchIndexBuffer;
		fs << patchIndexBufferOffset;
		fs << (uintptr_t)indirectBuffer;
		fs << indirectBufferOffset;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger numberOfPatchControlPoints;
		uintptr_t patchIndexBuffer;
		NSUInteger patchIndexBufferOffset;
		uintptr_t indirectBuffer;
		NSUInteger indirectBufferOffset;
		fs >> numberOfPatchControlPoints;
		fs >> patchIndexBuffer;
		fs >> patchIndexBufferOffset;
		fs >> indirectBuffer;
		fs >> indirectBufferOffset;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawPatches:numberOfPatchControlPoints patchIndexBuffer:MTITrace::Get().FetchObject(patchIndexBuffer) patchIndexBufferOffset:patchIndexBufferOffset indirectBuffer:MTITrace::Get().FetchObject(indirectBuffer) indirectBufferOffset:indirectBufferOffset];
	}
};
static MTITraceRenderEncoderDrawpatches2Handler GMTITraceRenderEncoderDrawpatches2Handler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawpatchesPatchindexbufferPatchindexbufferoffsetIndirectbufferIndirectbufferoffset,  void,   NSUInteger numberOfPatchControlPoints, id <MTLBuffer> patchIndexBuffer,NSUInteger patchIndexBufferOffset,id <MTLBuffer> indirectBuffer,NSUInteger indirectBufferOffset)
{
	GMTITraceRenderEncoderDrawpatches2Handler.Trace(Obj, numberOfPatchControlPoints, patchIndexBuffer, patchIndexBufferOffset, indirectBuffer, indirectBufferOffset);
	Original(Obj, Cmd, numberOfPatchControlPoints,patchIndexBuffer,patchIndexBufferOffset,indirectBuffer,indirectBufferOffset);
}

struct MTITraceRenderEncoderDrawindexedpatches3Handler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawindexedpatches3Handler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawindexedpatchesPatchstartPatchcountPatchindexbufferPatchindexbufferoffsetControlpointindexbufferControlpointindexbufferoffsetInstancecountBaseinstance")
	{
		
	}
	
	void Trace(id Object, NSUInteger numberOfPatchControlPoints,NSUInteger patchStart,NSUInteger patchCount, id <MTLBuffer> patchIndexBuffer,NSUInteger patchIndexBufferOffset,id <MTLBuffer> controlPointIndexBuffer,NSUInteger controlPointIndexBufferOffset,NSUInteger instanceCount,NSUInteger baseInstance)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << numberOfPatchControlPoints;
		fs << patchStart;
		fs << patchCount;
		fs << (uintptr_t)patchIndexBuffer;
		fs << patchIndexBufferOffset;
		fs << (uintptr_t)controlPointIndexBuffer;
		fs << controlPointIndexBufferOffset;
		fs << instanceCount;
		fs << baseInstance;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger numberOfPatchControlPoints;
		NSUInteger patchStart;
		NSUInteger patchCount;
		uintptr_t patchIndexBuffer;
		NSUInteger patchIndexBufferOffset;
		uintptr_t controlPointIndexBuffer;
		NSUInteger controlPointIndexBufferOffset;
		NSUInteger instanceCount;
		NSUInteger baseInstance;
		fs >> numberOfPatchControlPoints;
		fs >> patchStart;
		fs >> patchCount;
		fs >> patchIndexBuffer;
		fs >> patchIndexBufferOffset;
		fs >> controlPointIndexBuffer;
		fs >> controlPointIndexBufferOffset;
		fs >> instanceCount;
		fs >> baseInstance;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawIndexedPatches:numberOfPatchControlPoints patchStart:patchStart patchCount:patchCount patchIndexBuffer:MTITrace::Get().FetchObject(patchIndexBuffer) patchIndexBufferOffset:patchIndexBufferOffset controlPointIndexBuffer:MTITrace::Get().FetchObject(controlPointIndexBuffer) controlPointIndexBufferOffset:controlPointIndexBufferOffset instanceCount:instanceCount baseInstance:baseInstance];
	}
};
static MTITraceRenderEncoderDrawindexedpatches3Handler GMTITraceRenderEncoderDrawindexedpatches3Handler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawindexedpatchesPatchstartPatchcountPatchindexbufferPatchindexbufferoffsetControlpointindexbufferControlpointindexbufferoffsetInstancecountBaseinstance,  void,   NSUInteger numberOfPatchControlPoints,NSUInteger patchStart,NSUInteger patchCount, id <MTLBuffer> patchIndexBuffer,NSUInteger patchIndexBufferOffset,id <MTLBuffer> controlPointIndexBuffer,NSUInteger controlPointIndexBufferOffset,NSUInteger instanceCount,NSUInteger baseInstance)
{
	GMTITraceRenderEncoderDrawindexedpatches3Handler.Trace(Obj, numberOfPatchControlPoints, patchStart, patchCount, patchIndexBuffer, patchIndexBufferOffset, controlPointIndexBuffer, controlPointIndexBufferOffset, instanceCount, baseInstance);
	Original(Obj, Cmd, numberOfPatchControlPoints,patchStart,patchCount,patchIndexBuffer,patchIndexBufferOffset,controlPointIndexBuffer,controlPointIndexBufferOffset,instanceCount,baseInstance);
}

struct MTITraceRenderEncoderDrawindexedpatches4Handler : public MTITraceCommandHandler
{
	MTITraceRenderEncoderDrawindexedpatches4Handler()
	: MTITraceCommandHandler("MTLRenderCommandEncoder", "DrawindexedpatchesPatchindexbufferPatchindexbufferoffsetControlpointindexbufferControlpointindexbufferoffsetIndirectbufferIndirectbufferoffset")
	{
		
	}
	
	void Trace(id Object, NSUInteger numberOfPatchControlPoints, id <MTLBuffer> patchIndexBuffer,NSUInteger patchIndexBufferOffset,id <MTLBuffer> controlPointIndexBuffer,NSUInteger controlPointIndexBufferOffset,id <MTLBuffer> indirectBuffer,NSUInteger indirectBufferOffset)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << numberOfPatchControlPoints;
		fs << (uintptr_t)patchIndexBuffer;
		fs << patchIndexBufferOffset;
		fs << (uintptr_t)controlPointIndexBuffer;
		fs << controlPointIndexBufferOffset;
		fs << (uintptr_t)indirectBuffer;
		fs << indirectBufferOffset;

		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger numberOfPatchControlPoints;
		uintptr_t patchIndexBuffer;
		NSUInteger patchIndexBufferOffset;
		uintptr_t controlPointIndexBuffer;
		NSUInteger controlPointIndexBufferOffset;
		uintptr_t indirectBuffer;
		NSUInteger indirectBufferOffset;
		fs >> numberOfPatchControlPoints;
		fs >> patchIndexBuffer;
		fs >> patchIndexBufferOffset;
		fs >> controlPointIndexBuffer;
		fs >> controlPointIndexBufferOffset;
		fs >> indirectBuffer;
		fs >> indirectBufferOffset;
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) drawIndexedPatches:numberOfPatchControlPoints patchIndexBuffer:MTITrace::Get().FetchObject(patchIndexBuffer) patchIndexBufferOffset:patchIndexBufferOffset controlPointIndexBuffer:MTITrace::Get().FetchObject(controlPointIndexBuffer) controlPointIndexBufferOffset:controlPointIndexBufferOffset indirectBuffer:MTITrace::Get().FetchObject(indirectBuffer) indirectBufferOffset:indirectBufferOffset];
	}
};
static MTITraceRenderEncoderDrawindexedpatches4Handler GMTITraceRenderEncoderDrawindexedpatches4Handler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, DrawindexedpatchesPatchindexbufferPatchindexbufferoffsetControlpointindexbufferControlpointindexbufferoffsetIndirectbufferIndirectbufferoffset,  void,   NSUInteger numberOfPatchControlPoints, id <MTLBuffer> patchIndexBuffer,NSUInteger patchIndexBufferOffset,id <MTLBuffer> controlPointIndexBuffer,NSUInteger controlPointIndexBufferOffset,id <MTLBuffer> indirectBuffer,NSUInteger indirectBufferOffset)
{
	GMTITraceRenderEncoderDrawindexedpatches4Handler.Trace(Obj, numberOfPatchControlPoints, patchIndexBuffer, patchIndexBufferOffset, controlPointIndexBuffer, controlPointIndexBufferOffset, indirectBuffer, indirectBufferOffset);
	Original(Obj, Cmd, numberOfPatchControlPoints,patchIndexBuffer,patchIndexBufferOffset,controlPointIndexBuffer,controlPointIndexBufferOffset,indirectBuffer,indirectBufferOffset);
}

struct MTITraceRendererEncoderUseresourceusageHandler : public MTITraceCommandHandler
{
	MTITraceRendererEncoderUseresourceusageHandler()
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
		
		[(id<MTLRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) useResource:MTITrace::Get().FetchObject(Res) usage:(MTLResourceUsage)usage];
	}
};
static MTITraceRendererEncoderUseresourceusageHandler GMTITraceRendererEncoderUseresourceusageHandler;
INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, UseresourceUsage,  void,   id <MTLResource> resource,MTLResourceUsage usage)
{
	GMTITraceRendererEncoderUseresourceusageHandler.Trace(Obj, resource, usage);
	Original(Obj, Cmd, resource, usage);
}

INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, UseresourcesCountUsage,  void,   const id <MTLResource> * resources,NSUInteger count,MTLResourceUsage usage)
{
	Original(Obj, Cmd, resources, count, usage);
}

INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, Useheap,  void,    id <MTLHeap> heap)
{
	Original(Obj, Cmd, heap);
}

INTERPOSE_DEFINITION( MTIRenderCommandEncoderTrace, UseheapsCount,  void,   const id <MTLHeap> * heaps, NSUInteger count)
{
	Original(Obj, Cmd, heaps, count);
}

INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, SetTilebytesLengthAtindex,	void, const void * b,NSUInteger l,NSUInteger i)
{
	Original(Obj, Cmd, b,l,i);
}
INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, SetTilebufferOffsetAtindex,	void,id <MTLBuffer> b,NSUInteger o,NSUInteger i)
{
	Original(Obj, Cmd, b,o,i);
}
INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, SetTilebufferoffsetAtindex,	void, NSUInteger o,NSUInteger i)
{
	Original(Obj, Cmd, o,i);
}
INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, SetTilebuffersOffsetsWithrange,	void, const id <MTLBuffer> * b,const NSUInteger * o, NSRange r)
{
	Original(Obj, Cmd, b,o,r);
}
INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, SetTiletextureAtindex,	void,id <MTLTexture> t,NSUInteger i)
{
	Original(Obj, Cmd, t,i);
}
INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, SetTiletexturesWithrange,	void, const id <MTLTexture> * t,NSRange r)
{
	Original(Obj, Cmd, t,r);
}
INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, SetTilesamplerstateAtindex,	void,id <MTLSamplerState> s,NSUInteger i)
{
	Original(Obj, Cmd, s,i);
}
INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, SetTilesamplerstatesWithrange,	void, const id <MTLSamplerState> * s,NSRange r)
{
	Original(Obj, Cmd, s,r);
}
INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, SetTilesamplerstateLodminclampLodmaxclampAtindex,	void,id <MTLSamplerState> s,float l,float x,NSUInteger i)
{
	Original(Obj, Cmd, s,l,x,i);
}
INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, SetTilesamplerstatesLodminclampsLodmaxclampsWithrange,	void, const id <MTLSamplerState> * s,const float * l,const float * x,NSRange r)
{
	Original(Obj, Cmd, s,l,x,r);
}
INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, dispatchThreadsPerTile,	void, MTLPPSize s)
{
	Original(Obj, Cmd, s);
}
INTERPOSE_DEFINITION(MTIRenderCommandEncoderTrace, setThreadgroupMemoryLength,	void, NSUInteger t, NSUInteger l, NSUInteger i)
{
	Original(Obj, Cmd, t,l,i);
}

MTLPP_END
