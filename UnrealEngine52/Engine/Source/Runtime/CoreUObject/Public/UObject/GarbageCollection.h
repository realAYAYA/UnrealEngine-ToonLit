// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GarbageCollection.h: Unreal realtime garbage collection helpers
=============================================================================*/

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "HAL/PlatformCrt.h"
#include "HAL/ThreadSafeBool.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Serialization/ArchiveUObject.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"

class FProperty;
class UObject;

#if !defined(UE_WITH_GC)
#	define UE_WITH_GC	1
#endif

/** Context sensitive keep flags for garbage collection */
#define GARBAGE_COLLECTION_KEEPFLAGS	(GIsEditor ? RF_Standalone : RF_NoFlags)

#define	ENABLE_GC_DEBUG_OUTPUT					1
#define PERF_DETAILED_PER_CLASS_GC_STATS				(LOOKING_FOR_PERF_ISSUES || 0) 

/** UObject pointer checks are disabled by default in shipping and test builds as they add roughly 20% overhead to GC times */
#ifndef ENABLE_GC_OBJECT_CHECKS
	#define ENABLE_GC_OBJECT_CHECKS (!(UE_BUILD_TEST || UE_BUILD_SHIPPING) || 0)
#endif

/** Token debug info (token names) enabled in non-shipping builds */
#define ENABLE_GC_TOKEN_DEBUG_INFO (!UE_BUILD_SHIPPING)

#define ENABLE_GC_HISTORY (!UE_BUILD_SHIPPING)

COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogGarbage, Warning, All);
DECLARE_STATS_GROUP(TEXT("Garbage Collection"), STATGROUP_GC, STATCAT_Advanced);

/**
 * Do extra checks on GC'd function references to catch uninitialized pointers?
 * These checks are possibly producing false positives now that our memory use is going over 128Gb = 2^39.
 */
#define DO_POINTER_CHECKS_ON_GC 0 

namespace UE::GC
{
struct FWorkerContext;
struct FTokenId;
}

using FGCArrayStruct = UE::GC::FWorkerContext;

/*-----------------------------------------------------------------------------
	Realtime garbage collection helper classes.
-----------------------------------------------------------------------------*/

/**
 * Enum of different supported reference type tokens.
 */
enum EGCReferenceType
{
	GCRT_None = 0,
	GCRT_Object,							// Scalar reference, i.e. MyObject* and TObjectPtr<MyObject>
	GCRT_ExternalPackage,					// UObject external package
	GCRT_ArrayObject,						// Array of references, e.g. TArray<MyObject*> and TArray<TObjectPtr<MyObject>>
	GCRT_ArrayStruct,						// Array of structs
	GCRT_FixedArray,						// C-style fixed arrays, e.g. SomeType Props[4]
	GCRT_AddStructReferencedObjects,		// AddStructReferencedObjects() function pointer
	GCRT_AddReferencedObjects,				// AddReferencedObjects() function pointer
	GCRT_AddTMapReferencedObjects,			// Map whose value or key contain references
	GCRT_AddTSetReferencedObjects,			// Map whose elements contain references
	GCRT_AddFieldPathReferencedObject,		// Field path strong reference to owner
	GCRT_ArrayAddFieldPathReferencedObject, // Array of field paths
	GCRT_EndOfPointer,						// Comes after pointer
	GCRT_EndOfStream,						// Terminator
	GCRT_ArrayObjectFreezable,				// Freezable array of references, e.g. TArray<MyObject*, FMemoryImageAllocator>
	GCRT_ArrayStructFreezable,				// Freezable array of structs
	GCRT_Optional,							// TOptional
	GCRT_WeakObject,						// TWeakObjectPtr
	GCRT_ArrayWeakObject,					// Array of weak pointers
	GCRT_LazyObject,						// TLazyObjectPtr
	GCRT_ArrayLazyObject,					// Array of lazy pointers
	GCRT_SoftObject,						// TSoftObjectPtr
	GCRT_ArraySoftObject,					// Array of soft object pointers
	GCRT_Delegate,							// UobjectDelegate weak reference to target object
	GCRT_ArrayDelegate,						// Array of delegates
	GCRT_MulticastDelegate,					// Delegate weak reference to target object
	GCRT_ArrayMulticastDelegate,			// Array of delegates
	GCRT_DynamicallyTypedValue,				// FDynamicallyTypedValue
	GCRT_SlowAddReferencedObjects,			// AddReferencedObjects() function using UE::GC::RegisterSlowImplementation
};

enum class EGCTokenType : uint32
{
	Native = 0,
	NonNative = 1	
};

/** Transcodes uint32 token <-> reflection info for one reference property */
struct FGCReferenceInfo
{
	FORCEINLINE FGCReferenceInfo(EGCReferenceType InReferenceType, uint32 InOffset)
		: ReturnCount(0)
		, Type(InReferenceType)
		, Offset(InOffset)
	{
		static_assert(sizeof(FGCReferenceInfo) == sizeof(uint32));
		checkf(InReferenceType != GCRT_None && InReferenceType <= 0x1F, TEXT("Invalid GC Token Reference Type (%d)"), (uint32)InReferenceType);
		checkf((InOffset & ~0x7FFFF) == 0, TEXT("Invalid GC Token Offset (%d), max is %d"), InOffset, 0x7FFFF);
	}

	FORCEINLINE explicit FGCReferenceInfo(uint32 InValue) : Value(InValue) {}

	FORCEINLINE operator uint32() const { return Value; }

	union
	{
		struct
		{
			/** Return depth, e.g. 1 for last entry in an array, 2 for last entry in an array of structs of arrays, ... */
			uint32 ReturnCount	: 8;
			/** Type of reference */
			uint32 Type			: 5; // The number of bits needs to match TFastReferenceCollector::FStackEntry::ContainerHelperType
			/** Offset into struct/ object */
			uint32 Offset		: 19;
		};
		/** uint32 value of reference info, used for easy conversion to/ from uint32 for token array */
		uint32 Value;
	};
};

/** Transcodes uint32 token <-> info needed to skip a dynamic array */
struct FGCSkipInfo
{
	FORCEINLINE FGCSkipInfo() {}
	FORCEINLINE explicit FGCSkipInfo(uint32 InValue) : Value(InValue) {}

	FORCEINLINE operator uint32() const { return Value; }

	union
	{
		/** Mapping to exactly one uint32 */
		struct
		{
			/** Return depth not taking into account embedded arrays. This is needed to return appropriately when skipping empty dynamic arrays as we never step into them */
			uint32 InnerReturnCount	: 8;
			/** Skip index */
			uint32 SkipIndex			: 24;
		};
		/** uint32 value of skip info, used for easy conversion to/ from uint32 for token array */
		uint32 Value;
	};
};

/** Token debug name and offset */
struct FTokenInfo
{
	int32 Offset;
	FName Name;
};

namespace UE::GC {

enum class EAROFlags
{
	None			= 0,
	Unbalanced		= 1 << 0,		// Some instances are very slow but most are fast. GC can flush these more frequently.
	ExtraSlow		= 2 << 0,		// All instances are slow. GC can work-steal these at finer batch granularity.
};
ENUM_CLASS_FLAGS(EAROFlags);

// Reference collection batches up slow AddReferencedObjects calls
COREUOBJECT_API void RegisterSlowImplementation(void (*AddReferencedObjects)(UObject*, FReferenceCollector&), EAROFlags Flags = EAROFlags::None);

struct FTokenStreamOwner;
class FTokenStreamBuilderIterator;

class /* ensure same cache line */ alignas(16) FTokenStreamView
{
public:
	FTokenStreamView() : StackSize(0), DroppedNum(0), IsNonNative(0) {}

	FORCEINLINE EGCTokenType GetTokenType() const
	{
		return IsNonNative ? EGCTokenType::NonNative : EGCTokenType::Native;
	}

	FORCEINLINE FGCReferenceInfo AccessReferenceInfo(uint32 CurrentIndex) const
	{
		return FGCReferenceInfo(Tokens[CurrentIndex]);
	}

	FORCEINLINE uint32 ReadStride(uint32& CurrentIndex) const
	{
		return Tokens[CurrentIndex++];
	}

	FORCEINLINE FGCSkipInfo ReadSkipInfo(uint32& CurrentIndex) const
	{
		FGCSkipInfo SkipInfo(Tokens[CurrentIndex]);
		SkipInfo.SkipIndex += CurrentIndex;
		CurrentIndex++;
		return SkipInfo;
	}

	/**
	 * Read return count stored at the index before the skip index. This is required 
	 * to correctly return the right amount of levels when skipping over an empty array.
	 *
	 * @param SkipIndex index of first token after array
	 */
	FORCEINLINE uint8 GetSkipReturnCount( FGCSkipInfo SkipInfo ) const
	{
		check( SkipInfo.SkipIndex > 0 && SkipInfo.SkipIndex <= Num );		
		FGCReferenceInfo ReferenceInfo(Tokens[SkipInfo.SkipIndex-1]);
		check( ReferenceInfo.Type != GCRT_None );
		return static_cast<uint8>(ReferenceInfo.ReturnCount - SkipInfo.InnerReturnCount);
	}

	FORCEINLINE uint32 ReadCount(uint32& CurrentIndex) const
	{
		return Tokens[CurrentIndex++];
	}

	FORCEINLINE void* ReadPointer(uint32& CurrentIndex)
	{
		UPTRINT Result = UPTRINT(Tokens[CurrentIndex]) | (UPTRINT(Tokens[CurrentIndex+1]) << 32);
		CurrentIndex += 2;
		return (void*)Result;
	}

	bool IsEmpty() const { return Num == 0; }
	uint32 NumTokens() const { return Num; }
	void* GetTokenData() const { return Tokens; }
private:
	friend FTokenStreamOwner;
	friend class FTokenStreamBuilder;
	static constexpr uint32 NumStackSizeBits = 28;
	static constexpr uint32 NumDroppedNumBits = 3;

	uint32* RESTRICT Tokens = nullptr; // Debug names stored after tokens
	uint32 Num = 0;
	uint32 StackSize : NumStackSizeBits;
	uint32 DroppedNum : NumDroppedNumBits;
	uint32 IsNonNative : 1;

#if ENABLE_GC_TOKEN_DEBUG_INFO
	const FName* GetDebugNames() const { return reinterpret_cast<const FName*>(Tokens + Num + DroppedNum); }
#endif
};


struct FTokenStreamOwner
{
	UE_NONCOPYABLE(FTokenStreamOwner);
	FTokenStreamOwner() = default;
	~FTokenStreamOwner() { Reset(); }

	FTokenStreamView Strong; // Strong references only
	FTokenStreamView Mixed; // Strong and weak references
	
	COREUOBJECT_API void Reset();

	/** Gets the number of bytes allocated for tokens */
	int64 GetTokenAllocatedSize() const;

	/** Gets the number of bytes allocated for token debug info */
	int64 GetDebugInfoAllocatedSize() const;

	/** Returns extended information about a token, including its name (if available) */
	FTokenInfo GetTokenInfo(UE::GC::FTokenId Id) const;
};

/** Reference token stream class. Used for creating and parsing stream of object references. */
class FTokenStreamBuilder
{
public:
	explicit FTokenStreamBuilder(UClass& Owner) : DebugOwner(Owner) {}

	uint32 Num() const { return static_cast<uint32>(Tokens.Num()); }
	bool IsEmpty() const { return Tokens.Num() == 0; }
	void SetStackSize(int32 InStackSize) { StackSize = InStackSize; }
	uint32 GetStackSize() const { return StackSize; }
	void SetTokenType(EGCTokenType InType) { TokenType = InType; }
	EGCTokenType GetTokenType() const { return TokenType; }

	/** Emit reference info and return index of the reference info */
	COREUOBJECT_API int32 EmitReferenceInfo(FGCReferenceInfo ReferenceInfo, FName DebugName);

	/**
	 * Emit placeholder for array skip index, updated in UpdateSkipIndexPlaceholder
	 *
	 * @return the index of the skip index, used later in UpdateSkipIndexPlaceholder
	 */
	COREUOBJECT_API uint32 EmitSkipIndexPlaceholder();

	/**
	 * Updates skip index place holder stored and passed in skip index index with passed
	 * in skip index. The skip index is used to skip over tokens in the case of an empty 
	 * dynamic array.
	 * 
	 * @param SkipIndexIndex index where skip index is stored at.
	 * @param SkipIndex index to store at skip index index
	 */
	COREUOBJECT_API void UpdateSkipIndexPlaceholder(uint32 SkipIndexIndex, uint32 SkipIndex);

	int32 EmitCount(uint32 Count);
	int32 EmitPointer(const void* Ptr);
	COREUOBJECT_API int32 EmitStride(uint32 Stride);

	/**
	 * Increase return count on last token.
	 *
	 * @return index of next token
	 */
	COREUOBJECT_API uint32 EmitReturn();

	COREUOBJECT_API void EmitObjectReference(int32 Offset, FName DebugName, EGCReferenceType Kind = GCRT_Object);
	COREUOBJECT_API void EmitObjectArrayReference(int32 Offset, FName DebugName);
	COREUOBJECT_API uint32 EmitStructArrayBegin(int32 Offset, FName DebugName, int32 Stride);

	/** The index following the current one will be written to the passed in SkipIndexIndex to skip tokens for empty dynamic arrays. */
	COREUOBJECT_API void EmitStructArrayEnd(uint32 SkipIndexIndex);

	/** All tokens issues between Begin and End will be replayed Count times. */
	COREUOBJECT_API void EmitFixedArrayBegin(int32 Offset, FName DebugName, int32 Stride, int32 Count);
	COREUOBJECT_API void EmitFixedArrayEnd();
	COREUOBJECT_API void EmitExternalPackageReference();

	void EmitFinalTokens(void(*AddReferencedObjects)(UObject*, FReferenceCollector&));	
	
	static FTokenStreamView DropFinalTokens(FTokenStreamView Mixed, void(*DropARO)(UObject*, FReferenceCollector&));

	/** Allocate merged stream with tokens from super class and current class */
	static void Merge(FTokenStreamView& Out, const FTokenStreamBuilder& Class, FTokenStreamView Super);
	
	class FConstIterator;
	static void CopyStrongTokens(FConstIterator MixedIt, FTokenStreamBuilder& Out);
	static bool CopyNextStrongToken(FConstIterator& /* in-out */ MixedIt, FTokenStreamBuilder& Out, uint32& OutReturnCount);

	/** Reads count and advances stream */
	FORCEINLINE uint32 ReadCount(uint32& CurrentIndex)
	{
		return Tokens[CurrentIndex++];
	}

	/** Reads stride and advances stream */
	FORCEINLINE uint32 ReadStride(uint32& CurrentIndex)
	{
		return Tokens[CurrentIndex++];
	}

	/** Reads pointer and advance stream */
	FORCEINLINE void* ReadPointer(uint32& CurrentIndex) const
	{
		UPTRINT Result = UPTRINT(Tokens[CurrentIndex++]);
		Result |= UPTRINT(Tokens[CurrentIndex++]) << 32;
		return (void*)Result;
	}

	/** Reads in reference info and advances stream. */
	FORCEINLINE FGCReferenceInfo ReadReferenceInfo( uint32& CurrentIndex )
	{
		return FGCReferenceInfo(Tokens[CurrentIndex++]);
	}

	/**
	 * Access reference info at passed in index. Used as helper to eliminate LHS.
	 *
	 * @return Reference info at passed in index
	 */
	FORCEINLINE FGCReferenceInfo AccessReferenceInfo(uint32 CurrentIndex) const
	{
		return FGCReferenceInfo(Tokens[CurrentIndex]);
	}

	/**
	 * Read in skip index and advances stream.
	 *
	 * @return read in skip index
	 */
	FORCEINLINE FGCSkipInfo ReadSkipInfo( uint32& CurrentIndex )
	{
		FGCSkipInfo SkipInfo(Tokens[CurrentIndex]);
		SkipInfo.SkipIndex += CurrentIndex;
		CurrentIndex++;
		return SkipInfo;
	}

	/**
	 * Read return count stored at the index before the skip index. This is required 
	 * to correctly return the right amount of levels when skipping over an empty array.
	 *
	 * @param SkipIndex index of first token after array
	 */
	FORCEINLINE uint8 GetSkipReturnCount( FGCSkipInfo SkipInfo )
	{
		check( SkipInfo.SkipIndex > 0 && SkipInfo.SkipIndex <= (uint32)Tokens.Num() );		
		FGCReferenceInfo ReferenceInfo(Tokens[SkipInfo.SkipIndex-1]);
		check( ReferenceInfo.Type != GCRT_None );
		return static_cast<uint8>(ReferenceInfo.ReturnCount - SkipInfo.InnerReturnCount);
	}

	/**
	 * Queries the stream for an end of stream condition
	 *
	 * @return true if the end of the stream has been reached, false otherwise
	 */
	FORCEINLINE bool EndOfStream(uint32 CurrentIndex)
	{
		return CurrentIndex >= (uint32)Tokens.Num();
	}

	/** Gets the maximum stack size required by all token streams */
	FORCEINLINE static uint32 GetMaxStackSize()
	{
		return MaxStackSize;
	}

	friend bool operator==(const FTokenStreamBuilder& A, const FTokenStreamBuilder& B)
	{
		return A.Tokens == B.Tokens && A.StackSize == B.StackSize && A.TokenType == B.TokenType;
	}

private:
	static constexpr uint32 SkipIndexPlaceholderMagic =  0xDEDEDEDEu; 

	/**
	 * Helper function to store a pointer into a preallocated token stream.
	 *
	 * @param Stream Preallocated token stream.
	 * @param Ptr pointer to store
	 */
	FORCEINLINE void StorePointer(uint32* Stream, const void* Ptr)
	{
		Stream[0] = UPTRINT(Ptr) & 0xffffffffu;
		Stream[1] = UPTRINT(Ptr) >> 32;
	}

	/** Stack size required by this token stream */
	uint32 StackSize = 0;
	EGCTokenType TokenType = EGCTokenType::Native;
	TArray<uint32, TInlineAllocator<128>> Tokens;
#if ENABLE_GC_TOKEN_DEBUG_INFO
	TArray<FName, TInlineAllocator<128>> DebugNames;
#endif
	UClass& DebugOwner;

	/** Maximum stack size for TFastReferenceCollector */
	COREUOBJECT_API static uint32 MaxStackSize;
};

struct FIntrinsicClassTokens
{
	COREUOBJECT_API static FTokenStreamBuilder& AllocateBuilder(UClass* Class);
	static TUniquePtr<FTokenStreamBuilder> ConsumeBuilder(UClass* Class);
};

} // namespace UE::GC

/** Prevent GC from running in the current scope */
class COREUOBJECT_API FGCScopeGuard
{
public:
	FGCScopeGuard();
	~FGCScopeGuard();
};

class FGCObject;

/** Information about references to objects marked as Garbage that's gather by the Garbage Collector */
struct FGarbageReferenceInfo
{
	/** Object marked as garbage */
	UObject* GarbageObject;
	/** Referencing object info */
	union FReferencerUnion
	{
		/** Referencing UObject */
		const UObject* Object;
		/** Referencing FGCObject */
		FGCObject* GCObject;
	} Referencer;
	/** True if the referencing object is a UObject. If false the referencing object is an FGCObject */
	bool bReferencerUObject;
	/** Referencing property name */
	FName PropertyName;

	FGarbageReferenceInfo(const UObject* InReferencingObject, UObject* InGarbageObject, FName InPropertyName)
		: GarbageObject(InGarbageObject)
		, bReferencerUObject(true)
		, PropertyName(InPropertyName)
	{
		Referencer.Object = InReferencingObject;
	}
	FGarbageReferenceInfo(FGCObject* InReferencingObject, UObject* InGarbageObject)
		: GarbageObject(InGarbageObject)
		, bReferencerUObject(false)
	{
		Referencer.GCObject = InReferencingObject;
	}

	/** Returns a formatted string with referencing object info */
	FString GetReferencingObjectInfo() const;
};

struct FGCDirectReference
{
	FGCDirectReference() = default;
	explicit FGCDirectReference(UObject* Obj) : ReferencedObject(Obj) {}
	/** Property or FGCObject name referencing this object */
	FName ReferencerName;
	UObject* ReferencedObject = nullptr;
};

/** True if Garbage Collection is running. Use IsGarbageCollecting() functio n instead of using this variable directly */
extern COREUOBJECT_API bool GIsGarbageCollecting;

/**
 * Gets the last time that the GC was run.
 *
 * @return	Returns the FPlatformTime::Seconds() for the last garbage collection, 0 if GC has never run.
 */
COREUOBJECT_API double GetLastGCTime();

/**
* Whether we are inside garbage collection
*/
FORCEINLINE bool IsGarbageCollecting()
{
	return GIsGarbageCollecting;
}

/**
* Whether garbage collection is locking the global uobject hash tables
*/
COREUOBJECT_API bool IsGarbageCollectingAndLockingUObjectHashTables();

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
