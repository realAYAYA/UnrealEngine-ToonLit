// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDOptionalDataChannel.h"
#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "HAL/ThreadSingleton.h"
#include "Templates/SharedPointer.h"

#if WITH_CHAOS_VISUAL_DEBUGGER

#include "ChaosVDRuntimeModule.h"

enum class EChaosVDContextType : int32
{
	Any,
	Solver,
	Query,
	SubTraceQuery,
};

enum class EChaosVDContextAttributes : int32
{
	None = 0,
	Resimulated = 1 << 0
};
ENUM_CLASS_FLAGS(EChaosVDContextAttributes)

/** Chaos Visual Debugger data used to context for logging or debugging purposes */
struct FChaosVDContext
{
	int32 OwnerID = INDEX_NONE;
	int32 Id = INDEX_NONE;
	int32 Type = INDEX_NONE;
	int32 Attributes = 0;

	FORCEINLINE void SetDataChannel(const TSharedRef<Chaos::VisualDebugger::FChaosVDOptionalDataChannel>& NewDataChannel)
	{
		CurrentDataChannel = NewDataChannel;
	}

	FORCEINLINE bool IsDataChannelEnabled() const { return CurrentDataChannel->IsChannelEnabled(); }

private:
	TSharedRef<Chaos::VisualDebugger::FChaosVDOptionalDataChannel> CurrentDataChannel = CVDDC_Default;
};

/** Singleton class that manages the thread local storage used to store CVD Context data */
class CHAOS_API FChaosVDThreadContext : public TThreadSingleton<FChaosVDThreadContext>
{
public:

	virtual ~FChaosVDThreadContext() override
	{
		if (RecordingStoppedHandle.IsValid())
		{
			FChaosVDRuntimeModule::RemoveRecordingStopCallback(RecordingStoppedHandle);
		}
	}

	/** Copies the Current CVD context data into the provided struct
	 * @return true if the copy was successful
	 */
	bool GetCurrentContext(FChaosVDContext& OutContext);
	
	/** Gets the current CVD context data -
	 * Don't use of a function that will recursively push new context data as it might invalidate the pointer
	 * @param Type type of the context we want to get
	 * @return Ptr to the Current CVD context data if it matches the provided type
	 */
	const FChaosVDContext* GetCurrentContext(EChaosVDContextType Type);
	
	/** Gets the current CVD context data -
	 * Don't use of a function that will recursively push new context data as it might invalidate the pointer
	 * @return Ptr to the Current CVD context data
	 */
	const FChaosVDContext* GetCurrentContext();
	
	/** Pushed a new CVD Context data to the local cvd context stack */
	void PushContext(const FChaosVDContext& InContext);
	
	/** Removes the CVD Context data at the top of the local cvd context stack */
	void PopContext();

protected:
	TArray<FChaosVDContext, TInlineAllocator<16>> LocalContextStack;

	TArray<uint8>& GetTLSDataBufferRef()
	{
		checkf(BufferAccessCounter == 0, TEXT("The CVD buffer is already in use!"));

		if (!RecordingStoppedHandle.IsValid())
		{
			// If we access the buffer, make sure we subscribe to the recording stop callback
			// So we free any memory we keep allocated
			RecordingStoppedHandle = FChaosVDRuntimeModule::RegisterRecordingStopCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateLambda([this](){ CVDDataBuffer.Empty(); }));
		}
		
		return CVDDataBuffer;
	}

	FDelegateHandle RecordingStoppedHandle;

	int32 BufferAccessCounter = 0;
	TArray<uint8> CVDDataBuffer;

	friend struct FChaosVDScopedTLSBufferAccessor;
};

namespace Chaos::VisualDebugger::Utils
{
	FORCEINLINE bool IsContextEnabledAndValid(const FChaosVDContext* Context)
	{
		if (!ensure(Context))
		{
			return false;
		}

		return Context->IsDataChannelEnabled();
	}
}

/** Utility Class that will push the provided CVD Context Data to the local thread storage
 * and remove it when it goes out of scope
 */
struct FChaosVDScopeContext
{
	FChaosVDScopeContext(const FChaosVDContext& InCVDContext)
	{
		FChaosVDThreadContext::Get().PushContext(InCVDContext);
	}

	~FChaosVDScopeContext()
	{
		FChaosVDThreadContext::Get().PopContext();
	}
};

struct FChaosCVDScopedDataChannelOverride
{
	FChaosCVDScopedDataChannelOverride(const TSharedRef<Chaos::VisualDebugger::FChaosVDOptionalDataChannel>& NewDataChannel)
	{
		if (const FChaosVDContext* CVDContextData = FChaosVDThreadContext::Get().GetCurrentContext())
		{
			FChaosVDContext NewContext = *CVDContextData;
			NewContext.SetDataChannel(NewDataChannel);

			FChaosVDThreadContext::Get().PushContext(MoveTemp(NewContext));
		}
	}

	~FChaosCVDScopedDataChannelOverride()
	{
		FChaosVDThreadContext::Get().PopContext();
	}
};

/** Wrapper class that provides access to the data buffer that should be used to trace CVD (Chaos Visual Debugger) data.
 * It ensures the buffer is cleared on scope exit but keeping a minimum amount of memory allocated
 */
struct FChaosVDScopedTLSBufferAccessor
{
	FChaosVDScopedTLSBufferAccessor() : BufferRef(FChaosVDThreadContext::Get().GetTLSDataBufferRef())
	{
		FChaosVDThreadContext::Get().BufferAccessCounter++;

		// This is close to the max amount of data we can Trace as a single trace event
		// Some times we will go over and some times this be more than we need
		// but keeping this amount allocated on average reduces the allocation cost when tracing.
		// TODO: There might be a better value for this, we need to do more testing
		constexpr uint32 MaxTraceChunkSize = TNumericLimits<uint16>::Max();
		BufferRef.Reserve(MaxTraceChunkSize);
	}

	~FChaosVDScopedTLSBufferAccessor()
	{
		// Clear the buffer on exit but maintain allocated a minimum amount of memory
		constexpr uint32 MaxTraceChunkSize = TNumericLimits<uint16>::Max();
		BufferRef.Reset(MaxTraceChunkSize);

		FChaosVDThreadContext::Get().BufferAccessCounter--;
	}
	
	TArray<uint8>& BufferRef;
};

#ifndef CVD_GET_CURRENT_CONTEXT
	#define CVD_GET_CURRENT_CONTEXT(OutContext) \
		FChaosVDThreadContext::Get().GetCurrentContext(OutContext);
#endif

#ifndef CVD_GET_WRAPPED_CURRENT_CONTEXT
	#define CVD_GET_WRAPPED_CURRENT_CONTEXT(OutWrappedContext) \
		FChaosVDContext CurrentContext; \
		FChaosVDThreadContext::Get().GetCurrentContext(CurrentContext); \
		OutWrappedContext = FChaosVDContextWrapper(CurrentContext);
#endif

#ifndef CVD_SCOPE_CONTEXT
	#define CVD_SCOPE_CONTEXT(InContext) \
		FChaosVDScopeContext CVDScope(InContext);
#endif

#ifndef CVD_SCOPED_DATA_CHANNEL_OVERRIDE
	#define CVD_SCOPED_DATA_CHANNEL_OVERRIDE(DataChannel) \
	FChaosCVDScopedDataChannelOverride CVDDC_Scope_Override##DataChannel(DataChannel);
#endif

#else // WITH_CHAOS_VISUAL_DEBUGGER

#ifndef CVD_GET_CURRENT_CONTEXT
	#define CVD_GET_CURRENT_CONTEXT(OutContext)
#endif

#ifndef CVD_SCOPE_CONTEXT
	#define CVD_SCOPE_CONTEXT(InContext)
#endif

#ifndef CVD_GET_WRAPPED_CURRENT_CONTEXT
	#define CVD_GET_WRAPPED_CURRENT_CONTEXT(OutWrappedContext)
#endif

#ifndef CVD_SCOPED_CONTEXT_DATA_CHANNEL_OVERRIDE
	#define CVD_SCOPED_CONTEXT_DATA_CHANNEL_OVERRIDE(DataChannel)
#endif

#endif // WITH_CHAOS_VISUAL_DEBUGGER

struct FChaosVDContextWrapper
{
	FChaosVDContextWrapper()
	{
	}
	
#if WITH_CHAOS_VISUAL_DEBUGGER
	FChaosVDContextWrapper(const FChaosVDContext& InContext)
	: Context(InContext)
	{
	}

	FChaosVDContext Context;
#endif
};
