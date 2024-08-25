// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "UObject/NameTypes.h"

class FChunkedFixedUObjectArray;

// Boilerplate that is included once for each module, even in monolithic builds
#if !defined(PER_MODULE_BOILERPLATE_ANYLINK)
#define PER_MODULE_BOILERPLATE_ANYLINK(ModuleImplClass, ModuleName)
#endif

/**
 * Override new + delete operators (and array versions) in every module.
 * This prevents the possibility of mismatched new/delete calls such as a new[] that
 * uses Unreal's allocator and a delete[] that uses the system allocator.
 *
 * Overloads have to guarantee at least 1 byte is allocated because
 * otherwise new T[0] could return a null pointer, as could ::operator new(0), depending
 * on the allocator (e.g. TBB), which is non-standard behaviour.
 * 
 * StdMalloc, StdRealloc and StdFree have been added for thirdparty libraries that need malloc. These
 * functions will allow for proper memory tracking.
 */
#if USING_CODE_ANALYSIS
	#define OPERATOR_NEW_MSVC_PRAGMA MSVC_PRAGMA( warning( suppress : 28251 ) )	//	warning C28251: Inconsistent annotation for 'new': this instance has no annotations
#else
	#define OPERATOR_NEW_MSVC_PRAGMA
#endif

// Disable the replacement new/delete when running the Clang static analyzer, due to false positives in 15.0.x:
// https://github.com/llvm/llvm-project/issues/58820
// For AutoRTFM when FORCE_ANSI_ALLOCATOR is specified we still need to re-route operator new/delete (to deal
// with transactionalization of memory allocation). Address sanitizer will still work even with this re-routing
// because we point the underlying FMemory allocator at ansi malloc/free, which ASan still hijacks for its
// purposes.
#if !(FORCE_ANSI_ALLOCATOR && !(defined(__AUTORTFM) && __AUTORTFM)) && !defined(__clang_analyzer__)
static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ <= 16, "Expecting 16-byte default operator new alignment - alignments > 16 may have bloat");
#define REPLACEMENT_OPERATOR_NEW_AND_DELETE \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new  ( size_t Size                                                    ) OPERATOR_NEW_THROW_SPEC      { return FMemory::Malloc( Size ? Size : 1, __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new[]( size_t Size                                                    ) OPERATOR_NEW_THROW_SPEC      { return FMemory::Malloc( Size ? Size : 1, __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new  ( size_t Size,                             const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return FMemory::Malloc( Size ? Size : 1, __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new[]( size_t Size,                             const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return FMemory::Malloc( Size ? Size : 1, __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new  ( size_t Size, std::align_val_t Alignment                        ) OPERATOR_NEW_THROW_SPEC      { return FMemory::Malloc( Size ? Size : 1, (std::size_t)Alignment ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new[]( size_t Size, std::align_val_t Alignment                        ) OPERATOR_NEW_THROW_SPEC      { return FMemory::Malloc( Size ? Size : 1, (std::size_t)Alignment ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new  ( size_t Size, std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return FMemory::Malloc( Size ? Size : 1, (std::size_t)Alignment ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new[]( size_t Size, std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return FMemory::Malloc( Size ? Size : 1, (std::size_t)Alignment ); } \
	void operator delete  ( void* Ptr                                                                             ) OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr                                                                             ) OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete  ( void* Ptr,                                                      const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr,                                                      const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void operator delete  ( void* Ptr,             size_t Size                                                    ) OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr,             size_t Size                                                    ) OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete  ( void* Ptr,             size_t Size,                             const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr,             size_t Size,                             const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void operator delete  ( void* Ptr,                          std::align_val_t Alignment                        ) OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr,                          std::align_val_t Alignment                        ) OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete  ( void* Ptr,                          std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr,                          std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void operator delete  ( void* Ptr,             size_t Size, std::align_val_t Alignment                        ) OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr,             size_t Size, std::align_val_t Alignment                        ) OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete  ( void* Ptr,             size_t Size, std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr,             size_t Size, std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void* StdMalloc( size_t Size, size_t Alignment ) { return FMemory::Malloc( Size ? Size : 1, Alignment ); } \
	void* StdRealloc( void* Original, size_t Size, size_t Alignment ) { return FMemory::Realloc(Original, Size ? Size : 1, Alignment ); } \
	void StdFree( void *Ptr ) { FMemory::Free( Ptr ); } 

#else
	#define REPLACEMENT_OPERATOR_NEW_AND_DELETE
#endif

class FChunkedFixedUObjectArray;

#ifdef DISABLE_UE4_VISUALIZER_HELPERS
	#define UE4_VISUALIZERS_HELPERS
#elif PLATFORM_UNIX
	// GDB/LLDB pretty printers don't use these - no need to export additional symbols. This also solves ODR violation reported by ASan on Linux
	#define UE4_VISUALIZERS_HELPERS
#else
	#define UE4_VISUALIZERS_HELPERS \
		uint8** GNameBlocksDebug = FNameDebugVisualizer::GetBlocks(); \
		FChunkedFixedUObjectArray*& GObjectArrayForDebugVisualizers = GCoreObjectArrayForDebugVisualizers; \
		UE::CoreUObject::Private::FStoredObjectPathDebug*& GComplexObjectPathDebug = GCoreComplexObjectPathDebug; \
		UE::CoreUObject::Private::FObjectHandlePackageDebugData*& GObjectHandlePackageDebug = GCoreObjectHandlePackageDebug;
#endif

// in DLL builds, these are done per-module, otherwise we just need one in the application
// visual studio cannot find cross dll data for visualizers, so these provide access
#define PER_MODULE_BOILERPLATE \
	UE4_VISUALIZERS_HELPERS \
	REPLACEMENT_OPERATOR_NEW_AND_DELETE
