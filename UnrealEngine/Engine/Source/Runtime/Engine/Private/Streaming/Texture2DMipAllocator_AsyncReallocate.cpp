// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn_AsyncReallocate.cpp: Load texture 2D mips using ITextureMipDataProvider
=============================================================================*/

#include "Texture2DMipAllocator_AsyncReallocate.h"
#include "RenderUtils.h"
#include "Containers/ResourceArray.h"

extern TAutoConsoleVariable<int32> CVarFlushRHIThreadOnSTreamingTextureLocks;

FTexture2DMipAllocator_AsyncReallocate::FTexture2DMipAllocator_AsyncReallocate(UTexture* Texture)
	: FTextureMipAllocator(Texture, ETickState::AllocateMips, ETickThread::Render)
{
}

FTexture2DMipAllocator_AsyncReallocate::~FTexture2DMipAllocator_AsyncReallocate()
{
	check(!LockedMipIndices.Num());
}

// ********************************************************
// ********* FTextureMipAllocator implementation **********
// ********************************************************

bool FTexture2DMipAllocator_AsyncReallocate::AllocateMips(
	const FTextureUpdateContext& Context, 
	FTextureMipInfoArray& OutMipInfos, 
	const FTextureUpdateSyncOptions& SyncOptions)
{
	check(PendingFirstLODIdx < CurrentFirstLODIdx);

	FRHITexture2D* Texture2DRHI = Context.Resource ? Context.Resource->GetTexture2DRHI() : nullptr;
	if (!Texture2DRHI)
	{
		return false;
	}

	// Step (1) : Create the texture on the renderthread using RHIAsyncReallocateTexture2D. Wait for the RHI to signal the operation is completed through the FThreadSafeCounter.
	if (!IntermediateTextureRHI)
	{
		const FTexture2DMipMap& OwnerMip = *Context.MipsView[PendingFirstLODIdx];
		check(SyncOptions.Counter);

		SyncOptions.Counter->Increment();

		IntermediateTextureRHI = RHIAsyncReallocateTexture2D(
			Texture2DRHI,
			ResourceState.NumRequestedLODs,
			OwnerMip.SizeX,
			OwnerMip.SizeY,
			SyncOptions.Counter);

		// Run the next step, when IntermediateTextureRHI will be somewhat valid (after synchronization).
		AdvanceTo(ETickState::AllocateMips, ETickThread::Render);
		return true;
	}
	// Step (2) : Finalize the texture state through RHIFinalizeAsyncReallocateTexture2D and lock the new mips.
	else
	{
		const bool bFlushRHIThread = CVarFlushRHIThreadOnSTreamingTextureLocks.GetValueOnAnyThread() > 0;

		RHIFinalizeAsyncReallocateTexture2D(IntermediateTextureRHI, true);

		OutMipInfos.AddDefaulted(CurrentFirstLODIdx);

		for (int32 MipIndex = PendingFirstLODIdx; MipIndex < CurrentFirstLODIdx; ++MipIndex)
		{
			const FTexture2DMipMap& OwnerMip = *Context.MipsView[MipIndex];
			FTextureMipInfo& MipInfo = OutMipInfos[MipIndex];

			MipInfo.Format = Context.Resource->GetPixelFormat();
			MipInfo.SizeX = OwnerMip.SizeX;
			MipInfo.SizeY = OwnerMip.SizeY;
#if WITH_EDITORONLY_DATA
			MipInfo.DataSize = CalcTextureMipMapSize(MipInfo.SizeX, MipInfo.SizeY, MipInfo.Format, 0);
#else // Hasn't really been used on console. To investigate!
			MipInfo.DataSize = 0;
#endif
			MipInfo.DestData = RHILockTexture2D(IntermediateTextureRHI, MipIndex - PendingFirstLODIdx, RLM_WriteOnly, MipInfo.RowPitch, false, bFlushRHIThread);

			// Add this mip in the locked list of mips so that it can safely be unlocked when needed.
			LockedMipIndices.Add(MipIndex - PendingFirstLODIdx);
		}

		// New mips are ready to be unlocked by the FTextureMipDataProvider implementation.
		AdvanceTo(ETickState::FinalizeMips, ETickThread::Render);
		return true;
	}
}

bool FTexture2DMipAllocator_AsyncReallocate::FinalizeMips(const FTextureUpdateContext& Context, const FTextureUpdateSyncOptions& SyncOptions)
{
	if (!IntermediateTextureRHI)
	{
		return false;
	}

	// Unlock the mips so that the texture can be updated.
	UnlockNewMips();
	// Use the new texture resource for the texture asset, must run on the renderthread.
	Context.Resource->FinalizeStreaming(IntermediateTextureRHI);
	// No need for the intermediate texture anymore.
	IntermediateTextureRHI.SafeRelease();

	// Update complete, nothing more to do.
	AdvanceTo(ETickState::Done, ETickThread::None);
	return true;
}

void FTexture2DMipAllocator_AsyncReallocate::Cancel(const FTextureUpdateSyncOptions& SyncOptions)
{
	// Unlock any locked mips.
	UnlockNewMips();
	// Release the intermediate texture.
	IntermediateTextureRHI.SafeRelease();
}

FTextureMipAllocator::ETickThread FTexture2DMipAllocator_AsyncReallocate::GetCancelThread() const
{
	// If there is an intermediate texture and possibly locked mips. Unlock them and release it on the renderthread.
	if (IntermediateTextureRHI)
	{
		return ETickThread::Render;
	}
	// Nothing to do.
	else
	{
		return ETickThread::None;
	}
}

// ****************************
// ********* Helpers **********
// ****************************

void FTexture2DMipAllocator_AsyncReallocate::UnlockNewMips()
{
	// Unlock any locked mips.
	if (IntermediateTextureRHI)
	{
		const bool bFlushRHIThread = CVarFlushRHIThreadOnSTreamingTextureLocks.GetValueOnAnyThread() > 0;
		for (int32 MipIndex : LockedMipIndices)
		{
			RHIUnlockTexture2D(IntermediateTextureRHI, MipIndex, false, CVarFlushRHIThreadOnSTreamingTextureLocks.GetValueOnAnyThread() > 0 );
		}
		LockedMipIndices.Empty();
	}
}
