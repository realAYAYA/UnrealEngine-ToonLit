// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "Async/TaskGraphInterfaces.h"
#include "UObject/UnrealType.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformProcess.h"
#include "UObject/FieldPath.h"
#include "Async/ParallelFor.h"
#include "UObject/UObjectArray.h"
#include "UObject/DynamicallyTypedValue.h"
#include "UObject/GCObject.h"

/** Token stream stack overflow checks are not enabled in test and shipping configs */
#define UE_ENABLE_TOKENSTREAM_STACKOVERFLOW_CHECKS (!(UE_BUILD_TEST || UE_BUILD_SHIPPING) || 0)

namespace UE::GC { class FWorkCoordinator; }

/*=============================================================================
	FastReferenceCollector.h: Unreal realtime garbage collection helpers
=============================================================================*/

enum class EGCOptions : uint32
{
	None = 0,
	Parallel = 1 << 0,					// Use all task workers to collect references, must be started on main thread
	AutogenerateTokenStream = 1 << 1,	// Lazily generate token streams for new UClasses
	WithClusters = 1 << 2,				// Use clusters, see FUObjectCluster
	ProcessWeakReferences UE_DEPRECATED(5.2, "Weak reference collection deprecated to reduce complexity. Use FArchive instead with ArIsObjectReferenceCollector and ArIsModifyingWeakAndStrongReferences")
					= 1 << 3,			// Collect both weak and strong references instead of just strong
	WithPendingKill = 1 << 4,			// Internal flag used by reachability analysis
};
ENUM_CLASS_FLAGS(EGCOptions);

inline constexpr bool IsParallel(EGCOptions Options) { return !!(Options & EGCOptions::Parallel); }
inline constexpr bool IsPendingKill(EGCOptions Options) { return !!(Options & EGCOptions::WithPendingKill); }
PRAGMA_DISABLE_DEPRECATION_WARNINGS
inline constexpr bool ShouldProcessWeakReferences(EGCOptions Options) { return !!(Options & EGCOptions::ProcessWeakReferences); }
PRAGMA_ENABLE_DEPRECATION_WARNINGS

using EFastReferenceCollectorOptions UE_DEPRECATED(5.2, "Use EGCOptions instead") = EGCOptions;

/** Helper to give GC internals friend access to certain core classes */
struct FGCInternals
{
	FORCEINLINE static FUObjectItem* GetResolvedOwner(FFieldPath& Path) { return Path.GetResolvedOwnerItemInternal(); }
	FORCEINLINE static void ClearCachedField(FFieldPath& Path) { Path.ClearCachedFieldInternal(); }

	template<class ObjectID>
	FORCEINLINE static FWeakObjectPtr& AccessWeakPtr(TPersistentObjectPtr<ObjectID>& Ptr) { return Ptr.WeakPtr; }
};

/** Interface to allow external systems to trace additional object references, used for bridging GCs */
class FGarbageCollectionTracer
{
public:
	virtual ~FGarbageCollectionTracer() {}
	virtual void PerformReachabilityAnalysisOnObjects(UE::GC::FWorkerContext* Context, EGCOptions Options) = 0;
};

//////////////////////////////////////////////////////////////////////////

namespace UE::GC
{

static constexpr uint32 ObjectLookahead = 16;

// Prefetches ClassPrivate/OuterPrivate/Class' ReferenceTokens/TokenData while iterating over an object array
//
// Tuned on a Gen5 console using an internal game replay and an in-game GC pass
class FPrefetchingObjectIterator
{
public:
	// Objects must be padded with PadObjects
	explicit FPrefetchingObjectIterator(TConstArrayView<UObject*> Objects)
	: It(Objects.begin())
	, End(Objects.end())
	, PrefetchedTokenStream(Objects.Num() ? &It[1]->GetClass()->ReferenceTokens.Strong : nullptr)
	{}

	FORCEINLINE_DEBUGGABLE void Advance()
	{	
		FPlatformMisc::Prefetch(PrefetchedTokenStream->GetTokenData());
		PrefetchedTokenStream = &It[2]->GetClass()->ReferenceTokens.Strong;

		UObjectBase::PrefetchOuter(It[6]);
		FPlatformMisc::Prefetch(It[6]->GetClass(), offsetof(UClass, ReferenceTokens));
		UObjectBase::PrefetchClass(It[ObjectLookahead]);

		++It;
	}
	
	bool HasMore() const { return It != End; }
	UObject* GetCurrentObject() { return *It; }

private:
	UObject*const* It;
	UObject*const* End;
	FTokenStreamView* PrefetchedTokenStream;
};

// Pad object array for FPrefetchingObjectIterator use
COREUOBJECT_API void PadObjectArray(TArray<UObject*>& Objects);

//////////////////////////////////////////////////////////////////////////

/** Fixed block of reachable objects waiting to be processed */
struct FWorkBlock
{
	static constexpr uint32 ObjectCapacity = 512 - /* Previous */ 1 - ObjectLookahead;

	FWorkBlock* Previous;
	UObject* Objects[ObjectCapacity + ObjectLookahead];

	TArrayView<UObject*> GetObjects() { return MakeArrayView(Objects, ObjectCapacity); }
	TArrayView<UObject*> GetPadding() { return MakeArrayView(Objects + ObjectCapacity, ObjectLookahead); }
};

class FWorkstealingQueue;

/** Reachable objects waiting to be processed. Type-erases parallel/serial queue. */
class FWorkBlockifier
{
public:
	UE_NONCOPYABLE(FWorkBlockifier);
	FWorkBlockifier() = default;
	COREUOBJECT_API ~FWorkBlockifier();
	
	void Init() { AllocateWipBlock(); }
	void SetAsyncQueue(FWorkstealingQueue& Queue) { AsyncQueue = &Queue; }
	void ResetAsyncQueue();

	template<EGCOptions Options>
	FORCEINLINE_DEBUGGABLE void Add(UObject* Object)
	{
		*WipIt = Object;
		if (++WipIt == Wip->GetPadding().GetData())
		{
			if constexpr (IsParallel(Options))
			{
				PushFullBlockAsync();
			}
			else
			{
				PushFullBlockSync();
			}
		}
	}

	FORCEINLINE_DEBUGGABLE FWorkBlock* PopPartialBlock(int32& OutNum)
	{
		if (int32 Num = PartialNum())
		{
			OutNum = Num;
			return PopWipBlock();
		}

		return nullptr;
	}

	template<EGCOptions Options>
	FORCEINLINE FWorkBlock* PopFullBlock()
	{
		return IsParallel(Options) ? PopFullBlockAsync() : PopFullBlockSync();
	}

	FORCEINLINE FWorkBlock* StealFullBlock() const
	{
		return StealAsyncBlock();
	}

	COREUOBJECT_API void FreeOwningBlock(UObject*const* BlockObjects);

	FORCEINLINE bool IsUnused() const
	{
		return PartialNum() == 0 && SyncQueue == nullptr;
	}

	void SetWorkerIndex(int32 Idx) { WorkerIndex = Idx; }
	int32 GetWorkerIndex() const { return WorkerIndex; }

	
private:
	UObject** WipIt; // Wip->Objects cursor
	FWorkBlock* Wip;
	union
	{
		FWorkBlock* SyncQueue = nullptr;
		FWorkstealingQueue* AsyncQueue;
	};
	int32 WorkerIndex = INDEX_NONE;
	
	void AllocateWipBlock();
	COREUOBJECT_API void PushFullBlockSync();
	COREUOBJECT_API void PushFullBlockAsync();
	COREUOBJECT_API FWorkBlock* PopFullBlockSync();
	COREUOBJECT_API FWorkBlock* PopFullBlockAsync();
	COREUOBJECT_API FWorkBlock* PopWipBlock();
	COREUOBJECT_API FWorkBlock* StealAsyncBlock() const;
	FORCEINLINE int32 PartialNum() const { return WipIt - Wip->Objects; }
};

//////////////////////////////////////////////////////////////////////////

struct FSlowARO
{
	COREUOBJECT_API static void CallSync(uint32 SlowAROIndex, UObject* Object, FReferenceCollector& Collector);
	COREUOBJECT_API static bool TryQueueCall(uint32 SlowAROIndex, UObject* Object, FWorkerContext& Context);
	COREUOBJECT_API static void ProcessUnbalancedCalls(FWorkerContext& Context, FReferenceCollector& Collector);
	// @return if any calls were made
	COREUOBJECT_API static bool ProcessAllCalls(FWorkerContext& Context, FReferenceCollector& Collector);
};

//////////////////////////////////////////////////////////////////////////

/** Thread-local context containing initial objects and references to collect */
struct alignas(PLATFORM_CACHE_LINE_SIZE) FWorkerContext
{
private:
	template <typename ProcessorType, typename CollectorType>
	friend class TFastReferenceCollector;
	friend class FSlowAROManager;
	
	// This is set by GC when processing references from the current referencing object
	UObject* ReferencingObject = nullptr;
	TConstArrayView<UObject*> InitialObjects;
public:
	UE_NONCOPYABLE(FWorkerContext);
	COREUOBJECT_API FWorkerContext();
	COREUOBJECT_API ~FWorkerContext();

	FWorkBlockifier ObjectsToSerialize;
	TConstArrayView<UObject**> InitialNativeReferences;
	FWorkCoordinator* Coordinator = nullptr;
	TArray<UObject**> WeakReferences;
#if !UE_BUILD_SHIPPING
	TArray<FGarbageReferenceInfo> GarbageReferences;
	bool bDetectedGarbageReference = false;
#endif
#if ENABLE_GC_HISTORY
	TMap<const UObject*, TArray<FGCDirectReference>*> History;
#endif

	FORCEINLINE UObject* GetReferencingObject()	{ return ReferencingObject;	}

	TConstArrayView<UObject*> GetInitialObjects() { return InitialObjects; }
	void ResetInitialObjects() { InitialObjects = {}; }

	/** @param Objects must outlive this context. It's data is padded by repeating the last object to allow prefetching past the end. */
	void SetInitialObjectsUnpadded(TArray<UObject*>& Objects)
	{
		PadObjectArray(Objects);
		SetInitialObjectsPrepadded(Objects);
	}

	/** @param PaddedObjects must already be padded to allow reading valid objects past the end */
	void SetInitialObjectsPrepadded(TConstArrayView<UObject*> PaddedObjects)
	{
		check(PaddedObjects.IsEmpty() || PaddedObjects.GetData()[PaddedObjects.Num() + ObjectLookahead - 1]->IsValidLowLevel() );
		InitialObjects = PaddedObjects;
	}

	/** Returns the size of memory allocated by internal arrays */
	int64 GetAllocatedSize() const
	{
		return WeakReferences.GetAllocatedSize() + sizeof(FWorkBlock);
	}
	
	FORCEINLINE int32 GetWorkerIndex() const { return ObjectsToSerialize.GetWorkerIndex(); }
	void AllocateWorkerIndex();
	void FreeWorkerIndex();
};

//////////////////////////////////////////////////////////////////////////

/* Debug id for references that lack a token */
enum class ETokenlessId
{
	Collector = 1,
	Class,
	Outer,
	ClassOuter,
	Cluster,
	InitialReference,
	Max = InitialReference
};

/** Debug identifier for a token stream index or a tokenless reference */
struct FTokenId
{
public:
	FORCEINLINE FTokenId(ETokenlessId Tokenless) : Index(0), TokenlessId((uint32)Tokenless), Mixed(0) {}
	FORCEINLINE FTokenId(uint32 Idx, bool bIsMixed) : Index(Idx), TokenlessId(0), Mixed(bIsMixed) {}
	
	bool IsMixed() const { return Mixed; }
	bool IsTokenless() const { return TokenlessId != 0; }
	uint32 GetIndex() const { check(!IsTokenless()); return Index; }
	int32 AsPrintableIndex() const { return IsTokenless() ? -int32(TokenlessId) : Index; }
	ETokenlessId AsTokenless() const { check(IsTokenless()); return (ETokenlessId)TokenlessId;}

	friend bool operator==(FTokenId A, FTokenId B) { return A.All == B.All; }
	friend bool operator!=(FTokenId A, FTokenId B) { return A.All != B.All; }

private:
	static constexpr uint32 TokenlessIdBits = 3;
	static constexpr uint32 IndexBits = 32 - TokenlessIdBits - 1;
	void StaticAssert();

	union
	{
		struct  
		{
			uint32 Index : IndexBits;
			uint32 TokenlessId : TokenlessIdBits;
			uint32 Mixed : 1;
		};
		uint32 All;
	};
};

//////////////////////////////////////////////////////////////////////////

/** Helper struct for stack based approach */
struct FStackEntry
{
	/** Current data pointer, incremented by stride */
	uint8* Data;
	/** Current container property for data pointer. DO NOT rely on its value being initialized. Instead check ContainerType first. */
	FProperty* ContainerProperty;
	/** Pointer to the container being processed by GC. DO NOT rely on its value being initialized. Instead check ContainerType first. */
	void* ContainerPtr;
	/** Current index within the container. DO NOT rely on its value being initialized. Instead check ContainerType first. */
	int32	ContainerIndex;
	/** Current container helper type */
	uint32	ContainerType : 5; // The number of bits needs to match FGCReferenceInfo::Type
	/** Current stride */
	uint32	Stride : 27; // This will always be bigger (8 bits more) than FGCReferenceInfo::Ofset which is the max offset GC can handle
	/** Current loop count, decremented each iteration */
	int32	Count;
	/** First token index in loop */
	int32	LoopStartIndex;

	FORCEINLINE bool MoveToNextContainerElementAndCheckIfValid()
	{
		if (ContainerType == GCRT_AddTMapReferencedObjects)
		{
			return ((FMapProperty*)ContainerProperty)->IsValidIndex(ContainerPtr, ++ContainerIndex);
		}
		else if (ContainerType == GCRT_AddTSetReferencedObjects)
		{
			return ((FSetProperty*)ContainerProperty)->IsValidIndex(ContainerPtr, ++ContainerIndex);
		}
		
		return true;
	}
};

//////////////////////////////////////////////////////////////////////////

/** Helper class to manage GC token stream stack  */
class FTokenStreamStack
{
	/** Allocated stack memory */
	TArray<FStackEntry> Stack;
	/** Current stack frame */
	FStackEntry* CurrentFrame = nullptr;
#if UE_ENABLE_TOKENSTREAM_STACKOVERFLOW_CHECKS
	/** Number of used stack frames (for debugging) */
	int32 NumberOfUsedStackFrames = 0;
#endif // UE_ENABLE_TOKENSTREAM_STACKOVERFLOW_CHECKS

public:
	FTokenStreamStack()
	{
		Stack.AddUninitialized(FTokenStreamBuilder::GetMaxStackSize());
	}

	/** Initializes the stack and returns its first frame */
	FORCEINLINE FStackEntry* Initialize()
	{
#if UE_ENABLE_TOKENSTREAM_STACKOVERFLOW_CHECKS
		NumberOfUsedStackFrames = 1;
#endif // UE_ENABLE_TOKENSTREAM_STACKOVERFLOW_CHECKS
		// Grab te first frame. From now on we'll be using it instead of TArray to move to the next one and back
		// in order to skip any extra internal TArray checks and overhead
		CurrentFrame = GetBottomFrame();
		return CurrentFrame;
	}

	/** Returns the frame at the bottom of the stack (the first one) */
	FORCEINLINE FStackEntry* GetBottomFrame()
	{
		return Stack.GetData();
	}

	/** Advances to the next frame on the stack */
	FORCEINLINE FStackEntry* Push()
	{
		checkSlow(NumberOfUsedStackFrames > 0); // sanity check to make sure Push() gets called after Initialize()
#if UE_ENABLE_TOKENSTREAM_STACKOVERFLOW_CHECKS
		NumberOfUsedStackFrames++;
		UE_CLOG(NumberOfUsedStackFrames > Stack.Num(), LogGarbage, Fatal, TEXT("Ran out of stack space for GC Token Stream. FGCReferenceTokenStream::GetMaxStackSize() = %d must be miscalculated. Verify EmitReferenceInfo code."), FTokenStreamBuilder::GetMaxStackSize());
#endif // UE_ENABLE_TOKENSTREAM_STACKOVERFLOW_CHECKS
		return ++CurrentFrame;
	}

	/** Pops back to the previous frame on the stack */
	FORCEINLINE FStackEntry* Pop()
	{
#if UE_ENABLE_TOKENSTREAM_STACKOVERFLOW_CHECKS
		NumberOfUsedStackFrames--;
		UE_CLOG(NumberOfUsedStackFrames < 1, LogGarbage, Fatal, TEXT("GC token stream stack Pop() was called too many times. Probably a Push() call is missing for one of the tokens."));
#endif // UE_ENABLE_TOKENSTREAM_STACKOVERFLOW_CHECKS
		return --CurrentFrame;
	}
};

//////////////////////////////////////////////////////////////////////////

/** Forwards references directly to ProcessorType::HandleTokenStreamObjectReference(), unlike TBatchDispatcher */
template<class ProcessorType, class ContextType>
struct TDirectDispatcher
{
	ProcessorType& Processor;
	ContextType& Context;

	FORCEINLINE void HandleReferenceDirectly(UObject* ReferencingObject, UObject*& Object, FTokenId TokenId, EGCTokenType TokenType, bool bAllowReferenceElimination) const
	{
		if (IsObjectHandleResolved(*reinterpret_cast<FObjectHandle*>(&Object)))
		{
			Processor.HandleTokenStreamObjectReference(Context, ReferencingObject, Object, TokenId, TokenType, bAllowReferenceElimination);
		}
	}
	
	FORCEINLINE void HandleKillableReference(UObject*& Object, FTokenId TokenId, EGCTokenType TokenType) const
	{
		HandleReferenceDirectly(Context.GetReferencingObject(), Object, TokenId, TokenType, true);
	}

	FORCEINLINE void HandleImmutableReference(UObject* Object, FTokenId TokenId, EGCTokenType TokenType) const
	{
		HandleReferenceDirectly(Context.GetReferencingObject(), Object, TokenId, TokenType, false);
	}

	FORCEINLINE void HandleKillableArray(TArray<UObject*>& Array, FTokenId TokenId, EGCTokenType TokenType) const
	{
		for (UObject*& Object : Array)
		{
			HandleReferenceDirectly(Context.GetReferencingObject(), Object, TokenId, TokenType, true);
		}
	}

	FORCEINLINE void HandleKillableReferences(TArrayView<UObject*> Objects, FTokenId TokenId, EGCTokenType TokenType) const
	{
		for (UObject*& Object : Objects)
		{
			HandleReferenceDirectly(Context.GetReferencingObject(), Object, TokenId, TokenType, true);
		}
	}

	FORCEINLINE void HandleWeakReference(FWeakObjectPtr& WeakPtr, UObject* ReferencingObject, FTokenId TokenId, EGCTokenType TokenType) const
	{
		UObject* WeakObject = WeakPtr.Get(true);
		HandleReferenceDirectly(ReferencingObject, WeakObject, TokenId, TokenType, false);
	}

	FORCEINLINE void FlushQueuedReferences() const {}
};

// Default implementation is to create new direct dispatcher
template<class CollectorType, class ProcessorType, class ContextType>
TDirectDispatcher<ProcessorType, ContextType> GetDispatcher(CollectorType&, ProcessorType& Processor, ContextType& Context)
{
	return { Processor, Context };
}

//////////////////////////////////////////////////////////////////////////

enum class ELoot { Nothing, Block, ARO, Context };
COREUOBJECT_API ELoot StealWork(FWorkerContext& Context, FReferenceCollector& Collector, FWorkBlock*& OutBlock);

/** Allocates contexts and coordinator, kicks worker tasks that also call ProcessSync. Processor is type-erased to void* to avoid templated code. */
COREUOBJECT_API void ProcessAsync(void (*ProcessSync)(void*, FWorkerContext&), void* Processor, FWorkerContext& InitialContext);

//////////////////////////////////////////////////////////////////////////

/**
 * Helper class that looks for UObject references by traversing UClass token stream and calls AddReferencedObjects.
 * Provides a generic way of processing references that is used by Unreal Engine garbage collection.
 * Can be used for fast (does not use serialization) reference collection purposes.
 * 
 * IT IS CRITICAL THIS CLASS DOES NOT CHANGE WITHOUT CONSIDERING PERFORMANCE IMPACT OF SAID CHANGES
 *
 * @see FSimpleReferenceProcessorBase and TDefaultCollector for documentation on required APIs
 */
template <typename ProcessorType, typename CollectorType>
class TFastReferenceCollector : public FGCInternals
{
	static constexpr EGCOptions Options = ProcessorType::Options;

	static constexpr FORCEINLINE bool IsParallel()					{ return !!(Options & EGCOptions::Parallel); }
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	static constexpr FORCEINLINE bool ShouldProcessWeakReferences()	{ return !!(Options & EGCOptions::ProcessWeakReferences); }
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	static FORCEINLINE FTokenId MakeTokenId(uint32 Index)
	{
		return FTokenId(Index, ShouldProcessWeakReferences());
	}

	static FORCEINLINE UE::GC::FTokenStreamView& GetTokenStream(UClass* Class)
	{
		if (!!(Options & EGCOptions::AutogenerateTokenStream) && !Class->HasAnyClassFlags(CLASS_TokenStreamAssembled))
		{			
			Class->AssembleReferenceTokenStream();
		}

		return ShouldProcessWeakReferences() ? Class->ReferenceTokens.Mixed : Class->ReferenceTokens.Strong;
	}

	ProcessorType& Processor;

public:
	TFastReferenceCollector(ProcessorType& InProcessor) : Processor(InProcessor) {}

	void ProcessObjectArray(FWorkerContext& Context)
	{
		static_assert(!EnumHasAllFlags(Options, EGCOptions::Parallel | EGCOptions::AutogenerateTokenStream), "Can't assemble token streams in parallel");

		// Presized "recursion" stack for handling arrays and structs.
		FTokenStreamStack Stack;
		
		CollectorType Collector(Processor, Context);

		// Either TDirectDispatcher living on the stack or TBatchDispatcher reference owned by Collector
		decltype(GetDispatcher(Collector, Processor, Context)) Dispatcher = GetDispatcher(Collector, Processor, Context);

StoleContext:
		// Process initial references first
		Context.ReferencingObject = FGCObject::GGCObjectReferencer;
		for (UObject** InitialReference : Context.InitialNativeReferences)
		{
			Dispatcher.HandleKillableReference(*InitialReference, ETokenlessId::InitialReference, EGCTokenType::Native);
		}

		TConstArrayView<UObject*> CurrentObjects = Context.InitialObjects;
		while (true)
		{
			for (FPrefetchingObjectIterator It(CurrentObjects); It.HasMore(); It.Advance())
			{
				UObject* CurrentObject = It.GetCurrentObject();
				UClass* Class = CurrentObject->GetClass();
				UObject* Outer = CurrentObject->GetOuter();

				FTokenStreamView TokenStream = GetTokenStream(Class);
				Context.ReferencingObject = CurrentObject;

				// Emit base references
				Dispatcher.HandleImmutableReference(Class, ETokenlessId::Class, EGCTokenType::Native);
				Dispatcher.HandleImmutableReference(Outer, ETokenlessId::Outer, EGCTokenType::Native);

				if (TokenStream.IsEmpty())
				{
					continue;
				}

				uint32 TokenStreamIndex = 0;
				// Keep track of index to reference info. Used to avoid LHSs.
				uint32 ReferenceTokenStreamIndex = 0;

				// Create stack entry and initialize sane values.
				FStackEntry* StackEntry = Stack.Initialize();
				uint8* StackEntryData = (uint8*)CurrentObject;
				StackEntry->Data = StackEntryData;
				StackEntry->ContainerType = GCRT_None;
				StackEntry->Stride = 0;
				StackEntry->Count = -1;
				StackEntry->LoopStartIndex = -1;
				
				// Keep track of token return count in separate integer as arrays need to fiddle with it.
				uint8 TokenReturnCount = 0;

				// Parse the token stream.
				while (true)
				{
					// Cache current token index as it is the one pointing to the reference info.
					ReferenceTokenStreamIndex = TokenStreamIndex;

					// Handle returning from an array of structs, array of structs of arrays of ... (yadda yadda)
					for (uint8 ReturnCount = 0; ReturnCount < TokenReturnCount; ReturnCount++)
					{
						// Make sure there's no stack underflow.
						check(StackEntry->Count != -1);

						// We pre-decrement as we're already through the loop once at this point.
						if (--StackEntry->Count > 0)
						{
							if (StackEntry->ContainerType == GCRT_None)
							{
								// Fast path for TArrays of structs
								// Point data to next entry.
								StackEntryData = StackEntry->Data + StackEntry->Stride;
								StackEntry->Data = StackEntryData;
							}
							else
							{
								// Slower path for other containers
								// Point data to next valid entry.
								do
								{
									StackEntryData = StackEntry->Data + StackEntry->Stride;
									StackEntry->Data = StackEntryData;
								} while (!StackEntry->MoveToNextContainerElementAndCheckIfValid());
							}

							// Jump back to the beginning of the loop.
							TokenStreamIndex = StackEntry->LoopStartIndex;
							ReferenceTokenStreamIndex = StackEntry->LoopStartIndex;
							// We're not done with this token loop so we need to early out instead of backing out further.
							break;
						}
						else
						{
							StackEntry->ContainerType = GCRT_None;
							StackEntry = Stack.Pop();
							StackEntryData = StackEntry->Data;
						}
					}

					TokenStreamIndex++;

					struct FUnpackedReferenceInfo
					{
						explicit FUnpackedReferenceInfo(FGCReferenceInfo In)
						: ReturnCount(In.ReturnCount)
						, Type(static_cast<EGCReferenceType>(In.Type))
						, Offset(In.Offset)
						{}

						uint8 ReturnCount;
						EGCReferenceType Type;
						uint32 Offset;
					};
					const FUnpackedReferenceInfo ReferenceInfo(TokenStream.AccessReferenceInfo(ReferenceTokenStreamIndex));
							
					const EGCTokenType TokenType = TokenStream.GetTokenType();

					switch (ReferenceInfo.Type)
					{
						case GCRT_Object:
						{								
							UObject** ObjectPtr = (UObject**)(StackEntryData + ReferenceInfo.Offset);
							TokenReturnCount = ReferenceInfo.ReturnCount;
							Dispatcher.HandleKillableReference(*ObjectPtr, MakeTokenId(ReferenceTokenStreamIndex), TokenType);
						}
						break;
						case GCRT_ArrayObject:
						{
							TArray<UObject*>& ObjectArray = *((TArray<UObject*>*)(StackEntryData + ReferenceInfo.Offset));
							TokenReturnCount = ReferenceInfo.ReturnCount;
							Dispatcher.HandleKillableArray(ObjectArray, MakeTokenId(ReferenceTokenStreamIndex), TokenType);
						}
						break;
						case GCRT_ArrayObjectFreezable:
						{
							TArray<UObject*, FMemoryImageAllocator>& ObjectArray = *((TArray<UObject*, FMemoryImageAllocator>*)(StackEntryData + ReferenceInfo.Offset));
							TokenReturnCount = ReferenceInfo.ReturnCount;
							Dispatcher.HandleKillableReferences(ObjectArray, MakeTokenId(ReferenceTokenStreamIndex), TokenType);
						}
						break;
						case GCRT_ArrayStruct:
						{
							const FScriptArray& Array = *((FScriptArray*)(StackEntryData + ReferenceInfo.Offset));
							StackEntry = Stack.Push();
							StackEntryData = (uint8*)Array.GetData();
							StackEntry->Data = StackEntryData;
							StackEntry->Stride = TokenStream.ReadStride(TokenStreamIndex);
							StackEntry->Count = Array.Num();
							StackEntry->ContainerType = GCRT_None;

							const FGCSkipInfo SkipInfo = TokenStream.ReadSkipInfo(TokenStreamIndex);
							StackEntry->LoopStartIndex = TokenStreamIndex;

							if (StackEntry->Count == 0)
							{
								// Skip empty array by jumping to skip index and set return count to the one about to be read in.
								TokenStreamIndex = SkipInfo.SkipIndex;
								TokenReturnCount = TokenStream.GetSkipReturnCount(SkipInfo);
							}
							else
							{
								// Loop again.
								check(StackEntry->Data);
								TokenReturnCount = 0;
							}
						}
						break;
						case GCRT_ArrayStructFreezable:
						{
							const FFreezableScriptArray& Array = *((FFreezableScriptArray*)(StackEntryData + ReferenceInfo.Offset));
							StackEntry = Stack.Push();
							StackEntryData = (uint8*)Array.GetData();
							StackEntry->Data = StackEntryData;
							StackEntry->Stride = TokenStream.ReadStride(TokenStreamIndex);
							StackEntry->Count = Array.Num();
							StackEntry->ContainerType = GCRT_None;

							const FGCSkipInfo SkipInfo = TokenStream.ReadSkipInfo(TokenStreamIndex);
							StackEntry->LoopStartIndex = TokenStreamIndex;

							if (StackEntry->Count == 0)
							{
								// Skip empty array by jumping to skip index and set return count to the one about to be read in.
								TokenStreamIndex = SkipInfo.SkipIndex;
								TokenReturnCount = TokenStream.GetSkipReturnCount(SkipInfo);
							}
							else
							{
								// Loop again.
								check(StackEntry->Data);
								TokenReturnCount = 0;
							}
						}
						break;
						case GCRT_AddReferencedObjects:
						{
							void(*AddReferencedObjects)(UObject*, FReferenceCollector&) = (void(*)(UObject*, FReferenceCollector&))TokenStream.ReadPointer(TokenStreamIndex);
							TokenReturnCount = ReferenceInfo.ReturnCount;
							AddReferencedObjects(CurrentObject, Collector);

							// ARO is always last and an implicit terminator
							goto TokensDone;
						}
						break;
						case GCRT_EndOfStream:
						{
							// Break out of loop
							goto TokensDone;
						}
						break;
						case GCRT_SlowAddReferencedObjects:
						{
							if (!IsParallel() ||
								!FSlowARO::TryQueueCall(/* ARO index */ ReferenceInfo.Offset, CurrentObject, Context))
							{
								FSlowARO::CallSync(/* ARO index */ ReferenceInfo.Offset, CurrentObject, Collector);
							}
								
							// ARO is always last and an implicit terminator
							goto TokensDone;
						}
						case GCRT_ExternalPackage:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							// Test if the object isn't itself, since currently package are their own external and tracking that reference is pointless
							UObject* Object = CurrentObject->GetExternalPackageInternal();
							Object = Object != CurrentObject ? Object : nullptr;
							Dispatcher.HandleImmutableReference( Object, MakeTokenId(ReferenceTokenStreamIndex), TokenType);
						}
						break;
						case GCRT_FixedArray:
						{
							uint8* PreviousData = StackEntryData;
							StackEntry = Stack.Push();
							StackEntryData = PreviousData;
							StackEntry->Data = PreviousData;
							StackEntry->Stride = TokenStream.ReadStride(TokenStreamIndex);
							StackEntry->Count = TokenStream.ReadCount(TokenStreamIndex);
							StackEntry->LoopStartIndex = TokenStreamIndex;
							StackEntry->ContainerType = GCRT_None;
							TokenReturnCount = 0;
						}
						break;
						case GCRT_AddStructReferencedObjects:
						{
							void* StructPtr = (void*)(StackEntryData + ReferenceInfo.Offset);
							TokenReturnCount = ReferenceInfo.ReturnCount;
							UScriptStruct::ICppStructOps::TPointerToAddStructReferencedObjects Func = (UScriptStruct::ICppStructOps::TPointerToAddStructReferencedObjects)TokenStream.ReadPointer(TokenStreamIndex);
							Func(StructPtr, Collector);
						}
						break;
						case GCRT_AddTMapReferencedObjects:
						{
							void* MapPtr = StackEntryData + ReferenceInfo.Offset;
							FMapProperty* MapProperty = (FMapProperty*)TokenStream.ReadPointer(TokenStreamIndex);
							TokenStreamIndex++; // GCRT_EndOfPointer

							StackEntry = Stack.Push();
							StackEntry->ContainerType = GCRT_AddTMapReferencedObjects;
							StackEntry->ContainerIndex = 0;
							StackEntry->ContainerProperty = MapProperty;
							StackEntry->ContainerPtr = MapPtr;
							StackEntry->Stride = MapProperty->GetPairStride();
							StackEntry->Count = MapProperty->GetNum(MapPtr);

							const FGCSkipInfo SkipInfo = TokenStream.ReadSkipInfo(TokenStreamIndex);
							StackEntry->LoopStartIndex = TokenStreamIndex;

							if (StackEntry->Count == 0)
							{
								// The map is empty
								StackEntryData = nullptr;
								StackEntry->Data = StackEntryData;

								// Skip empty map by jumping to skip index and set return count to the one about to be read in.
								TokenStreamIndex = SkipInfo.SkipIndex;
								TokenReturnCount = TokenStream.GetSkipReturnCount(SkipInfo);
							}
							else
							{
								// Skip any initial invalid entries in the map. We need a valid index for MapProperty->GetPairPtr()
								int32 FirstValidIndex = 0;
								while (!MapProperty->IsValidIndex(MapPtr, FirstValidIndex))
								{
									FirstValidIndex++;
								}

								StackEntry->ContainerIndex = FirstValidIndex;
								StackEntryData = MapProperty->GetPairPtr(MapPtr, FirstValidIndex);
								StackEntry->Data = StackEntryData;

								// Loop again.
								TokenReturnCount = 0;
							}
						}
						break;
						case GCRT_AddTSetReferencedObjects:
						{
							void* SetPtr = StackEntryData + ReferenceInfo.Offset;
							FSetProperty* SetProperty = (FSetProperty*)TokenStream.ReadPointer(TokenStreamIndex);
							TokenStreamIndex++; // GCRT_EndOfPointer

							StackEntry = Stack.Push();
							StackEntry->ContainerProperty = SetProperty;
							StackEntry->ContainerPtr = SetPtr;
							StackEntry->ContainerType = GCRT_AddTSetReferencedObjects;
							StackEntry->ContainerIndex = 0;
							StackEntry->Stride = SetProperty->GetStride();
							StackEntry->Count = SetProperty->GetNum(SetPtr);

							const FGCSkipInfo SkipInfo = TokenStream.ReadSkipInfo(TokenStreamIndex);
							StackEntry->LoopStartIndex = TokenStreamIndex;

							if (StackEntry->Count == 0)
							{
								// The set is empty or it doesn't contain any valid elements
								StackEntryData = nullptr;
								StackEntry->Data = StackEntryData;

								// Skip empty set by jumping to skip index and set return count to the one about to be read in.
								TokenStreamIndex = SkipInfo.SkipIndex;
								TokenReturnCount = TokenStream.GetSkipReturnCount(SkipInfo);
							}
							else
							{
								// Skip any initial invalid entries in the set. We need a valid index for SetProperty->GetElementPtr()
								int32 FirstValidIndex = 0;
								while (!SetProperty->IsValidIndex(SetPtr, FirstValidIndex))
								{
									FirstValidIndex++;
								}

								StackEntry->ContainerIndex = FirstValidIndex;
								StackEntryData = SetProperty->GetElementPtr(SetPtr, FirstValidIndex);
								StackEntry->Data = StackEntryData;

								// Loop again.
								TokenReturnCount = 0;
							}
						}
						break;
						case GCRT_AddFieldPathReferencedObject:
						{
							FFieldPath* FieldPathPtr = (FFieldPath*)(StackEntryData + ReferenceInfo.Offset);
							FUObjectItem* FieldOwnerItem = GetResolvedOwner(*FieldPathPtr);
							TokenReturnCount = ReferenceInfo.ReturnCount;
							if (FieldOwnerItem)
							{
								UObject* OwnerObject = static_cast<UObject*>(FieldOwnerItem->Object);
								UObject* PreviousOwner = OwnerObject;
								Dispatcher.HandleReferenceDirectly(CurrentObject, OwnerObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType, true);
								
								// Handle reference elimination (PendingKill owner)
								if (PreviousOwner && !OwnerObject)
								{
									ClearCachedField(*FieldPathPtr);
								}
							}
						}
						break;
						case GCRT_ArrayAddFieldPathReferencedObject:
						{
							TArray<FFieldPath>& FieldArray = *((TArray<FFieldPath>*)(StackEntryData + ReferenceInfo.Offset));
							TokenReturnCount = ReferenceInfo.ReturnCount;
							for (int32 FieldIndex = 0, FieldNum = FieldArray.Num(); FieldIndex < FieldNum; ++FieldIndex)
							{
								if (FUObjectItem* FieldOwnerItem = GetResolvedOwner(FieldArray[FieldIndex]))
								{
									UObject* OwnerObject = static_cast<UObject*>(FieldOwnerItem->Object);
									UObject* PreviousOwner = OwnerObject;
									Dispatcher.HandleReferenceDirectly(CurrentObject, OwnerObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType, true);

									// Handle reference elimination (PendingKill owner)
									if (PreviousOwner && !OwnerObject)
									{
										ClearCachedField(FieldArray[FieldIndex]);
									}
								}
							}
						}
						break;
						case GCRT_Optional:
						{
							uint32 ValueSize = TokenStream.ReadStride(TokenStreamIndex); // Size of value in bytes. This is also the offset to the bIsSet variable stored thereafter.
							const FGCSkipInfo SkipInfo = TokenStream.ReadSkipInfo(TokenStreamIndex);
							const bool& bIsSet = *((bool*)(StackEntryData + ReferenceInfo.Offset + ValueSize));
							if (bIsSet)
							{
								// It's set - push a stack entry for processing the value
								// This is somewhat suboptimal since there is only ever just one value, but this approach avoids any changes to the surrounding code
								StackEntry = Stack.Push();
								StackEntryData += ReferenceInfo.Offset;
								StackEntry->Data = StackEntryData;
								StackEntry->Stride = ValueSize;
								StackEntry->Count = 1;
								StackEntry->LoopStartIndex = TokenStreamIndex;
							}
							else
							{
								// It's unset - keep going by jumping to skip index
								TokenStreamIndex = SkipInfo.SkipIndex;
							}
							TokenReturnCount = 0;
						}
						break;
						case GCRT_EndOfPointer:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
						}
						break;
						case GCRT_WeakObject:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							if (ShouldProcessWeakReferences())
							{
								FWeakObjectPtr& WeakPtr = *(FWeakObjectPtr*)(StackEntryData + ReferenceInfo.Offset);
								Dispatcher.HandleWeakReference(WeakPtr, CurrentObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType);
							}
						}
						break;
						case GCRT_ArrayWeakObject:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							if (ShouldProcessWeakReferences())
							{
								TArray<FWeakObjectPtr>& WeakPtrArray = *((TArray<FWeakObjectPtr>*)(StackEntryData + ReferenceInfo.Offset));
								for (FWeakObjectPtr& WeakPtr : WeakPtrArray)
								{
									Dispatcher.HandleWeakReference(WeakPtr, CurrentObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType);
								}
							}
						}
						break;
						case GCRT_LazyObject:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							if (ShouldProcessWeakReferences())
							{
								FLazyObjectPtr& LazyPtr = *(FLazyObjectPtr*)(StackEntryData + ReferenceInfo.Offset);
								FWeakObjectPtr& WeakPtr = AccessWeakPtr(LazyPtr);
								Dispatcher.HandleWeakReference(WeakPtr, CurrentObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType);
							}
						}
						break;
						case GCRT_ArrayLazyObject:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							if (ShouldProcessWeakReferences())
							{
								TArray<FLazyObjectPtr>& LazyPtrArray = *((TArray<FLazyObjectPtr>*)(StackEntryData + ReferenceInfo.Offset));
								TokenReturnCount = ReferenceInfo.ReturnCount;
								for (FLazyObjectPtr& LazyPtr : LazyPtrArray)
								{
									FWeakObjectPtr& WeakPtr = AccessWeakPtr(LazyPtr);
									Dispatcher.HandleWeakReference(WeakPtr, CurrentObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType);
								}
							}
						}
						break;
						case GCRT_SoftObject:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							if (ShouldProcessWeakReferences())
							{
								FSoftObjectPtr& SoftPtr = *(FSoftObjectPtr*)(StackEntryData + ReferenceInfo.Offset);
								FWeakObjectPtr& WeakPtr = AccessWeakPtr(SoftPtr);
								Dispatcher.HandleWeakReference(WeakPtr, CurrentObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType);
							}
						}
						break;
						case GCRT_ArraySoftObject:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							if (ShouldProcessWeakReferences())
							{
								TArray<FSoftObjectPtr>& SoftPtrArray = *((TArray<FSoftObjectPtr>*)(StackEntryData + ReferenceInfo.Offset));
								for (FSoftObjectPtr& SoftPtr : SoftPtrArray)
								{
									FWeakObjectPtr& WeakPtr = AccessWeakPtr(SoftPtr);
									Dispatcher.HandleWeakReference(WeakPtr, CurrentObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType);
								}
							}
						}
						break;
						case GCRT_Delegate:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							if (ShouldProcessWeakReferences())
							{
								FScriptDelegate& Delegate = *(FScriptDelegate*)(StackEntryData + ReferenceInfo.Offset);
								UObject* DelegateObject = Delegate.GetUObject();
								Dispatcher.HandleReferenceDirectly(CurrentObject, DelegateObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType, false);
							}
						}
						break;
						case GCRT_ArrayDelegate:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							if (ShouldProcessWeakReferences())
							{
								TArray<FScriptDelegate>& DelegateArray = *((TArray<FScriptDelegate>*)(StackEntryData + ReferenceInfo.Offset));
								for (FScriptDelegate& Delegate : DelegateArray)
								{
									UObject* DelegateObject = Delegate.GetUObject();
									Dispatcher.HandleReferenceDirectly(CurrentObject, DelegateObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType, false);
								}
							}
						}
						break;
						case GCRT_MulticastDelegate:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							if (ShouldProcessWeakReferences())
							{
								FMulticastScriptDelegate& Delegate = *(FMulticastScriptDelegate*)(StackEntryData + ReferenceInfo.Offset);
								TArray<UObject*> DelegateObjects(Delegate.GetAllObjects());
								for (UObject* DelegateObject : DelegateObjects)
								{
									Dispatcher.HandleReferenceDirectly(CurrentObject, DelegateObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType, false);
								}
							}
						}
						break;
						case GCRT_ArrayMulticastDelegate:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							if (ShouldProcessWeakReferences())
							{
								TArray<FMulticastScriptDelegate>& DelegateArray = *((TArray<FMulticastScriptDelegate>*)(StackEntryData + ReferenceInfo.Offset));
								for (FMulticastScriptDelegate& Delegate : DelegateArray)
								{
									TArray<UObject*> DelegateObjects(Delegate.GetAllObjects());
									for (UObject* DelegateObject : DelegateObjects)
									{
										Dispatcher.HandleReferenceDirectly(CurrentObject, DelegateObject, MakeTokenId(ReferenceTokenStreamIndex), TokenType, false);
									}
								}
							}
						}
						break;
						case GCRT_DynamicallyTypedValue:
						{
							TokenReturnCount = ReferenceInfo.ReturnCount;
							UE::FDynamicallyTypedValue* DynamicallyTypedValue = (UE::FDynamicallyTypedValue*)(StackEntryData + ReferenceInfo.Offset);
							DynamicallyTypedValue->GetType().MarkReachable(Collector);
							if (DynamicallyTypedValue->GetType().GetContainsReferences() != UE::FDynamicallyTypedValueType::EContainsReferences::DoesNot)
							{
								DynamicallyTypedValue->GetType().MarkValueReachable(DynamicallyTypedValue->GetDataPointer(), Collector);
							}
						}
						break;
						default:
						{
							UE_LOG(LogGarbage, Fatal, TEXT("Unknown token. Type:%d ReferenceTokenStreamIndex:%d Class:%s Obj:%s"), ReferenceInfo.Type, ReferenceTokenStreamIndex, CurrentObject ? *GetNameSafe(CurrentObject->GetClass()) : TEXT("Unknown"), *GetPathNameSafe(CurrentObject));
							break;
						}
					} // switch (Type)
				} // while (true)
TokensDone:
				check(StackEntry == Stack.GetBottomFrame());
			} // for (FPrefetchingObjectIterator)

			// Free finished work block
			if (CurrentObjects.GetData() != Context.InitialObjects.GetData())
			{
				Context.ObjectsToSerialize.FreeOwningBlock(CurrentObjects.GetData());
			}

			int32 BlockSize = FWorkBlock::ObjectCapacity;
			FWorkBlockifier& RemainingObjects = Context.ObjectsToSerialize;
			FWorkBlock* Block = RemainingObjects.PopFullBlock<Options>();
			if (!Block)
			{
				if constexpr (IsParallel())
				{
					FSlowARO::ProcessUnbalancedCalls(Context, Collector);
				}

StoleARO:		
				Dispatcher.FlushQueuedReferences();

				if (	 Block = RemainingObjects.PopFullBlock<Options>(); Block);
				else if (Block = RemainingObjects.PopPartialBlock(/* out if successful */ BlockSize); Block);
				else if (IsParallel()) // if constexpr yields MSVC unreferenced label warning
				{
					switch (StealWork(/* in-out */ Context, Collector, /* out */ Block))
					{
						case ELoot::Nothing:	break;				// Done, stop working
						case ELoot::Block:		break;				// Stole full block, process it
						case ELoot::ARO:		goto StoleARO;		// Stole and made ARO calls that feed into Dispatcher queues and RemainingObjects
						case ELoot::Context:	goto StoleContext;	// Stole initial references and initial objects worker that hasn't started working
					}
				}

				if (!Block)
				{
					break;
				}
			}

			CurrentObjects = MakeArrayView(Block->Objects, BlockSize);
		} // while (true)
		
	#if PERF_DETAILED_PER_CLASS_GC_STATS
		// Detailed per class stats should not be performed when parallel GC is running
		check(!IsParallel());
		Processor.LogDetailedStatsSummary();
	#endif
	}
};

//////////////////////////////////////////////////////////////////////////

/** Default reference collector for CollectReferences() */
template <typename ProcessorType>
class TDefaultCollector : public FReferenceCollector
{
protected:
	ProcessorType& Processor;
	UE::GC::FWorkerContext& Context;

public:
	TDefaultCollector(ProcessorType& InProcessor, FGCArrayStruct& InContext)
	: Processor(InProcessor)
	, Context(InContext)
	{}

	virtual void HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const FProperty* ReferencingProperty) override
	{
		Processor.HandleTokenStreamObjectReference(Context, const_cast<UObject*>(ReferencingObject), Object, ETokenlessId::Collector, EGCTokenType::Native, false);
	}
	virtual void HandleObjectReferences(UObject** InObjects, const int32 ObjectNum, const UObject* ReferencingObject, const FProperty* InReferencingProperty) override
	{
		for (int32 ObjectIndex = 0; ObjectIndex < ObjectNum; ++ObjectIndex)
		{
			UObject*& Object = InObjects[ObjectIndex];
			Processor.HandleTokenStreamObjectReference(Context, const_cast<UObject*>(ReferencingObject), Object, ETokenlessId::Collector, EGCTokenType::Native, false);
		}
	}

	virtual bool IsIgnoringArchetypeRef() const override { return false;}
	virtual bool IsIgnoringTransient() const override {	return false; }
};

inline constexpr EGCOptions DefaultOptions = EGCOptions::AutogenerateTokenStream;

} // namespace UE::GC

/** Simple single-threaded reference processor base class for use with CollectReferences() */
class FSimpleReferenceProcessorBase
{
public:
	static constexpr EGCOptions Options = UE::GC::DefaultOptions;
	void UpdateDetailedStats(UObject* CurrentObject, uint32 DeltaCycles) {}
	void LogDetailedStatsSummary() {}

	// Implement this in your derived class, don't make this virtual as it will affect performance!
	//FORCEINLINE void HandleTokenStreamObjectReference(UE::GC::FWorkerContext& Context, UObject* ReferencingObject, UObject*& Object, UE::GC::FTokenId TokenIndex, EGCTokenType TokenType, bool bAllowReferenceElimination)
};


template<class CollectorType, class ProcessorType>
FORCEINLINE static void CollectReferences(ProcessorType& Processor, UE::GC::FWorkerContext& Context)
{
	using namespace UE::GC;
	using FastReferenceCollector = TFastReferenceCollector<ProcessorType, CollectorType>;
	
	if constexpr (IsParallel(ProcessorType::Options))
	{
		ProcessAsync([](void* P, FWorkerContext& C) { FastReferenceCollector(*reinterpret_cast<ProcessorType*>(P)).ProcessObjectArray(C); }, &Processor, Context);
	}
	else
	{
		FastReferenceCollector(Processor).ProcessObjectArray(Context);
	}
}

template<class ProcessorType>
FORCEINLINE static void CollectReferences(ProcessorType& Processor, UE::GC::FWorkerContext& Context)
{
	CollectReferences<UE::GC::TDefaultCollector<ProcessorType>, ProcessorType>(Processor, Context);
}

// Get number of workers to use when calling CollectReferences in parallel
COREUOBJECT_API int32 GetNumCollectReferenceWorkers();