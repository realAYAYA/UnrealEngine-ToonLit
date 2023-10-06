// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTIArgumentEncoder.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

INTERPOSE_PROTOCOL_REGISTER(MTIArgumentEncoderTrace, id<MTLArgumentEncoder>);

INTERPOSE_DEFINITION_VOID(MTIArgumentEncoderTrace, Device, id<MTLDevice>)
{
	return Original(Obj, Cmd);
}
INTERPOSE_DEFINITION_VOID(MTIArgumentEncoderTrace, Label, NSString*)
{
	return Original(Obj, Cmd);
}

struct MTITraceArgumentEncoderSetLabelHandler : public MTITraceCommandHandler
{
	MTITraceArgumentEncoderSetLabelHandler()
	: MTITraceCommandHandler("MTLArgumentEncoder", "setLabel")
	{
		
	}
	
	void Trace(id Object, NSString* Label)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << MTIString(Label ? [Label UTF8String] : "");
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTIString label;
		fs >> label;
		
		[(id<MTLArgumentEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setLabel:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceArgumentEncoderSetLabelHandler GMTITraceArgumentEncoderSetLabelHandler;
INTERPOSE_DEFINITION(MTIArgumentEncoderTrace, SetLabel, void, NSString* S)
{
	GMTITraceArgumentEncoderSetLabelHandler.Trace(Obj, S);
	Original(Obj, Cmd, S);
}
INTERPOSE_DEFINITION_VOID(MTIArgumentEncoderTrace, EncodedLength, NSUInteger)
{
	return Original(Obj, Cmd );
}
INTERPOSE_DEFINITION_VOID(MTIArgumentEncoderTrace, Alignment, NSUInteger)
{
	return Original(Obj, Cmd );
}

struct MTITraceArgumentEncoderSetBufferOffsetHandler : public MTITraceCommandHandler
{
	MTITraceArgumentEncoderSetBufferOffsetHandler()
	: MTITraceCommandHandler("MTLArgumentEncoder", "Setargumentbufferoffset")
	{
		
	}
	
	void Trace(id Object, id <MTLBuffer> b, NSUInteger o)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)b;
		fs << o;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t b;
		fs >> b;
		
		NSUInteger o;
		fs >> o;
		
		[(id<MTLArgumentEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setArgumentBuffer:MTITrace::Get().FetchObject(b) offset:o];
	}
};
static MTITraceArgumentEncoderSetBufferOffsetHandler GMTITraceArgumentEncoderSetBufferOffsetHandler;
INTERPOSE_DEFINITION(MTIArgumentEncoderTrace, Setargumentbufferoffset, void, id <MTLBuffer> b, NSUInteger o)
{
	GMTITraceArgumentEncoderSetBufferOffsetHandler.Trace(Obj, b, o);
	Original(Obj, Cmd, b, o);
}

struct MTITraceArgumentEncoderSetBufferOffsetElementHandler : public MTITraceCommandHandler
{
	MTITraceArgumentEncoderSetBufferOffsetElementHandler()
	: MTITraceCommandHandler("MTLArgumentEncoder", "Setargumentbufferstartoffsetarrayelement")
	{
		
	}
	
	void Trace(id Object, id <MTLBuffer> b, NSUInteger o, NSUInteger e)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)b;
		fs << o;
		fs << e;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t b;
		fs >> b;
		
		NSUInteger o;
		fs >> o;
		
		NSUInteger e;
		fs >> e;
		
		[(id<MTLArgumentEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setArgumentBuffer:MTITrace::Get().FetchObject(b) startOffset:o arrayElement:e];
	}
};
static MTITraceArgumentEncoderSetBufferOffsetElementHandler GMTITraceArgumentEncoderSetBufferOffsetElementHandler;
INTERPOSE_DEFINITION(MTIArgumentEncoderTrace, Setargumentbufferstartoffsetarrayelement, void, id<MTLBuffer> b, NSUInteger o, NSUInteger e)
{
	GMTITraceArgumentEncoderSetBufferOffsetElementHandler.Trace(Obj, b, o, e);
	Original(Obj, Cmd, b, o, e);
}

struct MTITraceArgumentEncoderSetBufferOffsetIndexHandler : public MTITraceCommandHandler
{
	MTITraceArgumentEncoderSetBufferOffsetIndexHandler()
	: MTITraceCommandHandler("MTLArgumentEncoder", "Setbufferoffsetatindex")
	{
		
	}
	
	void Trace(id Object, id <MTLBuffer> b, NSUInteger o, NSUInteger i)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)b;
		fs << o;
		fs << i;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t b;
		fs >> b;
		
		NSUInteger o;
		fs >> o;
		
		NSUInteger i;
		fs >> i;
		
		[(id<MTLArgumentEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setBuffer:MTITrace::Get().FetchObject(b) offset:o atIndex:i];
	}
};
static MTITraceArgumentEncoderSetBufferOffsetIndexHandler GMTITraceArgumentEncoderSetBufferOffsetIndexHandler;
INTERPOSE_DEFINITION(MTIArgumentEncoderTrace, Setbufferoffsetatindex, void,  id <MTLBuffer> b, NSUInteger o, NSUInteger i)
{
	GMTITraceArgumentEncoderSetBufferOffsetIndexHandler.Trace(Obj, b, o, i);
	Original(Obj, Cmd, b, o, i);
}

struct MTITraceArgumentEncoderSetBufferOffsetRangeHandler : public MTITraceCommandHandler
{
	MTITraceArgumentEncoderSetBufferOffsetRangeHandler()
	: MTITraceCommandHandler("MTLArgumentEncoder", "Setbuffersoffsetswithrange")
	{
		
	}
	
	void Trace(id Object, const id <MTLBuffer>  * b , const NSUInteger * o, NSRange r)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << r.location;
		fs << r.length;
		
		for (unsigned i = 0; i < r.length; i++)
		{
			fs << (uintptr_t)b[i];
			fs << o[i];
		}
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSRange range;
		fs >> range.location;
		fs >> range.length;
		
		std::vector<id<MTLBuffer>> Buffers;
		std::vector<NSUInteger> Offsets;
		uintptr_t b;
		NSUInteger o;
		for (unsigned i = 0; i < range.length; i++)
		{
			fs >> b;
			fs >> o;
			Buffers.push_back(MTITrace::Get().FetchObject(b));
			Offsets.push_back(o);
		}
		
		[(id<MTLArgumentEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setBuffers:Buffers.data() offsets:Offsets.data() withRange:range];
	}
};
static MTITraceArgumentEncoderSetBufferOffsetRangeHandler GMTITraceArgumentEncoderSetBufferOffsetRangeHandler;
INTERPOSE_DEFINITION(MTIArgumentEncoderTrace, Setbuffersoffsetswithrange, void, const id <MTLBuffer>  * b , const NSUInteger * o, NSRange r)
{
	GMTITraceArgumentEncoderSetBufferOffsetRangeHandler.Trace(Obj, b, o, r);
	Original(Obj, Cmd, b, o, r);
}

struct MTITraceArgumentEncoderSetTextureHandler : public MTITraceCommandHandler
{
	MTITraceArgumentEncoderSetTextureHandler()
	: MTITraceCommandHandler("MTLArgumentEncoder", "Settextureatindex")
	{
		
	}
	
	void Trace(id Object, id <MTLTexture> t, NSUInteger i)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)t;
		fs << i;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t t;
		fs >> t;
		
		NSUInteger i;
		fs >> i;
		
		[(id<MTLArgumentEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setTexture:MTITrace::Get().FetchObject(t) atIndex:i];
	}
};
static MTITraceArgumentEncoderSetTextureHandler GMTITraceArgumentEncoderSetTextureHandler;
INTERPOSE_DEFINITION(MTIArgumentEncoderTrace, Settextureatindex, void,  id <MTLTexture> t , NSUInteger i)
{
	GMTITraceArgumentEncoderSetTextureHandler.Trace(Obj, t, i);
	Original(Obj, Cmd, t, i);
}

struct MTITraceArgumentEncoderSetTextureRangeHandler : public MTITraceCommandHandler
{
	MTITraceArgumentEncoderSetTextureRangeHandler()
	: MTITraceCommandHandler("MTLArgumentEncoder", "Settextureswithrange")
	{
		
	}
	
	void Trace(id Object, const id <MTLTexture>  * b , NSRange r)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << r.location;
		fs << r.length;
		
		for (unsigned i = 0; i < r.length; i++)
		{
			fs << (uintptr_t)b[i];
		}
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSRange range;
		fs >> range.location;
		fs >> range.length;
		
		std::vector<id<MTLTexture>> Buffers;
		uintptr_t b;
		for (unsigned i = 0; i < range.length; i++)
		{
			fs >> b;
			Buffers.push_back(MTITrace::Get().FetchObject(b));
		}
		
		[(id<MTLArgumentEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setTextures:Buffers.data() withRange:range];
	}
};
static MTITraceArgumentEncoderSetTextureRangeHandler GMTITraceArgumentEncoderSetTextureRangeHandler;
INTERPOSE_DEFINITION(MTIArgumentEncoderTrace, Settextureswithrange, void, const id <MTLTexture>  * t , NSRange r)
{
	GMTITraceArgumentEncoderSetTextureRangeHandler.Trace(Obj, t, r);
	Original(Obj, Cmd, t, r);
}

struct MTITraceArgumentEncoderSetSamplerHandler : public MTITraceCommandHandler
{
	MTITraceArgumentEncoderSetSamplerHandler()
	: MTITraceCommandHandler("MTLArgumentEncoder", "Setsamplerstateatindex")
	{
		
	}
	
	void Trace(id Object, id <MTLSamplerState> t, NSUInteger i)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)t;
		fs << i;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t t;
		fs >> t;
		
		NSUInteger i;
		fs >> i;
		
		[(id<MTLArgumentEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setSamplerState:MTITrace::Get().FetchObject(t) atIndex:i];
	}
};
static MTITraceArgumentEncoderSetSamplerHandler GMTITraceArgumentEncoderSetSamplerHandler;
INTERPOSE_DEFINITION(MTIArgumentEncoderTrace, Setsamplerstateatindex, void,  id <MTLSamplerState> s , NSUInteger i)
{
	GMTITraceArgumentEncoderSetSamplerHandler.Trace(Obj, s, i);
	Original(Obj, Cmd, s, i);
}

struct MTITraceArgumentEncoderSetSamplerStateRangeHandler : public MTITraceCommandHandler
{
	MTITraceArgumentEncoderSetSamplerStateRangeHandler()
	: MTITraceCommandHandler("MTLArgumentEncoder", "Setsamplerstateswithrange")
	{
		
	}
	
	void Trace(id Object, const id <MTLSamplerState>  * b , NSRange r)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << r.location;
		fs << r.length;
		
		for (unsigned i = 0; i < r.length; i++)
		{
			fs << (uintptr_t)b[i];
		}
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSRange range;
		fs >> range.location;
		fs >> range.length;
		
		std::vector<id<MTLSamplerState>> Buffers;
		uintptr_t b;
		for (unsigned i = 0; i < range.length; i++)
		{
			fs >> b;
			Buffers.push_back(MTITrace::Get().FetchObject(b));
		}
		
		[(id<MTLArgumentEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setSamplerStates:Buffers.data() withRange:range];
	}
};
static MTITraceArgumentEncoderSetSamplerStateRangeHandler GMTITraceArgumentEncoderSetSamplerStateRangeHandler;
INTERPOSE_DEFINITION(MTIArgumentEncoderTrace, Setsamplerstateswithrange, void, const id <MTLSamplerState>  * s, NSRange r)
{
	Original(Obj, Cmd, s, r);
}
INTERPOSE_DEFINITION(MTIArgumentEncoderTrace, ConstantDataAtIndex, void*, NSUInteger i)
{
	return Original(Obj, Cmd, i);
}

struct MTITraceArgumentEncoderNewArgumentEncoderForBufferAtIndexHandler : public MTITraceCommandHandler
{
	MTITraceArgumentEncoderNewArgumentEncoderForBufferAtIndexHandler()
	: MTITraceCommandHandler("MTLArgumentEncoder", "NewArgumentEncoderForBufferAtIndex")
	{
		
	}
	
	id<MTLArgumentEncoder> Trace(id Object, NSUInteger i, id<MTLArgumentEncoder> Encoder)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << i;
		fs << (uintptr_t)Encoder;
		
		MTITrace::Get().EndWrite();
		return Encoder;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger i;
		fs >> i;
		uintptr_t t;
		fs >> t;
		
		id<MTLArgumentEncoder> Encoder = [(id<MTLArgumentEncoder>)MTITrace::Get().FetchObject(Header.Receiver) newArgumentEncoderForBufferAtIndex:i];
		MTITrace::Get().RegisterObject(t, Encoder);
	}
};
static MTITraceArgumentEncoderNewArgumentEncoderForBufferAtIndexHandler GMTITraceArgumentEncoderNewArgumentEncoderForBufferAtIndexHandler;
INTERPOSE_DEFINITION(MTIArgumentEncoderTrace, NewArgumentEncoderForBufferAtIndex, id<MTLArgumentEncoder>, NSUInteger i)
{
	return GMTITraceArgumentEncoderNewArgumentEncoderForBufferAtIndexHandler.Trace(Obj, i, Original(Obj, Cmd, i));
}

MTLPP_END
