// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHIRenderQuery.h: Metal RHI Render Query Definitions.
=============================================================================*/

#pragma once


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Private Query Buffer Forward Declarations


class FMetalContext;
class FMetalQueryBufferPool;
class FMetalQueryResult;
struct FMetalCommandBufferFence;


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Private Query Buffer Resource Class -


class FMetalQueryBuffer : public FRHIResource
{
public:
	FMetalQueryBuffer(FMetalContext* InContext, FMetalBuffer InBuffer);
	virtual ~FMetalQueryBuffer();

	uint64 GetResult(uint32 Offset);

	TWeakPtr<FMetalQueryBufferPool, ESPMode::ThreadSafe> Pool;
	FMetalBuffer Buffer;
	uint32 WriteOffset;
};


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Private Query Buffer Pool Class -


class FMetalQueryBufferPool
{
public:
	enum
	{
		EQueryBufferAlignment = 8,
		EQueryResultMaxSize   = 8,
		EQueryBufferMaxSize   = (1 << 16)
	};

	// Disallow a default constructor
	FMetalQueryBufferPool() = delete;

	FMetalQueryBufferPool(FMetalContext* InContext);
	~FMetalQueryBufferPool();

	void Allocate(FMetalQueryResult& NewQuery);
	FMetalQueryBuffer* GetCurrentQueryBuffer();
	void ReleaseCurrentQueryBuffer();
	void ReleaseQueryBuffer(FMetalBuffer& Buffer);

	TRefCountPtr<FMetalQueryBuffer> CurrentBuffer;
	TArray<FMetalBuffer> Buffers;
	FMetalContext* Context;
};


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Private Query Result Class -


class FMetalQueryResult
{
public:
	FMetalQueryResult() = default;
	~FMetalQueryResult() = default;

	bool Wait(uint64 Millis);
	uint64 GetResult();

	TRefCountPtr<FMetalQueryBuffer> SourceBuffer = nullptr;
	TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe> CommandBufferFence = nullptr;
	uint32 Offset = 0;
	bool bCompleted = false;
	bool bBatchFence = false;
};


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Render Query Class -


class FMetalRHIRenderQuery : public FRHIRenderQuery
{
public:
	FMetalRHIRenderQuery(ERenderQueryType InQueryType);
	virtual ~FMetalRHIRenderQuery();

	/**
	 * Kick off an occlusion test
	 */
	void Begin(FMetalContext* Context, TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe> const& BatchFence);

	/**
	 * Finish up an occlusion test
	 */
	void End(FMetalContext* Context);

	/**
	 * Get the query result
	 */
	bool GetResult(uint64& OutNumPixels, bool bWait, uint32 GPUIndex);

private:
	// The type of query
	ERenderQueryType Type;

	// Query buffer allocation details as the buffer is already set on the command-encoder
	FMetalQueryResult Buffer;

	// Query result.
	volatile uint64 Result;

	// Result availability - if not set the first call to acquire it will read the buffer & cache
	volatile bool bAvailable;

	// Timer event completion signal
	FEvent* QueryWrittenEvent;
};
