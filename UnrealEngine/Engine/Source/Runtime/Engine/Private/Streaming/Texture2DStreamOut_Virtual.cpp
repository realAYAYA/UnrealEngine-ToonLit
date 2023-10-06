// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamOut_Virtual.cpp: Definitions of classes used for texture.
=============================================================================*/

#include "Streaming/Texture2DStreamOut_Virtual.h"
#include "Rendering/Texture2DResource.h"
#include "Streaming/Texture2DUpdate.h"

FTexture2DStreamOut_Virtual::FTexture2DStreamOut_Virtual(UTexture2D* InTexture)
	: FTexture2DUpdate(InTexture) 
{
	if (!ensure(ResourceState.NumRequestedLODs <ResourceState.NumResidentLODs))
	{
		bIsCancelled = true;
	}
	
	PushTask(FContext(InTexture, TT_None), TT_Render, SRA_UPDATE_CALLBACK(Finalize), TT_None, nullptr);
}

// ****************************
// ******* Update Steps *******
// ****************************

void FTexture2DStreamOut_Virtual::Finalize(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamOut_Virtual::Finalize"), STAT_Texture2DStreamOutVirtual_Finalize, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	static TConsoleVariableData<int32>* CVarReducedMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTextureReducedMemory"));
	check(CVarReducedMode);

	if (!CVarReducedMode->GetValueOnRenderThread() || ResourceState.NumRequestedLODs > ResourceState.NumNonStreamingLODs)
	{
		IntermediateTextureRHI = Context.Resource->GetTexture2DRHI();
		RHIVirtualTextureSetFirstMipVisible(IntermediateTextureRHI, PendingFirstLODIdx);
		RHIVirtualTextureSetFirstMipInMemory(IntermediateTextureRHI, PendingFirstLODIdx);
	}
	else
	{
		 DoConvertToNonVirtual(Context);
	}
	DoFinishUpdate(Context);
}

// ****************************
// ******* Cancel Steps *******
// ****************************

void FTexture2DStreamOut_Virtual::Cancel(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamOut_Virtual::Cancel"), STAT_Texture2DStreamOutVirtual_Cancel, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	DoFinishUpdate(Context);
}
