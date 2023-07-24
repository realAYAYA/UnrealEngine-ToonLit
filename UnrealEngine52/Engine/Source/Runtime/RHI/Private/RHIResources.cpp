// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIResources.h"
#include "Containers/ConsumeAllMpmcQueue.h"
#include "RHI.h"
#include "Experimental/Containers/HazardPointer.h"
#include "Misc/MemStack.h"
#include "Stats/Stats.h"
#include "TextureProfiler.h"

UE::TConsumeAllMpmcQueue<FRHIResource*> PendingDeletes;
UE::TConsumeAllMpmcQueue<FRHIResource*> PendingDeletesWithLifetimeExtension;

FRHIResource* FRHIResource::CurrentlyDeleting = nullptr;

FRHIResource::FRHIResource(ERHIResourceType InResourceType)
	: ResourceType(InResourceType)
	, bCommitted(true)
	, bAllowExtendLifetime(true)
#if RHI_ENABLE_RESOURCE_INFO
	, bBeingTracked(false)
#endif
{
#if RHI_ENABLE_RESOURCE_INFO
	BeginTrackingResource(this);
#endif
}

FRHIResource::~FRHIResource()
{
	check(IsEngineExitRequested() || CurrentlyDeleting == this);
	check(AtomicFlags.GetNumRefs(std::memory_order_relaxed) == 0); // this should not have any outstanding refs
	CurrentlyDeleting = nullptr;

#if RHI_ENABLE_RESOURCE_INFO
	EndTrackingResource(this);
#endif
}

void FRHIResource::Destroy() const
{
	if (!AtomicFlags.MarkForDelete(std::memory_order_release))
	{
		if (bAllowExtendLifetime)
		{
			PendingDeletesWithLifetimeExtension.ProduceItem(const_cast<FRHIResource*>(this));
		}
		else
		{
			PendingDeletes.ProduceItem(const_cast<FRHIResource*>(this));
		}
	}
}

bool FRHIResource::Bypass()
{
	return GRHICommandList.Bypass();
}

int32 FRHIResource::FlushPendingDeletes(FRHICommandListImmediate& RHICmdList)
{
	return RHICmdList.FlushPendingDeletes();
}

FRHITexture::FRHITexture(const FRHITextureCreateDesc& InDesc)
	: FRHIViewableResource(RRT_Texture, InDesc.InitialState)
#if ENABLE_RHI_VALIDATION
	, RHIValidation::FTextureResource(InDesc)
#endif
	, TextureDesc(InDesc)
{
	SetName(InDesc.DebugName);
}

void FRHITexture::SetName(const FName& InName)
{
	Name = InName;

#if TEXTURE_PROFILER_ENABLED
	FTextureProfiler::Get()->UpdateTextureName(this);
#endif
}
