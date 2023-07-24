// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateElementIndexBuffer.h"
#include "SlateGlobals.h"
#include "Rendering/RenderingCommon.h"
#include "RHI.h"

DECLARE_MEMORY_STAT(TEXT("Index Buffer Memory (GPU)"), STAT_SlateIndexBufferMemory, STATGROUP_SlateMemory);

FSlateElementIndexBuffer::FSlateElementIndexBuffer()
	: BufferSize(0)	 
	, MinBufferSize(0)
	, BufferUsageSize(0)
{
}

FSlateElementIndexBuffer::~FSlateElementIndexBuffer()
{
}

void FSlateElementIndexBuffer::Init( int32 MinNumIndices )
{
	MinBufferSize = sizeof(SlateIndex) * FMath::Max( MinNumIndices, 100 );

	if ( IsInRenderingThread() )
	{
		InitResource();
	}
	else
	{
		BeginInitResource(this);
	}
}

void FSlateElementIndexBuffer::Destroy()
{
	if ( IsInRenderingThread() )
	{
		ReleaseResource();
	}
	else
	{
		BeginReleaseResource(this);
	}
}

/** Initializes the index buffers RHI resource. */
void FSlateElementIndexBuffer::InitDynamicRHI()
{
	checkSlow( IsInRenderingThread() );

	check( MinBufferSize > 0 );

	SetBufferSize(MinBufferSize);

	FRHIResourceCreateInfo CreateInfo(TEXT("FSlateElementIndexBuffer"));
	IndexBufferRHI = RHICreateIndexBuffer( sizeof(SlateIndex), MinBufferSize, BUF_Dynamic, CreateInfo );
	check( IsValidRef(IndexBufferRHI) );
}

/** Resizes the buffer to the passed in size.  Preserves internal data */
void FSlateElementIndexBuffer::ResizeBuffer( int32 NewSizeBytes )
{
	checkSlow( IsInRenderingThread() );

	int32 FinalSize = FMath::Max( NewSizeBytes, MinBufferSize );

	if( FinalSize != 0 && FinalSize != BufferSize )
	{
		IndexBufferRHI.SafeRelease();
		FRHIResourceCreateInfo CreateInfo(TEXT("FSlateElementIndexBuffer"));
		IndexBufferRHI = RHICreateIndexBuffer( sizeof(SlateIndex), FinalSize, BUF_Dynamic, CreateInfo );
		check(IsValidRef(IndexBufferRHI));

		SetBufferSize(FinalSize);
	}
}

void FSlateElementIndexBuffer::PreFillBuffer(int32 RequiredIndexCount, bool bShrinkToMinSize)
{
	SCOPE_CYCLE_COUNTER( STAT_SlatePreFullBufferRTTime );

	checkSlow(IsInRenderingThread());

	if (RequiredIndexCount > 0)
	{
		int32 RequiredBufferSize = RequiredIndexCount*sizeof(SlateIndex);

		// resize if needed
		if (RequiredBufferSize > GetBufferSize() || bShrinkToMinSize)
		{
			// Use array resize techniques for the vertex buffer
			ResizeBuffer(RequiredBufferSize);
		}

		BufferUsageSize = RequiredBufferSize;		
	}
}

/** Releases the index buffers RHI resource. */
void FSlateElementIndexBuffer::ReleaseDynamicRHI()
{
	IndexBufferRHI.SafeRelease();
	SetBufferSize(0);
}

void FSlateElementIndexBuffer::SetBufferSize(int32 NewBufferSize)
{
	DEC_MEMORY_STAT_BY(STAT_SlateIndexBufferMemory, BufferSize);
	BufferSize = NewBufferSize;
	INC_MEMORY_STAT_BY(STAT_SlateIndexBufferMemory, BufferSize);
}
