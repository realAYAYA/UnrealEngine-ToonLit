// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DArrayStreaming.cpp: Helpers to stream in and out texture 2D array LODs.
=============================================================================*/

#include "Streaming/Texture2DArrayStreaming.h"
#include "RenderUtils.h"

//*****************************************************************************
//***************************** Global Definitions ****************************
//*****************************************************************************

void RHICopySharedMips(FRHICommandList& RHICmdList, FRHITexture2DArray* DestTexture, FRHITexture2DArray* SrcTexture)
{
	// Transition to copy source and dest
	{
		FRHITransitionInfo TransitionsBefore[] = { FRHITransitionInfo(SrcTexture, ERHIAccess::SRVMask, ERHIAccess::CopySrc), FRHITransitionInfo(DestTexture, ERHIAccess::SRVMask, ERHIAccess::CopyDest) };
		RHICmdList.Transition(MakeArrayView(TransitionsBefore, UE_ARRAY_COUNT(TransitionsBefore)));
	}

	// Copy 
	{
		FRHICopyTextureInfo CopyInfo;

		auto SetCopyInfo = [&](FRHITexture2DArray* Texture2DArray)
		{
			CopyInfo.Size.X = Texture2DArray->GetSizeX();
			CopyInfo.Size.Y = Texture2DArray->GetSizeY();
			CopyInfo.NumSlices = Texture2DArray->GetSizeZ();
			CopyInfo.NumMips = Texture2DArray->GetNumMips();
		};

		if (DestTexture->GetNumMips() < SrcTexture->GetNumMips())
		{
			SetCopyInfo(DestTexture);
		}
		else
		{
			SetCopyInfo(SrcTexture);
		}

		CopyInfo.SourceMipIndex = SrcTexture->GetNumMips() - CopyInfo.NumMips;
		CopyInfo.DestMipIndex = DestTexture->GetNumMips() - CopyInfo.NumMips;
		RHICmdList.CopyTexture(SrcTexture, DestTexture, CopyInfo);
	}

	// Transition to SRV
	{
		FRHITransitionInfo TransitionsAfter[] = { FRHITransitionInfo(SrcTexture, ERHIAccess::CopySrc, ERHIAccess::SRVMask), FRHITransitionInfo(DestTexture, ERHIAccess::CopyDest, ERHIAccess::SRVMask) };
		RHICmdList.Transition(MakeArrayView(TransitionsAfter, UE_ARRAY_COUNT(TransitionsAfter)));
	}
}

//*****************************************************************************
//******************* FTexture2DArrayMipAllocator_Reallocate ******************
//*****************************************************************************

FTexture2DArrayMipAllocator_Reallocate::FTexture2DArrayMipAllocator_Reallocate(UTexture* Texture)
	: FTextureMipAllocator(Texture, ETickState::AllocateMips, ETickThread::Async)
{
}

FTexture2DArrayMipAllocator_Reallocate::~FTexture2DArrayMipAllocator_Reallocate()
{
}

bool FTexture2DArrayMipAllocator_Reallocate::AllocateMips(const FTextureUpdateContext& Context, FTextureMipInfoArray& OutMipInfos, const FTextureUpdateSyncOptions& SyncOptions)
{
	OutMipInfos.AddDefaulted(CurrentFirstLODIdx);
	StreamedMipData.AddDefaulted(CurrentFirstLODIdx);
	StreamedSliceSize.AddZeroed(CurrentFirstLODIdx);

	// Allocate the mip memory as temporary buffers so that the FTextureMipDataProvider implementation can write to it.
	for (int32 MipIdx = PendingFirstLODIdx; MipIdx < CurrentFirstLODIdx; ++MipIdx)
	{
		const FTexture2DMipMap& OwnerMip = *Context.MipsView[MipIdx];
		FTextureMipInfo& MipInfo = OutMipInfos[MipIdx];

		// Note that streamed in mips size will always be at least as big as block size.
		MipInfo.Format = Context.Resource->GetPixelFormat();
		MipInfo.SizeX = OwnerMip.SizeX;
		MipInfo.SizeY = OwnerMip.SizeY;
		MipInfo.ArraySize = OwnerMip.SizeZ;

		uint32 TextureAlign = 0;

		MipInfo.DataSize = MipInfo.ArraySize * CalcTextureMipMapSize(MipInfo.SizeX, MipInfo.SizeY, MipInfo.Format, 0);
		MipInfo.DestData = FMemory::Malloc(MipInfo.DataSize, 16);

		check(MipInfo.DataSize % MipInfo.ArraySize == 0);

		StreamedMipData[MipIdx].Reset((uint8*)MipInfo.DestData);
		StreamedSliceSize[MipIdx] = MipInfo.DataSize / MipInfo.ArraySize;
	}

	AdvanceTo(ETickState::FinalizeMips, ETickThread::Render);
	return true;
}

bool FTexture2DArrayMipAllocator_Reallocate::FinalizeMips(const FTextureUpdateContext& Context, const FTextureUpdateSyncOptions& SyncOptions)
{
	const uint32 NumSlices = Context.Resource->GetSizeZ();

	// Create new Texture.
	{
		const FTexture2DMipMap& FirstMip = *Context.MipsView[PendingFirstLODIdx];

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2DArray(TEXT("FinalizeMips"), FirstMip.SizeX, FirstMip.SizeY, FirstMip.SizeZ, Context.Resource->GetPixelFormat())
			.SetNumMips(ResourceState.NumRequestedLODs)
			.SetFlags(Context.Resource->GetCreationFlags())
			.SetExtData(Context.Resource->GetExtData());

		IntermediateTextureRHI = RHICreateTexture(Desc);
	}

	// Copy shared mips.
	{
		bool bCopySharedMipsDone = false;
		ENQUEUE_RENDER_COMMAND(FCopySharedMipsForTexture2DArray)(
			[&](FRHICommandListImmediate& RHICmdList)
		{
			RHICopySharedMips(RHICmdList, IntermediateTextureRHI.GetReference(), Context.Resource->GetTexture2DArrayRHI());
			bCopySharedMipsDone = true;
		});
		// Expected to execute immediately since ran on the renderthread.
		check(bCopySharedMipsDone);
	}

	// Update the streamed in mips if they were not initialized from the bulk data.
	for (int32 MipIdx = PendingFirstLODIdx; MipIdx < CurrentFirstLODIdx; ++MipIdx)
	{
		const FTexture2DMipMap& Mip = *Context.MipsView[MipIdx];
		const uint8* MipData = StreamedMipData[MipIdx].Get();
		const uint64 SlizeSize = StreamedSliceSize[MipIdx];
		if (MipData && SlizeSize)
		{
			const int32 RHIMipIndex = MipIdx - PendingFirstLODIdx;
			for (uint32 SliceIdx = 0; SliceIdx < NumSlices; ++SliceIdx)
			{
				uint32 DestStride = 0;
				void* DestData = RHILockTexture2DArray(IntermediateTextureRHI, SliceIdx, RHIMipIndex, RLM_WriteOnly, DestStride, false);
				FMemory::Memcpy(DestData, MipData + SliceIdx * SlizeSize, SlizeSize);
				RHIUnlockTexture2DArray(IntermediateTextureRHI, SliceIdx, RHIMipIndex, false);
			}

		}
	}
	StreamedMipData.Empty();
	StreamedSliceSize.Empty();

	// Use the new texture resource for the texture asset, must run on the renderthread.
	Context.Resource->FinalizeStreaming(IntermediateTextureRHI);
	// No need for the intermediate texture anymore.
	IntermediateTextureRHI.SafeRelease();

	// Update is complete, nothing more to do.
	AdvanceTo(ETickState::Done, ETickThread::None);
	return true;
}

void FTexture2DArrayMipAllocator_Reallocate::Cancel(const FTextureUpdateSyncOptions& SyncOptions)
{
	// Release the intermediate texture. If not null, this will be on the renderthread.
	IntermediateTextureRHI.SafeRelease();
	// Release the temporary mip data. Can be run on either renderthread or async threads.
	StreamedMipData.Empty();
	StreamedSliceSize.Empty();
}

FTextureMipAllocator::ETickThread FTexture2DArrayMipAllocator_Reallocate::GetCancelThread() const
{
	// If there is an  intermediate texture, it is safer to released on the renderthread.
	if (IntermediateTextureRHI)
	{
		return ETickThread::Render;
	}
	// Otherwise, if there are only temporary mip data, they can be freed on any threads.
	else if (StreamedMipData.Num())
	{
		return ETickThread::Async;
	}
	// Nothing to do.
	else
	{
		return ETickThread::None;
	}
}
