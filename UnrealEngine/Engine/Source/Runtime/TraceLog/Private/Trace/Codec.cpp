// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

THIRD_PARTY_INCLUDES_START
#if defined(_MSC_VER)
#	pragma warning(push)
#	pragma warning(disable : 6239)
#endif

#if !defined(TRACE_PRIVATE_EXTERNAL_LZ4)
#	define LZ4_NAMESPACE Trace
#		include "LZ4/lz4.c.inl"
#	undef LZ4_NAMESPACE
#	define TRACE_PRIVATE_LZ4_NAMESPACE ::Trace::
#else
#	define TRACE_PRIVATE_LZ4_NAMESPACE
#endif

#if defined(_MSC_VER)
#	pragma warning(pop)
#endif
THIRD_PARTY_INCLUDES_END

namespace UE {
namespace Trace {
namespace Private {

////////////////////////////////////////////////////////////////////////////////
int32 Encode(const void* Src, int32 SrcSize, void* Dest, int32 DestSize)
{
	return TRACE_PRIVATE_LZ4_NAMESPACE LZ4_compress_fast(
		(const char*)Src,
		(char*)Dest,
		SrcSize,
		DestSize,
		1 // increase by 1 for small speed increase
	);
}

////////////////////////////////////////////////////////////////////////////////
uint32 GetEncodeMaxSize(uint32 InputSize)
{
	return LZ4_COMPRESSBOUND(InputSize);
}

////////////////////////////////////////////////////////////////////////////////
TRACELOG_API int32 Decode(const void* Src, int32 SrcSize, void* Dest, int32 DestSize)
{
	return TRACE_PRIVATE_LZ4_NAMESPACE LZ4_decompress_safe((const char*)Src, (char*)Dest, SrcSize, DestSize);
}

} // namespace Private
} // namespace Trace
} // namespace UE
