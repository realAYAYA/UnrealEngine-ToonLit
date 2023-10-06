// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	MetalTransitionData.cpp: Metal RHI Resource Transition Implementation.
==============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalTransitionData.h"


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Resource Transition Data Definitions -

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
}

void FMetalTransitionData::BeginResourceTransitions() const
{
}

void FMetalTransitionData::EndResourceTransitions() const
{
	//@ToDo: Support AysncCompute with MTLEvent
	check(SrcPipelines == DstPipelines);

	for (const auto& Info : Infos)
	{
		if (nullptr == Info.Resource)
		{
			continue;
		}

		if (Info.AccessAfter == ERHIAccess::Discard)
		{
			// Discard as a destination is a no-op
			continue;
		}

		checkf(Info.AccessAfter != ERHIAccess::Unknown, TEXT("Transitioning a resource to an unknown state is not allowed."));

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

void FMetalRHICommandContext::RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions)
{
	for (auto Transition : Transitions)
	{
		Transition->GetPrivateData<FMetalTransitionData>()->BeginResourceTransitions();
	}
}

void FMetalRHICommandContext::RHIEndTransitions(TArrayView<const FRHITransition*> Transitions)
{
	for (auto Transition : Transitions)
	{
		Transition->GetPrivateData<FMetalTransitionData>()->EndResourceTransitions();
	}
}
