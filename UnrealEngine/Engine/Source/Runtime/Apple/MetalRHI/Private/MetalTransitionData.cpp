// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	MetalTransitionData.cpp: Metal RHI Resource Transition Implementation.
==============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalTransitionData.h"


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Resource Transition Data Definitions -

#define UE_METAL_TRANSITION_DATA_USES_EXPLICIT_FENCING					0


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Resource Transition Data Implementation -


FMetalTransitionData::FMetalTransitionData(ERHIPipeline                         InSrcPipelines,
										   ERHIPipeline                         InDstPipelines,
										   ERHITransitionCreateFlags            InCreateFlags,
										   TArrayView<const FRHITransitionInfo> InInfos)
{
	SrcPipelines   = InSrcPipelines;
	DstPipelines   = InDstPipelines;
	CreateFlags    = InCreateFlags;

	bCrossPipeline = (SrcPipelines != DstPipelines);

	Infos.Append(InInfos.GetData(), InInfos.Num());

	// TODO: Determine whether the Metal RHI needs to create a separate, per-transition fence.
#if UE_METAL_TRANSITION_DATA_USES_EXPLICIT_FENCING
	if (bCrossPipeline && !EnumHasAnyFlags(CreateFlags, ERHITransitionCreateFlags::NoFence))
	{
		// Get the current context pointer.
		FMetalContext* Context = GetMetalDeviceContext().GetCurrentContext();

		// Get the current render pass fence.
		TRefCountPtr<FMetalFence> const& MetalFence = Context->GetCurrentRenderPass().End();

		// Write it again as we may wait on this fence in two different encoders.
		Context->GetCurrentRenderPass().Update(MetalFence);

		// Write it into the transition data.
		Fence = MetalFence;
		Fence->AddRef();
	}
#endif // UE_METAL_TRANSITION_DATA_USES_EXPLICIT_FENCING
}

void FMetalTransitionData::BeginResourceTransitions() const
{
	// TODO: Determine whether the Metal RHI needs to do anything here.
#if UE_METAL_TRANSITION_DATA_USES_EXPLICIT_FENCING
	if (Fence.IsValid())
	{
		FMetalContext* Context = GetMetalDeviceContext().GetCurrentContext();

		if (Context->GetCurrentCommandBuffer())
		{
			Context->SubmitCommandsHint(EMetalSubmitFlagsNone);
		}

		Context->GetCurrentRenderPass().Begin(Fence);
	}
#endif // UE_METAL_TRANSITION_DATA_USES_EXPLICIT_FENCING
}

void FMetalTransitionData::EndResourceTransitions() const
{
	// No action necessary for same pipe transitions
	if (SrcPipelines == DstPipelines)
	{
		return;
	}

	for (const auto& Info : Infos)
	{
		if (nullptr == Info.Resource)
		{
			continue;
		}

		switch (Info.Type)
		{
			case FRHITransitionInfo::EType::UAV:
				GetMetalDeviceContext().TransitionResource(Info.UAV);
				break;

			case FRHITransitionInfo::EType::Buffer:
				GetMetalDeviceContext().TransitionRHIResource(Info.Buffer);
				break;

			case FRHITransitionInfo::EType::Texture:
				GetMetalDeviceContext().TransitionResource(Info.Texture);
				break;

			default:
				checkNoEntry();
				break;
		}
	}
}
