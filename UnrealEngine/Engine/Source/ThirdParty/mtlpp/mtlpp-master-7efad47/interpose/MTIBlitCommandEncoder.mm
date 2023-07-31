// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTIBlitCommandEncoder.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

INTERPOSE_PROTOCOL_REGISTER(MTIBlitCommandEncoderTrace, id<MTLBlitCommandEncoder>);

struct MTITraceBlitEncoderSynchronizeResourceHandler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderSynchronizeResourceHandler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "SynchronizeResource")
	{
		
	}
	
	void Trace(id Object, id<MTLResource> Res)
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
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) synchronizeResource:MTITrace::Get().FetchObject(Res)];
	}
};
static MTITraceBlitEncoderSynchronizeResourceHandler GMTITraceBlitEncoderSynchronizeResourceHandler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, SynchronizeResource, void, id<MTLResource> Res)
{
	GMTITraceBlitEncoderSynchronizeResourceHandler.Trace(Obj, Res);
	Original(Obj, Cmd, Res);
}

struct MTITraceBlitEncoderSynchronizeTextureSliceLevelHandler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderSynchronizeTextureSliceLevelHandler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "SynchronizeTextureSliceLevel")
	{
		
	}
	
	void Trace(id Object, id<MTLTexture> Res, NSUInteger Slice, NSUInteger Level)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Res;
		fs << Slice;
		fs << Level;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Res;
		fs >> Res;
		
		NSUInteger Slice, Level;
		fs >> Slice;
		fs >> Level;
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) synchronizeTexture:MTITrace::Get().FetchObject(Res) slice:Slice level:Level];
	}
};
static MTITraceBlitEncoderSynchronizeTextureSliceLevelHandler GMTITraceBlitEncoderSynchronizeTextureSliceLevelHandler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, SynchronizeTextureSliceLevel, void, id<MTLTexture> Res, NSUInteger Slice, NSUInteger Level)
{
	GMTITraceBlitEncoderSynchronizeTextureSliceLevelHandler.Trace(Obj, Res, Slice, Level);
	Original(Obj, Cmd, Res, Slice, Level);
}

struct MTITraceBlitEncoderCopyFromTextureToTextureHandler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderCopyFromTextureToTextureHandler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "CopyFromTextureToTexture")
	{
		
	}
	
	void Trace(id Object, id<MTLTexture> tex, NSUInteger slice, NSUInteger level, MTLPPOrigin origin, MTLPPSize size, id<MTLTexture> dst, NSUInteger dstslice, NSUInteger dstlevel, MTLPPOrigin dstorigin)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)tex;
		fs << slice;
		fs << level;
		fs << origin.x;
		fs << origin.y;
		fs << origin.z;
		fs << size.width;
		fs << size.height;
		fs << size.depth;
		fs << (uintptr_t)dst;
		fs << dstslice;
		fs << dstlevel;
		fs << dstorigin.x;
		fs << dstorigin.y;
		fs << dstorigin.z;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t tex;
		NSUInteger slice;
		NSUInteger level;
		MTLOrigin origin;
		MTLSize size;
		uintptr_t dst;
		NSUInteger dstslice;
		NSUInteger dstlevel;
		MTLOrigin dstorigin;
		
		fs >> tex;
		fs >> slice;
		fs >> level;
		fs >> origin.x;
		fs >> origin.y;
		fs >> origin.z;
		fs >> size.width;
		fs >> size.height;
		fs >> size.depth;
		fs >> dst;
		fs >> dstslice;
		fs >> dstlevel;
		fs >> dstorigin.x;
		fs >> dstorigin.y;
		fs >> dstorigin.z;
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) copyFromTexture:MTITrace::Get().FetchObject(tex) sourceSlice:slice sourceLevel:level sourceOrigin:origin sourceSize:size toTexture:MTITrace::Get().FetchObject(dst) destinationSlice:dstslice destinationLevel:dstlevel destinationOrigin:dstorigin];
	}
};
static MTITraceBlitEncoderCopyFromTextureToTextureHandler GMTITraceBlitEncoderCopyFromTextureToTextureHandler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, CopyFromTexturesourceSlicesourceLevelsourceOriginsourceSizetoTexturedestinationSlicedestinationLeveldestinationOrigin, void, id<MTLTexture> tex, NSUInteger slice, NSUInteger level, MTLPPOrigin origin, MTLPPSize size, id<MTLTexture> dst, NSUInteger dstslice, NSUInteger dstlevel, MTLPPOrigin dstorigin)
{
	GMTITraceBlitEncoderCopyFromTextureToTextureHandler.Trace(Obj, tex, slice, level, origin, size, dst, dstslice, dstlevel, dstorigin);
	Original(Obj, Cmd, tex, slice, level, origin, size, dst, dstslice, dstlevel, dstorigin);
}

struct MTITraceBlitEncoderCopyFromBufferToTexture2Handler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderCopyFromBufferToTexture2Handler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "CopyFromBufferToTexture2")
	{
		
	}
	
	void Trace(id Object, id<MTLBuffer> buffer, NSUInteger offset, NSUInteger bpr, NSUInteger bpi, MTLPPSize size, id<MTLTexture> dst, NSUInteger slice, NSUInteger level, MTLPPOrigin origin)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)buffer;
		fs << offset;
		fs << bpr;
		fs << bpi;
		fs << size.width;
		fs << size.height;
		fs << size.depth;
		fs << (uintptr_t)dst;
		fs << slice;
		fs << level;
		fs << origin.x;
		fs << origin.y;
		fs << origin.z;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Buffer;
		NSUInteger offset;
		NSUInteger bpr;
		NSUInteger bpi;
		MTLSize size;
		uintptr_t dest;
		NSUInteger slice;
		NSUInteger level;
		MTLOrigin origin;
		
		fs >> Buffer;
		fs >> offset;
		fs >> bpr;
		fs >> bpi;
		fs >> size.width;
		fs >> size.height;
		fs >> size.depth;
		fs >> dest;
		fs >> slice;
		fs >> level;
		fs >> origin.x;
		fs >> origin.y;
		fs >> origin.z;
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) copyFromBuffer:MTITrace::Get().FetchObject(Buffer) sourceOffset:offset sourceBytesPerRow:bpr sourceBytesPerImage:bpi sourceSize:size toTexture:MTITrace::Get().FetchObject(dest) destinationSlice:slice destinationLevel:level destinationOrigin:origin];
	}
};
static MTITraceBlitEncoderCopyFromBufferToTexture2Handler GMTITraceBlitEncoderCopyFromBufferToTexture2Handler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, CopyFromBuffersourceOffsetsourceBytesPerRowsourceBytesPerImagesourceSizetoTexturedestinationSlicedestinationLeveldestinationOrigin, void, id<MTLBuffer> buffer, NSUInteger offset, NSUInteger bpr, NSUInteger bpi, MTLPPSize size, id<MTLTexture> dst, NSUInteger slice, NSUInteger level, MTLPPOrigin origin)
{
	GMTITraceBlitEncoderCopyFromBufferToTexture2Handler.Trace(Obj, buffer, offset, bpr, bpi, size, dst, slice, level, origin);
	Original(Obj, Cmd, buffer, offset, bpr, bpi, size, dst, slice, level, origin);
}



struct MTITraceBlitEncoderCopyFromBufferToTexture3Handler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderCopyFromBufferToTexture3Handler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "CopyFromBufferToTexture3")
	{
		
	}
	
	void Trace(id Object, id<MTLBuffer> buffer, NSUInteger offset, NSUInteger bpr, NSUInteger bpi, MTLPPSize size, id<MTLTexture> dst, NSUInteger slice, NSUInteger level, MTLPPOrigin origin, MTLBlitOption Opts)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)buffer;
		fs << offset;
		fs << bpr;
		fs << bpi;
		fs << size.width;
		fs << size.height;
		fs << size.depth;
		fs << (uintptr_t)dst;
		fs << slice;
		fs << level;
		fs << origin.x;
		fs << origin.y;
		fs << origin.z;
		fs << Opts;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Buffer;
		NSUInteger offset;
		NSUInteger bpr;
		NSUInteger bpi;
		MTLSize size;
		uintptr_t dest;
		NSUInteger slice;
		NSUInteger level;
		MTLOrigin origin;
		NSUInteger Opts;
		
		fs >> Buffer;
		fs >> offset;
		fs >> bpr;
		fs >> bpi;
		fs >> size.width;
		fs >> size.height;
		fs >> size.depth;
		fs >> dest;
		fs >> slice;
		fs >> level;
		fs >> origin.x;
		fs >> origin.y;
		fs >> origin.z;
		fs >> Opts;
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) copyFromBuffer:MTITrace::Get().FetchObject(Buffer) sourceOffset:offset sourceBytesPerRow:bpr sourceBytesPerImage:bpi sourceSize:size toTexture:MTITrace::Get().FetchObject(dest) destinationSlice:slice destinationLevel:level destinationOrigin:origin options:Opts];
	}
};
static MTITraceBlitEncoderCopyFromBufferToTexture3Handler GMTITraceBlitEncoderCopyFromBufferToTexture3Handler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, CopyFromBuffersourceOffsetsourceBytesPerRowsourceBytesPerImagesourceSizetoTexturedestinationSlicedestinationLeveldestinationOriginoptions, void, id<MTLBuffer> buffer, NSUInteger offset, NSUInteger bpr, NSUInteger bpi, MTLPPSize size, id<MTLTexture> dst, NSUInteger slice, NSUInteger level, MTLPPOrigin origin, MTLBlitOption Opts)
{
	GMTITraceBlitEncoderCopyFromBufferToTexture3Handler.Trace(Obj, buffer, offset, bpr, bpi, size, dst, slice, level, origin, Opts);
	Original(Obj, Cmd, buffer, offset, bpr, bpi, size, dst, slice, level, origin, Opts);
}

struct MTITraceBlitEncoderCopyFromTextureToBufferHandler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderCopyFromTextureToBufferHandler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "CopyFromTextureToBuffer")
	{
		
	}
	
	void Trace(id Object, id<MTLTexture> Tex, NSUInteger slice, NSUInteger level, MTLPPOrigin origin, MTLPPSize size, id<MTLBuffer> dest, NSUInteger offset, NSUInteger bpr, NSUInteger bpi)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Tex;
		fs << slice;
		fs << level;
		fs << origin.x;
		fs << origin.y;
		fs << origin.z;
		fs << size.width;
		fs << size.height;
		fs << size.depth;
		fs << (uintptr_t)dest;
		fs << offset;
		fs << bpr;
		fs << bpi;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Tex;
		NSUInteger slice;
		NSUInteger level;
		MTLOrigin origin;
		MTLSize size;
		uintptr_t dest;
		NSUInteger offset;
		NSUInteger bpr;
		NSUInteger bpi;
		
		fs >> Tex;
		fs >> slice;
		fs >> level;
		fs >> origin.x;
		fs >> origin.y;
		fs >> origin.z;
		fs >> size.width;
		fs >> size.height;
		fs >> size.depth;
		fs >> dest;
		fs >> offset;
		fs >> bpr;
		fs >> bpi;
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) copyFromTexture:MTITrace::Get().FetchObject(Tex) sourceSlice:slice sourceLevel:level sourceOrigin:origin sourceSize:size toBuffer:MTITrace::Get().FetchObject(dest) destinationOffset:offset destinationBytesPerRow:bpr destinationBytesPerImage:bpi];
	}
};
static MTITraceBlitEncoderCopyFromTextureToBufferHandler GMTITraceBlitEncoderCopyFromTextureToBufferHandler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, CopyFromTexturesourceSlicesourceLevelsourceOriginsourceSizetoBufferdestinationOffsetdestinationBytesPerRowdestinationBytesPerImage, void, id<MTLTexture> tex, NSUInteger slice, NSUInteger level, MTLPPOrigin origin, MTLPPSize size, id<MTLBuffer> dst, NSUInteger offset, NSUInteger bpr, NSUInteger bpi)
{
	GMTITraceBlitEncoderCopyFromTextureToBufferHandler.Trace(Obj, tex, slice, level, origin, size, dst, offset, bpr, bpi);
	Original(Obj, Cmd, tex, slice, level, origin, size, dst, offset, bpr, bpi);
}

struct MTITraceBlitEncoderCopyFromTextureToBuffer2Handler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderCopyFromTextureToBuffer2Handler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "CopyFromTextureToBuffer2")
	{
		
	}
	
	void Trace(id Object, id<MTLTexture> Tex, NSUInteger slice, NSUInteger level, MTLPPOrigin origin, MTLPPSize size, id<MTLBuffer> dest, NSUInteger offset, NSUInteger bpr, NSUInteger bpi, MTLBlitOption Opts)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Tex;
		fs << slice;
		fs << level;
		fs << origin.x;
		fs << origin.y;
		fs << origin.z;
		fs << size.width;
		fs << size.height;
		fs << size.depth;
		fs << (uintptr_t)dest;
		fs << offset;
		fs << bpr;
		fs << bpi;
		fs << Opts;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Tex;
		NSUInteger slice;
		NSUInteger level;
		MTLOrigin origin;
		MTLSize size;
		uintptr_t dest;
		NSUInteger offset;
		NSUInteger bpr;
		NSUInteger bpi;
		MTLBlitOption Opts;
		
		fs >> Tex;
		fs >> slice;
		fs >> level;
		fs >> origin.x;
		fs >> origin.y;
		fs >> origin.z;
		fs >> size.width;
		fs >> size.height;
		fs >> size.depth;
		fs >> dest;
		fs >> offset;
		fs >> bpr;
		fs >> bpi;
		fs >> Opts;
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) copyFromTexture:MTITrace::Get().FetchObject(Tex) sourceSlice:slice sourceLevel:level sourceOrigin:origin sourceSize:size toBuffer:MTITrace::Get().FetchObject(dest) destinationOffset:offset destinationBytesPerRow:bpr destinationBytesPerImage:bpi options:Opts];
	}
};
static MTITraceBlitEncoderCopyFromTextureToBuffer2Handler GMTITraceBlitEncoderCopyFromTextureToBuffer2Handler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, CopyFromTexturesourceSlicesourceLevelsourceOriginsourceSizetoBufferdestinationOffsetdestinationBytesPerRowdestinationBytesPerImageoptions, void, id<MTLTexture> Tex, NSUInteger slice, NSUInteger level, MTLPPOrigin origin, MTLPPSize size, id<MTLBuffer> dest, NSUInteger offset, NSUInteger bpr, NSUInteger bpi, MTLBlitOption Opts)
{
	GMTITraceBlitEncoderCopyFromTextureToBuffer2Handler.Trace(Obj, Tex, slice, level, origin, size, dest, offset, bpr, bpi, Opts);
	Original(Obj, Cmd, Tex, slice, level, origin, size, dest, offset, bpr, bpi, Opts);
}

struct MTITraceBlitEncoderGenerateMipmapsForTextureHandler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderGenerateMipmapsForTextureHandler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "GenerateMipmapsForTexture")
	{
		
	}
	
	void Trace(id Object, id<MTLTexture> Tex)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Tex;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Tex;
		
		fs >> Tex;
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) generateMipmapsForTexture:MTITrace::Get().FetchObject(Tex)];
	}
};
static MTITraceBlitEncoderGenerateMipmapsForTextureHandler GMTITraceBlitEncoderGenerateMipmapsForTextureHandler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, GenerateMipmapsForTexture, void, id<MTLTexture> Tex)
{
	GMTITraceBlitEncoderGenerateMipmapsForTextureHandler.Trace(Obj, Tex);
	Original(Obj, Cmd, Tex);
}

struct MTITraceBlitEncoderFillBufferRangeValueHandler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderFillBufferRangeValueHandler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "FillBufferRangeValue")
	{
		
	}
	
	void Trace(id Object, id <MTLBuffer> Buffer, NSRange Range, uint8_t Val)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Buffer;
		fs << Range.location;
		fs << Range.length;
		fs << Val;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Buffer;
		NSRange Range;
		uint8_t Val;
		
		fs >> Buffer;
		fs >> Range.location;
		fs >> Range.length;
		fs >> Val;
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) fillBuffer:MTITrace::Get().FetchObject(Buffer) range:Range value:Val];
	}
};
static MTITraceBlitEncoderFillBufferRangeValueHandler GMTITraceBlitEncoderFillBufferRangeValueHandler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, FillBufferRangeValue, void, id <MTLBuffer> Buffer, NSRange Range, uint8_t Val)
{
	GMTITraceBlitEncoderFillBufferRangeValueHandler.Trace(Obj, Buffer, Range, Val);
	Original(Obj, Cmd, Buffer, Range, Val);
}

struct MTITraceBlitEncoderCopyFromBufferSourceOffsetToBufferDestinationOffsetSizeHandler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderCopyFromBufferSourceOffsetToBufferDestinationOffsetSizeHandler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "CopyFromBufferSourceOffsetToBufferDestinationOffsetSize")
	{
		
	}
	
	void Trace(id Object, id <MTLBuffer> Buffer, NSUInteger Offset, id <MTLBuffer> Dest,  NSUInteger DstOffset, NSUInteger Size)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Buffer;
		fs << Offset;
		fs << (uintptr_t)Dest;
		fs << DstOffset;
		fs << Size;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Buffer;
		NSUInteger Offset;
		uintptr_t Dest;
		NSUInteger DstOffset;
		NSUInteger Size;
		
		fs >> Buffer;
		fs >> Offset;
		fs >> Dest;
		fs >> DstOffset;
		fs >> Size;
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) copyFromBuffer:MTITrace::Get().FetchObject(Buffer) sourceOffset:Offset toBuffer:MTITrace::Get().FetchObject(Dest) destinationOffset:DstOffset size:Size];
	}
};
static MTITraceBlitEncoderCopyFromBufferSourceOffsetToBufferDestinationOffsetSizeHandler GMTITraceBlitEncoderCopyFromBufferSourceOffsetToBufferDestinationOffsetSizeHandler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, CopyFromBufferSourceOffsetToBufferDestinationOffsetSize, void, id <MTLBuffer> Buffer, NSUInteger Offset, id <MTLBuffer> Dest,  NSUInteger DstOffset, NSUInteger Size)
{
	GMTITraceBlitEncoderCopyFromBufferSourceOffsetToBufferDestinationOffsetSizeHandler.Trace(Obj, Buffer, Offset, Dest, DstOffset, Size);
	Original(Obj, Cmd, Buffer, Offset, Dest, DstOffset, Size);
}

struct MTITraceBlitEncoderUpdateFenceHandler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderUpdateFenceHandler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "UpdateFence")
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
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) updateFence:MTITrace::Get().FetchObject(Res)];
	}
};
static MTITraceBlitEncoderUpdateFenceHandler GMTITraceBlitEncoderUpdateFenceHandler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, UpdateFence, void, id <MTLFence> Fence)
{
	GMTITraceBlitEncoderUpdateFenceHandler.Trace(Obj, Fence);
	Original(Obj, Cmd, Fence);
}

struct MTITraceBlitEncoderWaitForFenceHandler : public MTITraceCommandHandler
{
	MTITraceBlitEncoderWaitForFenceHandler()
	: MTITraceCommandHandler("MTLBlitCommandEncoder", "WaitForFence")
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
		
		[(id<MTLBlitCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) waitForFence:MTITrace::Get().FetchObject(Res)];
	}
};
static MTITraceBlitEncoderWaitForFenceHandler GMTITraceBlitEncoderWaitForFenceHandler;
INTERPOSE_DEFINITION(MTIBlitCommandEncoderTrace, WaitForFence, void, id <MTLFence> Fence)
{
	GMTITraceBlitEncoderWaitForFenceHandler.Trace(Obj, Fence);
	Original(Obj, Cmd, Fence);
}

MTLPP_END
