// Copyright Epic Games, Inc. All Rights Reserved.

#include "Textures/TextureAtlas.h"
#include "Experimental/ConcurrentLinearAllocator.h"
#include "Stats/Stats.h"
#include "Textures/SlateShaderResource.h"
#include "Textures/SlateTextureData.h"
#include "Trace/SlateMemoryTags.h"
#include "Misc/MemStack.h"

#include <limits>

DEFINE_STAT(STAT_SlateTextureGPUMemory);
DEFINE_STAT(STAT_SlateTextureDataMemory);
DECLARE_MEMORY_STAT(TEXT("Texture Atlas Memory (CPU)"), STAT_SlateTextureAtlasMemory, STATGROUP_SlateMemory);


ESlateTextureAtlasThreadId GetCurrentSlateTextureAtlasThreadId()
{
	// Note: For Game thread ownership, there is a point at which multiple worker threads operate on text simultaneously while the game thread is blocked
	// Access to the font cache is controlled through mutexes so we simply need to check that we are not accessing it from the render thread
	// Game thread access is also allowed when the game thread and render thread are the same
	if (!IsInActualRenderingThread())
	{
		return ESlateTextureAtlasThreadId::Game;
	}
	else if (IsInRenderingThread())
	{
		return ESlateTextureAtlasThreadId::Render;
	}

	return ESlateTextureAtlasThreadId::Unknown;
}

/* FSlateTextureAtlas helper class
 *****************************************************************************/

FSlateTextureAtlas::~FSlateTextureAtlas()
{
	EmptyAtlasData();
}


/* FSlateTextureAtlas interface
 *****************************************************************************/

void FSlateTextureAtlas::EmptyAtlasData()
{
	// Remove all nodes
	TArray<FAtlasedTextureSlot*, FConcurrentLinearArrayAllocator> DeleteSlots;

	for (FAtlasedTextureSlot::TIterator SlotIt(AtlasUsedSlots); SlotIt; SlotIt.Next())
	{
		FAtlasedTextureSlot& CurSlot = *SlotIt;
		DeleteSlots.Add(&CurSlot);
	}

	for (FAtlasedTextureSlot::TIterator SlotIt(AtlasEmptySlots); SlotIt; SlotIt.Next())
	{
		FAtlasedTextureSlot& CurSlot = *SlotIt;
		DeleteSlots.Add(&CurSlot);
	}

	AtlasUsedSlots = NULL;
	AtlasEmptySlots = NULL;

	for (FAtlasedTextureSlot* CurSlot : DeleteSlots)
	{
		delete CurSlot;
	}

	DeleteSlots.Empty();


	STAT(SIZE_T MemoryBefore = AtlasData.GetAllocatedSize());

	// Clear all raw data
	AtlasData.Empty();

	STAT(SIZE_T MemoryAfter = AtlasData.GetAllocatedSize());
	DEC_MEMORY_STAT_BY(STAT_SlateTextureAtlasMemory, MemoryBefore-MemoryAfter);
}


const FAtlasedTextureSlot* FSlateTextureAtlas::AddTexture( uint32 TextureWidth, uint32 TextureHeight, const TArray<uint8>& Data )
{
	// Find a spot for the character in the texture
	const FAtlasedTextureSlot* NewSlot = FindSlotForTexture(TextureWidth, TextureHeight);

	// handle cases like space, where the size of the glyph is zero. The copy data code doesn't handle zero sized source data with a padding
	// so make sure to skip this call.
	if (NewSlot && TextureWidth > 0 && TextureHeight > 0)
	{
		CopyDataIntoSlot(NewSlot, Data);
		MarkTextureDirty();
	}

	return NewSlot;
}

void FSlateTextureAtlas::MarkTextureDirty()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		const ESlateTextureAtlasThreadId AtlasThreadId = GetCurrentSlateTextureAtlasThreadId();
		check(AtlasThreadId != ESlateTextureAtlasThreadId::Unknown);

		check((GSlateLoadingThreadId != 0) || (AtlasOwnerThread == AtlasThreadId));
	}
#endif

	bNeedsUpdate = true;
}

void FSlateTextureAtlas::InitAtlasData()
{
	LLM_SCOPE_BYTAG(UI_Texture);

	check(AtlasEmptySlots == NULL && AtlasData.Num() == 0);

	FAtlasedTextureSlot* RootSlot = new FAtlasedTextureSlot(0, 0, AtlasWidth, AtlasHeight, GetPaddingAmount());

	RootSlot->LinkHead(AtlasEmptySlots);

	AtlasData.Reserve(AtlasWidth * AtlasHeight * BytesPerPixel);
	AtlasData.AddZeroed(AtlasWidth * AtlasHeight * BytesPerPixel);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	AtlasOwnerThread = GetCurrentSlateTextureAtlasThreadId();
	check(AtlasOwnerThread != ESlateTextureAtlasThreadId::Unknown);
#endif

	INC_MEMORY_STAT_BY(STAT_SlateTextureAtlasMemory, AtlasData.GetAllocatedSize());
} //-V773


void FSlateTextureAtlas::CopyRow( const FCopyRowData& CopyRowData )
{
	const uint8* Data = CopyRowData.SrcData;
	uint8* Start = CopyRowData.DestData;
	const uint32 SourceWidth = CopyRowData.SrcTextureWidth;
	const uint32 DestWidth = CopyRowData.DestTextureWidth;
	const uint32 SrcRow = CopyRowData.SrcRow;
	const uint32 DestRow = CopyRowData.DestRow;
	// this can only be one or zero
	const uint32 Padding = GetPaddingAmount();

	const uint8* SourceDataAddr = &Data[(SrcRow * SourceWidth) * BytesPerPixel];
	uint8* DestDataAddr = &Start[(DestRow * DestWidth + Padding) * BytesPerPixel];
	FMemory::Memcpy(DestDataAddr, SourceDataAddr, SourceWidth * BytesPerPixel);

	if (Padding > 0)
	{ 
		uint8* DestPaddingPixelLeft = &Start[(DestRow * DestWidth) * BytesPerPixel];
		uint8* DestPaddingPixelRight = DestPaddingPixelLeft + ((CopyRowData.RowWidth - 1) * BytesPerPixel);
		if (PaddingStyle == ESlateTextureAtlasPaddingStyle::DilateBorder)
		{
			const uint8* FirstPixel = SourceDataAddr; 
			const uint8* LastPixel = SourceDataAddr + ((SourceWidth - 1) * BytesPerPixel);
			FMemory::Memcpy(DestPaddingPixelLeft, FirstPixel, BytesPerPixel);
			FMemory::Memcpy(DestPaddingPixelRight, LastPixel, BytesPerPixel);
		}
		else
		{
			FMemory::Memzero(DestPaddingPixelLeft, BytesPerPixel);
			FMemory::Memzero(DestPaddingPixelRight, BytesPerPixel);
		}

	} 
}

void FSlateTextureAtlas::ZeroRow( const FCopyRowData& CopyRowData )
{
	const uint32 SourceWidth = CopyRowData.SrcTextureWidth;
	const uint32 DestWidth = CopyRowData.DestTextureWidth;
	const uint32 DestRow = CopyRowData.DestRow;

	uint8* DestDataAddr = &CopyRowData.DestData[DestRow * DestWidth * BytesPerPixel];
	FMemory::Memzero(DestDataAddr, CopyRowData.RowWidth * BytesPerPixel);
}


void FSlateTextureAtlas::CopyDataIntoSlot( const FAtlasedTextureSlot* SlotToCopyTo, const TArray<uint8>& Data )
{
	// Copy pixel data to the texture
	uint8* Start = &AtlasData[SlotToCopyTo->Y*AtlasWidth*BytesPerPixel + SlotToCopyTo->X*BytesPerPixel];
	
	// Account for same padding on each sides
	const uint32 Padding = GetPaddingAmount();
	const uint32 AllPadding = Padding*2;

	// Make sure the actual slot is not zero-area (otherwise padding could corrupt other images in the atlas)
	check(SlotToCopyTo->Width > AllPadding);
	check(SlotToCopyTo->Height > AllPadding);

	// The width of the source texture without padding (actual width)
	const uint32 SourceWidth = SlotToCopyTo->Width - AllPadding; 
	const uint32 SourceHeight = SlotToCopyTo->Height - AllPadding;

	FCopyRowData CopyRowData;
	CopyRowData.DestData = Start;
	CopyRowData.SrcData = Data.GetData();
	CopyRowData.DestTextureWidth = AtlasWidth;
	CopyRowData.SrcTextureWidth = SourceWidth;
	CopyRowData.RowWidth = SlotToCopyTo->Width;

	// Apply the padding for bilinear filtering. 
	// Not used if no padding (assumes sampling outside boundaries of the sub texture is not possible)
	if (Padding > 0)
	{
		// Copy first color row into padding.  
		CopyRowData.SrcRow = 0;
		CopyRowData.DestRow = 0;

		if (PaddingStyle == ESlateTextureAtlasPaddingStyle::DilateBorder)
		{
			CopyRow(CopyRowData);
		}
		else
		{
			ZeroRow(CopyRowData);
		}
	}

	// Copy each row of the texture
	for (uint32 Row = Padding; Row < SlotToCopyTo->Height-Padding; ++Row)
	{
		CopyRowData.SrcRow = Row-Padding;
		CopyRowData.DestRow = Row;

		CopyRow(CopyRowData);
	}

	if (Padding > 0)
	{
		// Copy last color row into padding row for bilinear filtering
		CopyRowData.SrcRow = SourceHeight - 1;
		CopyRowData.DestRow = SlotToCopyTo->Height - Padding;

		if (PaddingStyle == ESlateTextureAtlasPaddingStyle::DilateBorder)
		{
			CopyRow(CopyRowData);
		}
		else
		{
			ZeroRow(CopyRowData);
		}
	}
}

#if WITH_ATLAS_DEBUGGING
const FAtlasedTextureSlot* FSlateTextureAtlas::GetSlotAtPosition(FIntPoint InPosition) const
{
	for (FAtlasedTextureSlot::TIterator SlotIt(AtlasUsedSlots); SlotIt; SlotIt++)
	{
		FAtlasedTextureSlot& CurSlot = *SlotIt;

		checkSlow(CurSlot.X <= (uint32)std::numeric_limits<int32>::max());
		checkSlow(CurSlot.Y <= (uint32)std::numeric_limits<int32>::max());
		checkSlow(CurSlot.X + CurSlot.Width <= (uint32)std::numeric_limits<int32>::max());
		checkSlow(CurSlot.Y + CurSlot.Height <= (uint32)std::numeric_limits<int32>::max());
		FIntRect CurSlotRect(FIntPoint((int32)CurSlot.X, (int32)CurSlot.Y), FIntPoint((int32)(CurSlot.X + CurSlot.Width), (int32)(CurSlot.Y + CurSlot.Height)));
		if (CurSlotRect.Contains(InPosition))
		{
			return &CurSlot;
		}
	}

	return nullptr;
}
#endif

const FAtlasedTextureSlot* FSlateTextureAtlas::FindSlotForTexture(uint32 InWidth, uint32 InHeight)
{
	FAtlasedTextureSlot* ReturnVal = NULL;

	// Account for padding on both sides
	const uint8 Padding = GetPaddingAmount();
	const uint32 TotalPadding = Padding * 2;
	const uint32 PaddedWidth = InWidth + TotalPadding;
	const uint32 PaddedHeight = InHeight + TotalPadding;

	// Previously, slots were stored as a binary tree - this has been replaced with a linked-list of slots on the edge of the tree
	// (slots on the edge of the tree represent empty slots); this iterates empty slots in same order as a binary depth-first-search,
	// except much faster.
	for (FAtlasedTextureSlot::TIterator SlotIt(AtlasEmptySlots); SlotIt; SlotIt++)
	{
		FAtlasedTextureSlot& CurSlot = *SlotIt;

		if (PaddedWidth <= CurSlot.Width && PaddedHeight <= CurSlot.Height)
		{
			ReturnVal = &CurSlot;
			break;
		}
	}


	if (ReturnVal != NULL)
	{
		// The width and height of the new child node
		const uint32 RemainingWidth =  FMath::Max<int32>(0, ReturnVal->Width - PaddedWidth);
		const uint32 RemainingHeight = FMath::Max<int32>(0, ReturnVal->Height - PaddedHeight);

		// New slots must have a minimum width/height, to avoid excessive slots i.e. excessive memory usage and iteration.
		// No glyphs seem to use slots this small, and cutting these slots out improves performance/memory-usage a fair bit
		const uint32 MinSlotDim = 2;

		// Split the remaining area around this slot into two children.
		if (RemainingHeight >= MinSlotDim || RemainingWidth >= MinSlotDim)
		{
			FAtlasedTextureSlot* LeftSlot = NULL;
			FAtlasedTextureSlot* RightSlot = NULL;

			if (RemainingHeight <= RemainingWidth)
			{
				// Split vertically
				// - - - - - - - - -
				// |       |       |
				// |  Slot |       |
				// |       |       |
				// | - - - | Right |
				// |       |       |
				// |  Left |       |
				// |       |       |
				// - - - - - - - - -
				LeftSlot = new FAtlasedTextureSlot(ReturnVal->X, ReturnVal->Y + PaddedHeight, PaddedWidth, RemainingHeight, Padding);
				RightSlot = new FAtlasedTextureSlot(ReturnVal->X + PaddedWidth, ReturnVal->Y, RemainingWidth, ReturnVal->Height, Padding);
			}
			else
			{
				// Split horizontally
				// - - - - - - - - -
				// |       |       |
				// |  Slot | Left  |
				// |       |       |
				// | - - - - - - - |
				// |               |
				// |     Right     |
				// |               |
				// - - - - - - - - -
				LeftSlot = new FAtlasedTextureSlot(ReturnVal->X + PaddedWidth, ReturnVal->Y, RemainingWidth, PaddedHeight, Padding);
				RightSlot = new FAtlasedTextureSlot(ReturnVal->X, ReturnVal->Y + PaddedHeight, ReturnVal->Width, RemainingHeight, Padding);
			}

			// Replace the old slot within AtlasEmptySlots, with the new Left and Right slot, then add the old slot to AtlasUsedSlots
			LeftSlot->LinkReplace(ReturnVal);
			RightSlot->LinkAfter(LeftSlot);

			ReturnVal->LinkHead(AtlasUsedSlots);
		}
		else
		{
			// Remove the old slot from AtlasEmptySlots, into AtlasUsedSlots
			ReturnVal->Unlink();
			ReturnVal->LinkHead(AtlasUsedSlots);
		}


		// Shrink the slot to the remaining area.
		ReturnVal->Width = PaddedWidth;
		ReturnVal->Height = PaddedHeight;
	}

	return ReturnVal;
}



FSlateFlushableAtlasCache::FSlateFlushableAtlasCache(const FAtlasFlushParams* InFlushParams)
	: FlushParams(InFlushParams)
{
	check(FlushParams);

	CurrentMaxGrayscaleAtlasPagesBeforeFlushRequest = FlushParams->InitialMaxAtlasPagesBeforeFlushRequest;
	CurrentMaxColorAtlasPagesBeforeFlushRequest = FlushParams->InitialMaxAtlasPagesBeforeFlushRequest;
	CurrentMaxNonAtlasedTexturesBeforeFlushRequest = FlushParams->InitialMaxNonAtlasPagesBeforeFlushRequest;
}

void FSlateFlushableAtlasCache::ResetFlushCounters()
{
	CurrentMaxGrayscaleAtlasPagesBeforeFlushRequest = FlushParams->InitialMaxAtlasPagesBeforeFlushRequest;
	CurrentMaxColorAtlasPagesBeforeFlushRequest = FlushParams->InitialMaxAtlasPagesBeforeFlushRequest;
	CurrentMaxNonAtlasedTexturesBeforeFlushRequest = FlushParams->InitialMaxNonAtlasPagesBeforeFlushRequest;
	FrameCounterLastFlushRequest = GFrameCounter;
}

void FSlateFlushableAtlasCache::UpdateFlushCounters(int32 NumGrayscale, int32 NumColor, int32 NumNonAtlased)
{
	bool bFlushRequested = false;
	bFlushRequested |= UpdateInternal(NumGrayscale, CurrentMaxGrayscaleAtlasPagesBeforeFlushRequest, FlushParams->InitialMaxAtlasPagesBeforeFlushRequest, FlushParams->GrowAtlasFrameWindow);
	bFlushRequested |= UpdateInternal(NumColor, CurrentMaxColorAtlasPagesBeforeFlushRequest, FlushParams->InitialMaxAtlasPagesBeforeFlushRequest, FlushParams->GrowAtlasFrameWindow);
	bFlushRequested |= UpdateInternal(NumNonAtlased, CurrentMaxNonAtlasedTexturesBeforeFlushRequest, FlushParams->InitialMaxNonAtlasPagesBeforeFlushRequest, FlushParams->GrowNonAtlasFrameWindow);

	if (bFlushRequested)
	{
		ResetFlushCounters();
	}
}

bool FSlateFlushableAtlasCache::UpdateInternal(int32 CurrentNum, int32& MaxNum, int32 InitialMax, int32 FrameWindowNum)
{
	const uint64 LastFlushRequestFrameDelta = GFrameCounter - FrameCounterLastFlushRequest;

	if (CurrentNum > MaxNum)
	{
		// The InitialMaxNonAtlasedTexturesBeforeFlushRequest may have changed since last flush,
		// so double check that we're below the current max or initial, if we're under it,
		// update the current to the initial.
		if (CurrentNum <= InitialMax)
		{
			MaxNum = InitialMax;
		}
		// If we grew back up to this number of non-atlased textures within the same or next frame of the previous flush request, then we likely legitimately have 
		// a lot of data cached. We should update CurrentMaxNonAtlasedTexturesBeforeFlushRequest to give us a bit more flexibility before the next flush request
		else if (LastFlushRequestFrameDelta <= FrameWindowNum)
		{
			MaxNum = CurrentNum;
			UE_LOG(LogSlate, Warning, TEXT("Setting the threshold to trigger a flush to %d non-atlased textures as there is a lot of font data being cached."), CurrentNum);
		}
		else
		{
			// We've grown beyond our current stable limit - try and request a flush
			RequestFlushCache(FString::Printf(TEXT("Large atlases out of space; %d/%d Textures; frames since last flush: %llu"), CurrentNum, MaxNum, LastFlushRequestFrameDelta));
			
			return true;
		}
	}

	return false;
}
