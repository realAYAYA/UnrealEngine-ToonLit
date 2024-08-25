// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHIValidation.cpp: Public RHI Validation layer definitions.
=============================================================================*/

#include "RHIValidation.h"
#include "RHIValidationContext.h"
#include "HAL/IConsoleManager.h"
#include "RHIValidationTransientResourceAllocator.h"
#include "HAL/PlatformStackWalk.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDeviceRedirector.h"
#include "RHIContext.h"
#include "RHIStrings.h"
#include "Algo/BinarySearch.h"

#if ENABLE_RHI_VALIDATION

bool GRHIValidationEnabled = false;

bool GRHIValidateBufferSourceCopy = true;

// When set to 1, callstack for each uniform buffer allocation will be tracked 
// (slow and leaks memory, but can be handy to find the location where an invalid
// allocation has been made)
#define CAPTURE_UNIFORMBUFFER_ALLOCATION_BACKTRACES 0

// When set to 1, logs resource transitions on all unnamed resources, useful for
// tracking down missing barriers when "-RHIValidationLog" cannot be used.
// Don't leave this enabled. Log backtraces are leaked.
#define LOG_UNNAMED_RESOURCES 0

namespace RHIValidation
{
	int32 GBreakOnTransitionError = 1;
	FAutoConsoleVariableRef CVarBreakOnTransitionError(
		TEXT("r.RHIValidation.DebugBreak.Transitions"),
		GBreakOnTransitionError,
		TEXT("Controls whether the debugger should break when a validation error is encountered.\n")
		TEXT(" 0: disabled;\n")
		TEXT(" 1: break in the debugger if a validation error is encountered."),
		ECVF_RenderThreadSafe);

	// Returns an array of resource names parsed from the "-RHIValidationLog" command line switch.
	// RHI validation logging is automatically enabled for resources whose debug names match those in this list.
	// Multiple values are comma separated, e.g. -RHIValidationLog="SceneDepthZ,GBufferA"
	static TArray<FString> const& GetAutoLogResourceNames()
	{
		struct FInit
		{
			TArray<FString> Strings;

			FInit()
			{
				FString ResourceNames;
				if (FParse::Value(FCommandLine::Get(), TEXT("-RHIValidationLog="), ResourceNames, false))
				{
					FString Left, Right;
					while (ResourceNames.Split(TEXT(","), &Left, &Right))
					{
						Left.TrimStartAndEndInline();
						Strings.Add(Left);
						ResourceNames = Right;
					}

					ResourceNames.TrimStartAndEndInline();
					Strings.Add(ResourceNames);
				}
			}
		} static Init;

		return Init.Strings;
	}

	FTextureResource::FTextureResource(FRHITextureCreateDesc const& CreateDesc)
		: FTextureResource()
	{
		InitBarrierTracking(CreateDesc);
	}

	void FTextureResource::InitBarrierTracking(FRHITextureCreateDesc const& CreateDesc)
	{
		InitBarrierTracking(
			CreateDesc.NumMips,
			CreateDesc.ArraySize * (CreateDesc.IsTextureCube() ? 6 : 1),
			CreateDesc.Format,
			CreateDesc.Flags,
			CreateDesc.InitialState,
			CreateDesc.DebugName);
	}

	void FTextureResource::InitBarrierTracking(int32 InNumMips, int32 InNumArraySlices, EPixelFormat PixelFormat, ETextureCreateFlags Flags, ERHIAccess InResourceState, const TCHAR* InDebugName)
	{
		FResource* Resource = GetTrackerResource();
		if (!Resource)
			return;

		int32 InNumPlanes = 1;

		// @todo: htile tracking
		if (IsStencilFormat(PixelFormat))
		{
			InNumPlanes = 2; // Depth + Stencil
		}
		else
		{
			InNumPlanes = 1; // Depth only
		}

		Resource->InitBarrierTracking(InNumMips, InNumArraySlices, InNumPlanes, InResourceState, InDebugName);
	}

	FResourceIdentity FTextureResource::GetViewIdentity(uint32 InMipIndex, uint32 InNumMips, uint32 InArraySlice, uint32 InNumArraySlices, uint32 InPlaneIndex, uint32 InNumPlanes)
	{
		FResource* Resource = GetTrackerResource();

		checkSlow((InMipIndex + InNumMips) <= Resource->NumMips);
		checkSlow((InArraySlice + InNumArraySlices) <= Resource->NumArraySlices);
		checkSlow((InPlaneIndex + InNumPlanes) <= Resource->NumPlanes);

		if (InNumMips == 0)
		{
			InNumMips = Resource->NumMips;
		}
		if (InNumArraySlices == 0)
		{
			InNumArraySlices = Resource->NumArraySlices;
		}
		if (InNumPlanes == 0)
		{
			InNumPlanes = Resource->NumPlanes;
		}

		FResourceIdentity Identity;
		Identity.Resource = Resource;
		Identity.SubresourceRange.MipIndex = InMipIndex;
		Identity.SubresourceRange.NumMips = InNumMips;
		Identity.SubresourceRange.ArraySlice = InArraySlice;
		Identity.SubresourceRange.NumArraySlices = InNumArraySlices;
		Identity.SubresourceRange.PlaneIndex = InPlaneIndex;
		Identity.SubresourceRange.NumPlanes = InNumPlanes;
		return Identity;
	}

	FResourceIdentity FTextureResource::GetTransitionIdentity(const FRHITransitionInfo& Info)
	{
		FResource* Resource = GetTrackerResource();

		FResourceIdentity Identity;
		Identity.Resource = Resource;

		if (Info.IsAllMips())
		{
			Identity.SubresourceRange.MipIndex = 0;
			Identity.SubresourceRange.NumMips = Resource->NumMips;
		}
		else
		{
			check(Info.MipIndex < uint32(Resource->NumMips));
			Identity.SubresourceRange.MipIndex = Info.MipIndex;
			Identity.SubresourceRange.NumMips = 1;
		}

		if (Info.IsAllArraySlices())
		{
			Identity.SubresourceRange.ArraySlice = 0;
			Identity.SubresourceRange.NumArraySlices = Resource->NumArraySlices;
		}
		else
		{
			check(Info.ArraySlice < uint32(Resource->NumArraySlices));
			Identity.SubresourceRange.ArraySlice = Info.ArraySlice;
			Identity.SubresourceRange.NumArraySlices = 1;
		}

		if (Info.IsAllPlaneSlices())
		{
			Identity.SubresourceRange.PlaneIndex = 0;
			Identity.SubresourceRange.NumPlanes = Resource->NumPlanes;
		}
		else
		{
			check(Info.PlaneSlice < uint32(Resource->NumPlanes));
			Identity.SubresourceRange.PlaneIndex = Info.PlaneSlice;
			Identity.SubresourceRange.NumPlanes = 1;
		}

		return Identity;
	}

	RHI_API FViewIdentity::FViewIdentity(FRHIViewableResource* InResource, FRHIViewDesc const& InViewDesc)
	{
		if (InViewDesc.IsBuffer())
		{
			FRHIBuffer* Buffer = static_cast<FRHIBuffer*>(InResource);
			Resource = Buffer;

			if (InViewDesc.IsUAV())
			{
				auto const Info = InViewDesc.Buffer.UAV.GetViewInfo(Buffer);
				if (ensureMsgf(!Info.bNullView, TEXT("Attempt to use a null buffer UAV.")))
				{
					SubresourceRange = Resource->GetWholeResourceRange();
					Stride = Info.StrideInBytes;
				}
			}
			else
			{
				auto const Info = InViewDesc.Buffer.SRV.GetViewInfo(Buffer);
				if (ensureMsgf(!Info.bNullView, TEXT("Attempt to use a null buffer SRV.")))
				{
					SubresourceRange = Resource->GetWholeResourceRange();
					Stride = Info.StrideInBytes;
				}
			}
		}
		else
		{
			FRHITexture* Texture = static_cast<FRHITexture*>(InResource);
			Resource = Texture->GetTrackerResource();
			
			auto GetPlaneIndex = [](ERHITexturePlane Plane)
			{
				switch (Plane)
				{
				default: checkNoEntry(); [[fallthrough]];
				case ERHITexturePlane::Primary:
				case ERHITexturePlane::PrimaryCompressed:
				case ERHITexturePlane::Depth:
					return EResourcePlane::Common;
				
				case ERHITexturePlane::Stencil:
					return EResourcePlane::Stencil;

				case ERHITexturePlane::HTile:
					return EResourcePlane::Htile;

				case ERHITexturePlane::FMask:
					return EResourcePlane::Cmask;

				case ERHITexturePlane::CMask:
					return EResourcePlane::Fmask;
				}
			};

			if (InViewDesc.IsUAV())
			{
				auto const Info = InViewDesc.Texture.UAV.GetViewInfo(Texture);

				SubresourceRange.MipIndex       = Info.MipLevel;
				SubresourceRange.NumMips        = 1;
				SubresourceRange.ArraySlice     = Info.ArrayRange.First;
				SubresourceRange.NumArraySlices = Info.ArrayRange.Num;
				SubresourceRange.PlaneIndex     = uint32(GetPlaneIndex(Info.Plane));
				SubresourceRange.NumPlanes      = 1;

				Stride = GPixelFormats[Info.Format].BlockBytes;
			}
			else
			{
				auto const Info = InViewDesc.Texture.SRV.GetViewInfo(Texture);

				SubresourceRange.MipIndex       = Info.MipRange.First;
				SubresourceRange.NumMips        = Info.MipRange.Num;
				SubresourceRange.ArraySlice     = Info.ArrayRange.First;
				SubresourceRange.NumArraySlices = Info.ArrayRange.Num;
				SubresourceRange.PlaneIndex     = uint32(GetPlaneIndex(Info.Plane));
				SubresourceRange.NumPlanes      = 1;

				Stride = GPixelFormats[Info.Format].BlockBytes;
			}
		}
	}

	void FTracker::FUAVTracker::DrawOrDispatch(FTracker* BarrierTracker, const FState& RequiredState)
	{
		// The barrier tracking expects us to call Assert() only once per unique resource.
		// However, multiple UAVs may be bound, all referencing the same resource.
		// Find the unique resources to ensure we only do the tracking once per resource.
		uint32 NumUniqueIdentities = 0;
		FResourceIdentity UniqueIdentities[MaxSimultaneousUAVs];

		for (int32 UAVIndex = 0; UAVIndex < UAVs.Num(); ++UAVIndex)
		{
			if (UAVs[UAVIndex])
			{
				const FResourceIdentity& Identity = UAVs[UAVIndex]->GetViewIdentity();

				// Check if we've already seen this resource.
				bool bFound = false;
				for (uint32 Index = 0; !bFound && Index < NumUniqueIdentities; ++Index)
				{
					bFound = UniqueIdentities[Index] == Identity;
				}

				if (!bFound)
				{
					check(NumUniqueIdentities < UE_ARRAY_COUNT(UniqueIdentities));
					UniqueIdentities[NumUniqueIdentities++] = Identity;

					// Assert unique resources have the required state.
					BarrierTracker->AddOp(FOperation::Assert(Identity, RequiredState));
				}
			}
		}
	}
}

TSet<uint32> FValidationRHI::SeenFailureHashes;
FCriticalSection FValidationRHI::SeenFailureHashesMutex;

FValidationRHI::FValidationRHI(FDynamicRHI* InRHI)
	: RHI(InRHI)
{
	check(RHI);
	UE_LOG(LogRHI, Log, TEXT("FValidationRHI on, intercepting %s RHI!"), InRHI && InRHI->GetName() ? InRHI->GetName() : TEXT("<NULL>"));
	GRHIValidationEnabled = true;
	SeenFailureHashes.Reserve(256);
}

FValidationRHI::~FValidationRHI()
{
	GRHIValidationEnabled = false;
}

IRHITransientResourceAllocator* FValidationRHI::RHICreateTransientResourceAllocator()
{
	// Wrap around validation allocator
	if (IRHITransientResourceAllocator* RHIAllocator = RHI->RHICreateTransientResourceAllocator())
	{
		return new FValidationTransientResourceAllocator(RHIAllocator);
	}
	else
	{
		return nullptr;
	}
}

IRHICommandContext* FValidationRHI::RHIGetDefaultContext()
{
	IRHICommandContext* LowLevelContext = RHI->RHIGetDefaultContext();
	IRHICommandContext* HighLevelContext = static_cast<IRHICommandContext*>(&LowLevelContext->GetHighestLevelContext());

	if (LowLevelContext == HighLevelContext)
	{
		FValidationContext* ValidationContext = new FValidationContext(FValidationContext::EType::Default);
		ValidationContext->LinkToContext(LowLevelContext);
		HighLevelContext = ValidationContext;
	}

	return HighLevelContext;
}

IRHIComputeContext* FValidationRHI::RHIGetDefaultAsyncComputeContext()
{
	IRHIComputeContext* LowLevelContext = RHI->RHIGetDefaultAsyncComputeContext();
	IRHIComputeContext* HighLevelContext = &LowLevelContext->GetHighestLevelContext();

	if (LowLevelContext == HighLevelContext)
	{
		FValidationComputeContext* ValidationContext = new FValidationComputeContext(FValidationComputeContext::EType::Default);
		ValidationContext->LinkToContext(LowLevelContext);
		HighLevelContext = ValidationContext;
	}

	return HighLevelContext;
}

struct FValidationCommandList : public IRHIPlatformCommandList
{
	ERHIPipeline Pipeline;
	IRHIPlatformCommandList* InnerCommandList;
	RHIValidation::FOperationsList CompletedOpList;
};

IRHIComputeContext* FValidationRHI::RHIGetCommandContext(ERHIPipeline Pipeline, FRHIGPUMask GPUMask)
{
	IRHIComputeContext* InnerContext = RHI->RHIGetCommandContext(Pipeline, GPUMask);
	check(InnerContext);

	switch (Pipeline)
	{
	case ERHIPipeline::Graphics:
	{
		FValidationContext* OuterContext = new FValidationContext(FValidationContext::EType::Parallel);
		OuterContext->LinkToContext(static_cast<IRHICommandContext*>(InnerContext));
		return OuterContext;
	}

	case ERHIPipeline::AsyncCompute:
	{
		FValidationComputeContext* OuterContext = new FValidationComputeContext(FValidationComputeContext::EType::Parallel);
		OuterContext->LinkToContext(InnerContext);
		return OuterContext;
	}

	default:
		checkNoEntry();
		return nullptr;
	}
}

IRHIPlatformCommandList* FValidationRHI::RHIFinalizeContext(IRHIComputeContext* OuterContext)
{
	IRHIComputeContext& InnerContext = OuterContext->GetLowestLevelContext();

	FValidationCommandList* OuterCommandList = new FValidationCommandList();

	// RHIFinalizeContext makes the context available to other threads, so finalize the tracker beforehand.
	OuterCommandList->CompletedOpList = InnerContext.Tracker->Finalize();
	OuterCommandList->InnerCommandList = RHI->RHIFinalizeContext(&InnerContext);
	OuterCommandList->Pipeline = OuterContext->GetPipeline();

	switch (OuterCommandList->Pipeline)
	{
	case ERHIPipeline::Graphics:
		if (static_cast<FValidationContext*>(OuterContext)->Type == FValidationContext::EType::Parallel)
			delete OuterContext;
		break;

	case ERHIPipeline::AsyncCompute:
		if (static_cast<FValidationComputeContext*>(OuterContext)->Type == FValidationComputeContext::EType::Parallel)
			delete OuterContext;
		break;

	default:
		checkNoEntry();
		break;
	}

	return OuterCommandList;
}

void FValidationRHI::RHISubmitCommandLists(TArrayView<IRHIPlatformCommandList*> OuterCommandLists, bool bFlushResources)
{
	FMemMark Mark(FMemStack::Get());
	TArray<IRHIPlatformCommandList*, TMemStackAllocator<>> InnerCommandLists;
	InnerCommandLists.Reserve(OuterCommandLists.Num());

	for (IRHIPlatformCommandList* CmdList : OuterCommandLists)
	{
		FValidationCommandList* OuterCommandList = static_cast<FValidationCommandList*>(CmdList);

		// Replay or queue any barrier operations to validate resource barrier usage.
		RHIValidation::FTracker::ReplayOpQueue(OuterCommandList->Pipeline, MoveTemp(OuterCommandList->CompletedOpList));

		if (OuterCommandList->InnerCommandList)
		{
			InnerCommandLists.Add(MoveTemp(OuterCommandList->InnerCommandList));
		}

		delete OuterCommandList;
	}

	if (InnerCommandLists.Num() || bFlushResources)
	{
		RHI->RHISubmitCommandLists(InnerCommandLists, bFlushResources);
	}
}

void FValidationRHI::ValidatePipeline(const FGraphicsPipelineStateInitializer& PSOInitializer)
{
	{
		// Verify depth/stencil access/usage
		bool bHasDepth = IsDepthOrStencilFormat(PSOInitializer.DepthStencilTargetFormat);
		bool bHasStencil = IsStencilFormat(PSOInitializer.DepthStencilTargetFormat);
		const FDepthStencilStateInitializerRHI& Initializer = DepthStencilStates.FindChecked(PSOInitializer.DepthStencilState);
		if (bHasDepth)
		{
			if (!bHasStencil)
			{
				RHI_VALIDATION_CHECK(!Initializer.bEnableFrontFaceStencil
					&& Initializer.FrontFaceStencilTest == CF_Always
					&& Initializer.FrontFaceStencilFailStencilOp == SO_Keep
					&& Initializer.FrontFaceDepthFailStencilOp == SO_Keep
					&& Initializer.FrontFacePassStencilOp == SO_Keep
					&& !Initializer.bEnableBackFaceStencil
					&& Initializer.BackFaceStencilTest == CF_Always
					&& Initializer.BackFaceStencilFailStencilOp == SO_Keep
					&& Initializer.BackFaceDepthFailStencilOp == SO_Keep
					&& Initializer.BackFacePassStencilOp == SO_Keep, TEXT("No stencil render target set, yet PSO wants to use stencil operations!"));
/*
				RHI_VALIDATION_CHECK(PSOInitializer.StencilTargetLoadAction == ERenderTargetLoadAction::ENoAction,
					TEXT("No stencil target set, yet PSO wants to load from it!"));
				RHI_VALIDATION_CHECK(PSOInitializer.StencilTargetStoreAction == ERenderTargetStoreAction::ENoAction,
					TEXT("No stencil target set, yet PSO wants to store into it!"));
*/
			}
		}
		else
		{
			RHI_VALIDATION_CHECK(!Initializer.bEnableDepthWrite && Initializer.DepthTest == CF_Always, TEXT("No depth render target set, yet PSO wants to use depth operations!"));
			RHI_VALIDATION_CHECK(PSOInitializer.DepthTargetLoadAction == ERenderTargetLoadAction::ENoAction
				&& PSOInitializer.StencilTargetLoadAction == ERenderTargetLoadAction::ENoAction,
				TEXT("No depth/stencil target set, yet PSO wants to load from it!"));
			RHI_VALIDATION_CHECK(PSOInitializer.DepthTargetStoreAction == ERenderTargetStoreAction::ENoAction
				&& PSOInitializer.StencilTargetStoreAction == ERenderTargetStoreAction::ENoAction,
				TEXT("No depth/stencil target set, yet PSO wants to store into it!"));
		}
	}
}

void FValidationRHI::RHICreateTransition(FRHITransition* Transition, const FRHITransitionCreateInfo& CreateInfo)
{
	using namespace RHIValidation;

	const ERHIPipeline SrcPipelines = CreateInfo.SrcPipelines;
	const ERHIPipeline DstPipelines = CreateInfo.DstPipelines;

	struct FFenceEdge
	{
		FFence* Fence = nullptr;
		ERHIPipeline SrcPipe = ERHIPipeline::None;
		ERHIPipeline DstPipe = ERHIPipeline::None;
	};

	TArray<FFenceEdge> Fences;

	if (SrcPipelines != DstPipelines)
	{
		for (ERHIPipeline SrcPipe : GetRHIPipelines())
		{
			if (!EnumHasAnyFlags(SrcPipelines, SrcPipe))
			{
				continue;
			}

			for (ERHIPipeline DstPipe : GetRHIPipelines())
			{
				if (!EnumHasAnyFlags(DstPipelines, DstPipe) || SrcPipe == DstPipe)
				{
					continue;
				}

				FFenceEdge FenceEdge;
				FenceEdge.Fence = new FFence;
				FenceEdge.SrcPipe = SrcPipe;
				FenceEdge.DstPipe = DstPipe;
				Fences.Add(FenceEdge);
			}
		}
	}

	TArray<FOperation> SignalOps, WaitOps, AliasingOps, AliasingOverlapOps, BeginOps, EndOps;
	SignalOps .Reserve(Fences.Num());
	WaitOps   .Reserve(Fences.Num());
	AliasingOps.Reserve(CreateInfo.AliasingInfos.Num());
	AliasingOverlapOps.Reserve(CreateInfo.AliasingInfos.Num());
	BeginOps  .Reserve(CreateInfo.TransitionInfos.Num());
	EndOps    .Reserve(CreateInfo.TransitionInfos.Num());

	for (const FFenceEdge& FenceEdge : Fences)
	{
		WaitOps.Emplace(FOperation::Wait(FenceEdge.Fence, FenceEdge.DstPipe));
	}

	// Take a backtrace of this transition creation if any of the resources it contains have logging enabled.
	bool bDoTrace = false;

	for (const FRHITransientAliasingInfo& Info : CreateInfo.AliasingInfos)
	{
		if (!Info.Resource)
		{
			continue;
		}

		FResource* Resource = nullptr;

		if (Info.Type == FRHITransientAliasingInfo::EType::Texture)
		{
			Resource = Info.Texture->GetTrackerResource();
		}
		else
		{
			Resource = Info.Buffer;
		}

		bDoTrace |= (Resource->LoggingMode != RHIValidation::ELoggingMode::None);

		if (Info.IsAcquire())
		{
			checkf(Resource->TransientState.bTransient, TEXT("Acquiring resource %s which is not transient. Only transient resources can be acquired."), Resource->GetDebugName());
			checkf(SrcPipelines == ERHIPipeline::Graphics, TEXT("Acquiring a transient resource (%s) must begin on the graphics pipe."), Resource->GetDebugName());

			AliasingOps.Emplace(FOperation::AcquireTransientResource(Resource, nullptr));

			for (const FRHITransientAliasingOverlap& Overlap : Info.Overlaps)
			{
				FResource* ResourceBefore = nullptr;

				if (Overlap.Type == FRHITransientAliasingOverlap::EType::Texture)
				{
					ResourceBefore = Overlap.Texture->GetTrackerResource();
				}
				else
				{
					ResourceBefore = Overlap.Buffer;
				}

				checkf(ResourceBefore, TEXT("Null resource provided as an aliasing overlap of %s"), Resource->GetDebugName());

				AliasingOverlapOps.Emplace(FOperation::AliasingOverlap(ResourceBefore, Resource, nullptr));
			}
		}
		else
		{
			checkf(Info.Overlaps.IsEmpty(), TEXT("Aliasing overlaps provided on a Discard for resource %s. Overlaps must be provided on an acquire."), Resource->GetDebugName());
			checkf(Resource->TransientState.bTransient, TEXT("Discarding resource %s which is not transient. Only transient resources can be discarded."), Resource->GetDebugName());
			checkf(DstPipelines == ERHIPipeline::Graphics, TEXT("Discarding a transient resource (%s) must end on the graphics pipe."), Resource->GetDebugName());

			AliasingOps.Emplace(FOperation::DiscardTransientResource(Resource, nullptr));
		}
	}

	for (int32 Index = 0; Index < CreateInfo.TransitionInfos.Num(); ++Index)
	{
		const FRHITransitionInfo& Info = CreateInfo.TransitionInfos[Index];
		if (!Info.Resource)
			continue;

		checkf(Info.AccessAfter != ERHIAccess::Unknown, TEXT("FRHITransitionInfo::AccessAfter cannot be Unknown when creating a resource transition."));
		checkf(Info.Type != FRHITransitionInfo::EType::Unknown, TEXT("FRHITransitionInfo::Type cannot be Unknown when creating a resource transition."));

		if (const FRHICommitResourceInfo* CommitInfo = Info.CommitInfo.GetPtrOrNull())
		{
			RHI_VALIDATION_CHECK((SrcPipelines == ERHIPipeline::Graphics && DstPipelines == ERHIPipeline::Graphics),
				TEXT("Reserved resource commit operations are only supported on the graphics pipeline and must not cross pipeline boundary."));

			if (Info.Type == FRHITransitionInfo::EType::Buffer)
			{
				const FRHIBuffer* Buffer = Info.Buffer;
				const EBufferUsageFlags BufferUsage = Buffer->GetUsage();
				const uint32 BufferSize = Buffer->GetSize();
				RHI_VALIDATION_CHECK(EnumHasAllFlags(BufferUsage, BUF_ReservedResource), TEXT("Commit transitions can only be used with reserved resources."));
				RHI_VALIDATION_CHECK(CommitInfo->SizeInBytes <= BufferSize, TEXT("Buffer commit size request must not be larger than the size of the buffer itself, as virtual memory allocation cannot be resized."));
			}
			else
			{
				RHI_VALIDATION_CHECK(false, TEXT("Reserved resource commit is only supported for buffers"));
			}
		}

		FResourceIdentity Identity;

		switch (Info.Type)
		{
		default: checkNoEntry(); // fall through
		case FRHITransitionInfo::EType::Texture:
			Identity = Info.Texture->GetTransitionIdentity(Info);
			break;

		case FRHITransitionInfo::EType::Buffer:
			Identity = Info.Buffer->GetWholeResourceIdentity();
			break;

		case FRHITransitionInfo::EType::UAV:
			Identity = Info.UAV->GetViewIdentity();
			break;

		case FRHITransitionInfo::EType::BVH:
			Identity = Info.BVH->GetWholeResourceIdentity();
			break;
		}

		bDoTrace |= (Identity.Resource->LoggingMode != RHIValidation::ELoggingMode::None);

		FState PreviousState = FState(Info.AccessBefore, SrcPipelines);
		FState NextState = FState(Info.AccessAfter, DstPipelines);

		BeginOps.Emplace(FOperation::BeginTransitionResource(Identity, PreviousState, NextState, Info.Flags, nullptr));
		EndOps  .Emplace(FOperation::EndTransitionResource(Identity, PreviousState, NextState, nullptr));
	}

	if (bDoTrace)
	{
		void* Backtrace = CaptureBacktrace();

		for (FOperation& Op : AliasingOps)
		{
			switch (Op.Type)
			{
			case EOpType::AcquireTransient:
				Op.Data_AcquireTransient.CreateBacktrace = Backtrace;
				break;
			case EOpType::DiscardTransient:
				Op.Data_DiscardTransient.CreateBacktrace = Backtrace;
				break;
			}
		}

		for (FOperation& Op : AliasingOverlapOps) { Op.Data_AliasingOverlap.CreateBacktrace = Backtrace; }
		for (FOperation& Op : BeginOps) { Op.Data_BeginTransition.CreateBacktrace = Backtrace; }
		for (FOperation& Op : EndOps) { Op.Data_EndTransition.CreateBacktrace = Backtrace; }
	}

	for (const FFenceEdge& FenceEdge : Fences)
	{
		SignalOps.Emplace(FOperation::Signal(FenceEdge.Fence, FenceEdge.SrcPipe));
	}

	Transition->PendingSignals.Operations = MoveTemp(SignalOps);
	Transition->PendingWaits.Operations = MoveTemp(WaitOps);
	Transition->PendingAliases.Operations = MoveTemp(AliasingOps);
	Transition->PendingAliasingOverlaps.Operations = MoveTemp(AliasingOverlapOps);
	Transition->PendingOperationsBegin.Operations = MoveTemp(BeginOps);
	Transition->PendingOperationsEnd.Operations = MoveTemp(EndOps);

	return RHI->RHICreateTransition(Transition, CreateInfo);
}

namespace RHIValidation
{
	static inline FString GetReasonString_LockBufferInsideRenderPass(FResource* Buffer)
	{
		const TCHAR* DebugName = Buffer->GetDebugName();
		return FString::Printf(TEXT("Locking non-volatile buffers for writing inside a render pass is not allowed. Resource: \"%s\" (0x%p)."), DebugName ? DebugName : TEXT("Unnamed"), Buffer);
	}
}

void FValidationRHI::LockBufferValidate(class FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, EResourceLockMode LockMode)
{
	using namespace RHIValidation;

	check(GRHISupportsMultithreadedResources || RHICmdList.IsImmediate());

	if (!EnumHasAnyFlags(Buffer->GetUsage(), BUF_Volatile) && LockMode == RLM_WriteOnly)
	{
		bool bIsInsideRenderPass;
		if (RHICmdList.IsTopOfPipe())
		{
			bIsInsideRenderPass = RHICmdList.IsInsideRenderPass();
		}
		else
		{
			FValidationContext& Ctx = static_cast<FValidationContext&>(RHICmdList.GetContext());
			bIsInsideRenderPass = Ctx.State.bInsideBeginRenderPass;
		}
		RHI_VALIDATION_CHECK(!bIsInsideRenderPass, *GetReasonString_LockBufferInsideRenderPass(Buffer));
	}
}

void* FValidationRHI::RHILockBuffer(class FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	LockBufferValidate(RHICmdList, Buffer, LockMode);

	return RHI->RHILockBuffer(RHICmdList, Buffer, Offset, SizeRHI, LockMode);
}

void* FValidationRHI::RHILockBufferMGPU(class FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, uint32 GPUIndex, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	LockBufferValidate(RHICmdList, Buffer, LockMode);

	return RHI->RHILockBufferMGPU(RHICmdList, Buffer, GPUIndex, Offset, SizeRHI, LockMode);
}

class FRHIValidationBreadcrumbScope
{
public:
	FRHIValidationBreadcrumbScope(TConstArrayView<const TCHAR*> InBreadcrumbs)
	{
		Breadcrumbs = InBreadcrumbs;
	}

	~FRHIValidationBreadcrumbScope()
	{
		Breadcrumbs = {};
	}

	thread_local static TConstArrayView<const TCHAR*> Breadcrumbs;
};

thread_local TConstArrayView<const TCHAR*> FRHIValidationBreadcrumbScope::Breadcrumbs;

static FString GetBreadcrumbPath()
{
	return FString::Join(FRHIValidationBreadcrumbScope::Breadcrumbs, TEXT("/"));
}

// FlushType: Thread safe
void FValidationRHI::RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHITexture* Texture, const TCHAR* Name)
{
	FString NameCopyRT = Name;
	RHICmdList.EnqueueLambda([Texture, NameCopyRHIT = MoveTemp(NameCopyRT)](FRHICommandListBase& RHICmdList)
	{
		((FValidationContext&)RHICmdList.GetContext()).Tracker->Rename(Texture->GetTrackerResource(), *NameCopyRHIT);
	});

	RHI->RHIBindDebugLabelName(RHICmdList, Texture, Name);
}

void FValidationRHI::RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, const TCHAR* Name)
{
	FString NameCopyRT = Name;
	RHICmdList.EnqueueLambda([Buffer, NameCopyRHIT = MoveTemp(NameCopyRT)](FRHICommandListBase& RHICmdList)
	{
		((FValidationContext&)RHICmdList.GetContext()).Tracker->Rename(Buffer, *NameCopyRHIT);
	});

	RHI->RHIBindDebugLabelName(RHICmdList, Buffer, Name);
}

void FValidationRHI::RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHIUnorderedAccessView* UnorderedAccessViewRHI, const TCHAR* Name)
{
	RHIValidation::FResource* Resource = UnorderedAccessViewRHI->GetViewIdentity().Resource;
	FString NameCopyRT = Name;
	RHICmdList.EnqueueLambda([Resource, NameCopyRHIT = MoveTemp(NameCopyRT)](FRHICommandListBase& RHICmdList)
	{
		((FValidationContext&)RHICmdList.GetContext()).Tracker->Rename(Resource, *NameCopyRHIT);
	});

	RHI->RHIBindDebugLabelName(RHICmdList, UnorderedAccessViewRHI, Name);
}

void FValidationRHI::ReportValidationFailure(const TCHAR* InMessage)
{
	// Report failures only once per session, since many of them will happen repeatedly. This is similar to what ensure() does, but
	// ensure() looks at the source location to determine if it's seen the error before. We want to look at the actual message, since
	// all failures of a given kind will come from the same place, but (hopefully) the error message contains the name of the resource
	// and a description of the state, so it should be unique for each failure.
	uint32 Hash = FCrc::StrCrc32<TCHAR>(InMessage);
	
	SeenFailureHashesMutex.Lock();
	bool bIsAlreadyInSet;
	SeenFailureHashes.Add(Hash, &bIsAlreadyInSet);
	SeenFailureHashesMutex.Unlock();

	if (bIsAlreadyInSet)
	{
		return;
	}

	FString Message;

	if (!FRHIValidationBreadcrumbScope::Breadcrumbs.IsEmpty())
	{
		Message = FString::Printf(
			TEXT("%s")
			TEXT("Breadcrumbs: %s\n")
			TEXT("--------------------------------------------------------------------\n"),
			InMessage, *GetBreadcrumbPath());
	}
	else
	{
		Message = InMessage;
	}

	UE_LOG(LogRHI, Error, TEXT("%s"), *Message);

	if (FPlatformMisc::IsDebuggerPresent() && RHIValidation::GBreakOnTransitionError)
	{
		// Print the message again using the debug output function, because UE_LOG doesn't always reach
		// the VS output window before the breakpoint is triggered, despite the log flush call below.
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s\n"), *Message);
		GLog->Flush();
		PLATFORM_BREAK();
	}
}

FValidationComputeContext::FValidationComputeContext(EType InType)
	: Type(InType)
{
	State.Reset();
	Tracker = &State.TrackerInstance;
}

void FValidationComputeContext::FState::Reset()
{
	ComputePassName.Reset();
	bComputePSOSet = false;
	TrackerInstance.ResetAllUAVState();
	StaticUniformBuffers.Reset();
}

FValidationContext::FValidationContext(EType InType)
	: Type(InType)
{
	State.Reset();
	Tracker = &State.TrackerInstance;
}

void FValidationContext::RHIBeginFrame()
{
	++State.BeginEndFrameCounter;
	if (State.BeginEndFrameCounter != 1)
	{
		ensureMsgf(0, TEXT("RHIBeginFrame called twice in a row! Previous callstack: (void**)0x%p,32"), State.PreviousBeginFrame);
	}
	delete [] (uint64*)State.PreviousBeginFrame;
	State.PreviousBeginFrame = RHIValidation::CaptureBacktrace();

	State.Reset();
	RHIContext->RHIBeginFrame();
}

void FValidationContext::RHIEndFrame()
{
	RHIContext->RHIEndFrame();

	// The RHI thread should always be updated at its own frequency (called from RHI thread if available)
	// The RenderThread FrameID is update in RHIAdvanceFrameFence which is called on the RenderThread
	FValidationRHI* ValidateRHI = (FValidationRHI*)GDynamicRHI;
	ValidateRHI->RHIThreadFrameID++;

	--State.BeginEndFrameCounter;
	if (State.BeginEndFrameCounter != 0)
	{
		ensureMsgf(0, TEXT("RHIEndFrame called twice in a row! Previous callstack: (void**)0x%p,32"), State.PreviousEndFrame);
		State.BeginEndFrameCounter = 0;
	}
	delete [] (uint64*)State.PreviousEndFrame;
	State.PreviousEndFrame = RHIValidation::CaptureBacktrace();
}

namespace RHIValidation
{
	static inline FString GetReasonString_SourceCopyFlagMissing(FRHIBuffer* Buffer)
	{
		return FString::Printf(TEXT("Buffers used as copy source need to be created with BUF_SourceCopy! Resource: \"%s\" (0x%p)."), 
			(Buffer->GetName().GetStringLength() > 0) ? *Buffer->GetName().ToString() : TEXT("Unnamed"), Buffer);
	}
}

void FValidationContext::RHICopyToStagingBuffer(FRHIBuffer* SourceBufferRHI, FRHIStagingBuffer* DestinationStagingBufferRHI, uint32 InOffset, uint32 InNumBytes)
{
	using namespace RHIValidation;
	Tracker->Assert(SourceBufferRHI->GetWholeResourceIdentity(), ERHIAccess::CopySrc);
	if (GRHIValidateBufferSourceCopy)
	{
		RHI_VALIDATION_CHECK(EnumHasAnyFlags(SourceBufferRHI->GetUsage(), BUF_SourceCopy), *GetReasonString_SourceCopyFlagMissing(SourceBufferRHI));
	}
	RHIContext->RHICopyToStagingBuffer(SourceBufferRHI, DestinationStagingBufferRHI, InOffset, InNumBytes);
}

void FValidationComputeContext::RHICopyToStagingBuffer(FRHIBuffer* SourceBufferRHI, FRHIStagingBuffer* DestinationStagingBufferRHI, uint32 InOffset, uint32 InNumBytes)
{
	using namespace RHIValidation;
	Tracker->Assert(SourceBufferRHI->GetWholeResourceIdentity(), ERHIAccess::CopySrc);
	if (GRHIValidateBufferSourceCopy)
	{
		RHI_VALIDATION_CHECK(EnumHasAnyFlags(SourceBufferRHI->GetUsage(), BUF_SourceCopy), *GetReasonString_SourceCopyFlagMissing(SourceBufferRHI));
	}
	RHIContext->RHICopyToStagingBuffer(SourceBufferRHI, DestinationStagingBufferRHI, InOffset, InNumBytes);
}

void FValidationContext::FState::Reset()
{
	bInsideBeginRenderPass = false;
	bGfxPSOSet = false;
	RenderPassName.Reset();
	PreviousRenderPassName.Reset();
	ComputePassName.Reset();
	bComputePSOSet = false;
	TrackerInstance.ResetAllUAVState();
	StaticUniformBuffers.Reset();
}

namespace RHIValidation
{
	void FStaticUniformBuffers::Reset()
	{
		Bindings.Reset();
		check(!bInSetPipelineStateCall);
	}

	void FStaticUniformBuffers::ValidateSetShaderUniformBuffer(FRHIUniformBuffer* UniformBuffer)
	{
		check(UniformBuffer);
		UniformBuffer->ValidateLifeTime();

		// Skip validating global uniform buffers that are set internally by the RHI as part of the pipeline state.
		if (bInSetPipelineStateCall)
		{
			return;
		}

		const FRHIUniformBufferLayout& Layout = UniformBuffer->GetLayout();

		checkf(EnumHasAnyFlags(Layout.BindingFlags, EUniformBufferBindingFlags::Shader), TEXT("Uniform buffer '%s' does not have the 'Shader' binding flag."), *Layout.GetDebugName());

		if (Layout.StaticSlot < Bindings.Num())
		{
			check(Layout.BindingFlags == EUniformBufferBindingFlags::StaticAndShader);

			ensureMsgf(Bindings[Layout.StaticSlot] == nullptr,
				TEXT("Uniform buffer '%s' was bound statically and is now being bound on a specific RHI shader. Only one binding model should be used at a time."),
				*Layout.GetDebugName());
		}
	}

	ERHIAccess DecayResourceAccess(ERHIAccess AccessMask, ERHIAccess RequiredAccess, bool bAllowUAVOverlap)
	{
		using T = __underlying_type(ERHIAccess);
		checkf((T(RequiredAccess) & (T(RequiredAccess) - 1)) == 0, TEXT("Only one required access bit may be set at once."));
		
		if (EnumHasAnyFlags(RequiredAccess, ERHIAccess::UAVMask | ERHIAccess::BVHWrite))
		{
			// UAV writes decay to no allowed resource access when overlaps are disabled. A barrier is always required after the dispatch/draw.
			// Otherwise keep the same accessmask and don't touch or decay the state
			return !bAllowUAVOverlap ? ERHIAccess::None : AccessMask;
		}

		// Handle DSV modes
		if (EnumHasAnyFlags(RequiredAccess, ERHIAccess::DSVWrite))
		{
			constexpr ERHIAccess CompatibleStates =
				ERHIAccess::DSVRead |
				ERHIAccess::DSVWrite;

			return AccessMask & CompatibleStates;
		}
		if (EnumHasAnyFlags(RequiredAccess, ERHIAccess::DSVRead))
		{
			constexpr ERHIAccess CompatibleStates =
				ERHIAccess::DSVRead |
				ERHIAccess::DSVWrite |
				ERHIAccess::SRVGraphics |
				ERHIAccess::SRVCompute |
				ERHIAccess::CopySrc;

			return AccessMask & CompatibleStates;
		}

		if (EnumHasAnyFlags(RequiredAccess, ERHIAccess::WritableMask))
		{
			// Decay to only 1 allowed state for all other writable states.
			return RequiredAccess;
		}

		// Else, the state is readable. All readable states are compatible.
		return AccessMask;
	}

#define BARRIER_TRACKER_LOG_PREFIX_REASON(ReasonString) TEXT("RHI validation failed: " ReasonString ":\n\n")\
	TEXT("--------------------------------------------------------------------\n")\
	TEXT("              RHI Resource Transition Validation Error              \n")\
	TEXT("--------------------------------------------------------------------\n")\
	TEXT("\n\n")
	
// Warning: this prefix expects a string argument for the resource name, make sure you add it.
#define BARRIER_TRACKER_LOG_PREFIX_RESNAME TEXT("RHI validation failed for resource: %s:\n\n")\
	TEXT("--------------------------------------------------------------------\n")\
	TEXT("              RHI Resource Transition Validation Error              \n")\
	TEXT("--------------------------------------------------------------------\n")\
	TEXT("\n\n")

#define BARRIER_TRACKER_LOG_SUFFIX TEXT("\n\n")\
	TEXT("--------------------------------------------------------------------\n")\
	TEXT("\n\n")

#define BARRIER_TRACKER_LOG_ENABLE_TRANSITION_BACKTRACE \
	TEXT("    --- Enable barrier logging for this resource to see a callstack backtrace for the RHIBeginTransitions() call ") \
	TEXT("which has not been completed. Use -RHIValidationLog=X,Y,Z to enable backtrace logging for individual resources.\n\n")

	static inline FString GetResourceDebugName(FResource const* Resource, FSubresourceIndex const& SubresourceIndex)
	{
		const TCHAR* DebugName = Resource->GetDebugName();
		if (!DebugName)
		{
			DebugName = TEXT("Unnamed");
		}

		if (SubresourceIndex.IsWholeResource())
		{
			return FString::Printf(
				TEXT("\"%s\" (0x%p) (Whole Resource)"),
				DebugName,
				Resource);
		}
		else
		{
			return FString::Printf(
				TEXT("\"%s\" (0x%p) (Mip %d, Slice %d, Plane %d)"),
				DebugName,
				Resource, 
				SubresourceIndex.MipIndex,
				SubresourceIndex.ArraySlice,
				SubresourceIndex.PlaneIndex);
		}
	}

	static inline FString GetReasonString_MissingBarrier(
		FResource* Resource, FSubresourceIndex const& SubresourceIndex, 
		const FState& CurrentState,
		const FState& RequiredState)
	{
		FString DebugName = GetResourceDebugName(Resource, SubresourceIndex);
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to access resource %s from a hardware unit it is not currently accessible from. A resource transition is required.\n\n")
			TEXT("    --- Allowed access states for this resource are: %s\n")
			TEXT("    --- Required access states are:                  %s\n")
			TEXT("    --- Allowed pipelines for this resource are:     %s\n")
			TEXT("    --- Required pipelines are:                      %s\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName,
			*GetRHIAccessName(CurrentState.Access),
			*GetRHIAccessName(RequiredState.Access),
			*GetRHIPipelineName(CurrentState.Pipelines),
			*GetRHIPipelineName(RequiredState.Pipelines));
	}

	static inline FString GetReasonString_IncorrectTrackedAccess(
		FResource* Resource, FSubresourceIndex const& SubresourceIndex,
		const FState& CurrentState,
		const FState& TrackedState)
	{
		FString DebugName = GetResourceDebugName(Resource, SubresourceIndex);
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to assign resource %s a tracked access that does not match its validation tracked access.\n\n")
			TEXT("    --- Actual access states:                    %s\n")
			TEXT("    --- Assigned access states:                  %s\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName,
			*GetRHIAccessName(CurrentState.Access),
			*GetRHIAccessName(TrackedState.Access));
	}

	static inline FString GetReasonString_BeginBacktrace(void* CreateTrace, void* BeginTrace)
	{
		if (CreateTrace || BeginTrace)
		{
			return FString::Printf(
				TEXT("    --- Callstack backtraces for the transition which has not been completed (resolve in the Watch window):\n")
				TEXT("        RHICreateTransition: (void**)0x%p,32\n")
				TEXT("        RHIBeginTransitions: (void**)0x%p,32\n"),
				CreateTrace,
				BeginTrace);
		}
		else
		{
			return BARRIER_TRACKER_LOG_ENABLE_TRANSITION_BACKTRACE;
		}
	}

	static inline FString GetReasonString_Backtrace(const TCHAR* OperationPrefix, const TCHAR* TracePrefix, void* Trace)
	{
		if (Trace)
		{
			return FString::Printf(
				TEXT("    --- Callstack backtrace for %s operation (resolve in the Watch window):\n")
				TEXT("        %s: (void**)0x%p,32\n"),
				OperationPrefix,
				TracePrefix,
				Trace);
		}
		else
		{
			return FString(BARRIER_TRACKER_LOG_ENABLE_TRANSITION_BACKTRACE);
		}
	}

	static inline FString GetReasonString_DuplicateBackTrace(void* PreviousTrace, void* CurrentTrace)
	{
		if (PreviousTrace || CurrentTrace)
		{
			return
				GetReasonString_Backtrace(TEXT("previous"), TEXT("RHICreateTransition"), PreviousTrace) +
				GetReasonString_Backtrace(TEXT("current"), TEXT("RHICreateTransition"), CurrentTrace);
		}
		else
		{
			return BARRIER_TRACKER_LOG_ENABLE_TRANSITION_BACKTRACE;
		}
	}

	static inline FString GetReasonString_AccessDuringTransition(
		FResource* Resource, FSubresourceIndex const& SubresourceIndex, 
		const FState& PendingState,
		const FState& AttemptedState,
		void* CreateTrace, void* BeginTrace)
	{
		FString DebugName = GetResourceDebugName(Resource, SubresourceIndex);
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to access resource %s whilst an asynchronous resource transition is in progress. A call to RHIEndTransitions() must be made before the resource can be accessed again.\n\n")
			TEXT("    --- Pending access states for this resource are: %s\n")
			TEXT("    --- Attempted access states are:                 %s\n")
			TEXT("    --- Pending pipelines for this resource are:     %s\n")
			TEXT("    --- Attempted pipelines are:                     %s\n")
			TEXT("%s")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName,
			*GetRHIAccessName(PendingState.Access),
			*GetRHIAccessName(AttemptedState.Access),
			*GetRHIPipelineName(PendingState.Pipelines),
			*GetRHIPipelineName(AttemptedState.Pipelines),
			*GetReasonString_BeginBacktrace(CreateTrace, BeginTrace));
	}

	static inline FString GetReasonString_TransitionWithoutAcquire(FResource* Resource)
	{
		FString DebugName = GetResourceDebugName(Resource, {});
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted a resource transition for transient resource %s without acquiring it. Transient resources must be acquired before any transitions are begun and discarded after all transitions are complete.\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName);
	}

	static inline FString GetReasonString_AcquireNonTransient(FResource* Resource)
	{
		FString DebugName = GetResourceDebugName(Resource, {});
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to acquire non-transient resource %s. Only transient resources may be acquired with the transient aliasing API.\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName);
	}

	static inline FString GetReasonString_DiscardNonTransient(FResource* Resource)
	{
		FString DebugName = GetResourceDebugName(Resource, {});
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to discard non-transient resource %s. Only transient resources may be discarded with the transient aliasing API.\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName);
	}

	static inline FString GetReasonString_AliasingOverlapNonDiscarded(FResource* ResourceBefore, FResource* ResourceAfter, void* CreateTrace)
	{
		FString DebugNameBefore = GetResourceDebugName(ResourceBefore, {});
		FString DebugNameAfter = GetResourceDebugName(ResourceAfter, {});
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to overlap resource %s (before) with resource %s (after), but %s (before) has not been discarded.\n")
			TEXT("%s")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugNameAfter,
			*DebugNameBefore,
			*DebugNameAfter,
			*DebugNameBefore,
			*GetReasonString_Backtrace(TEXT("acquire"), TEXT("RHICreateTransition"), CreateTrace));
	}

	static inline FString GetReasonString_AliasingOverlapNonTransient(FResource* ResourceBefore, FResource* ResourceAfter)
	{
		FString DebugNameBefore = GetResourceDebugName(ResourceBefore, {});
		FString DebugNameAfter = GetResourceDebugName(ResourceAfter, {});
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to overlap non-transient resource %s when acquiring resource %s. Only transient resources may be used in an aliasing overlap operation.\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugNameBefore,
			*DebugNameBefore,
			*DebugNameAfter);
	}

	static inline FString GetReasonString_DuplicateAcquireTransient(FResource* Resource, void* PreviousAcquireTrace, void* CurrentAcquireTrace)
	{
		FString DebugName = GetResourceDebugName(Resource, {});
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Mismatched acquire of transient resource %s. A transient resource may only be acquired once in its lifetime.\n")
			TEXT("%s")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName,
			*GetReasonString_DuplicateBackTrace(PreviousAcquireTrace, CurrentAcquireTrace));
	}

	static inline FString GetReasonString_DuplicateDiscardTransient(FResource* Resource, void* PreviousDiscardTrace, void* CurrentDiscardTrace)
	{
		FString DebugName = GetResourceDebugName(Resource, {});
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Mismatched discard of transient resource %s. A transient resource may only be discarded once in its lifetime.\n")
			TEXT("%s")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName,
			*GetReasonString_DuplicateBackTrace(PreviousDiscardTrace, CurrentDiscardTrace));
	}

	static inline FString GetReasonString_DiscardWithoutAcquireTransient(FResource* Resource, void* DiscardTrace)
	{
		FString DebugName = GetResourceDebugName(Resource, {});
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to discard transient resource %s, but it was never acquired.\n")
			TEXT("%s")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName,
			*GetReasonString_Backtrace(TEXT("acquire"), TEXT("RHICreateTransition"), DiscardTrace));
	}

	static inline FString GetReasonString_DuplicateBeginTransition(
		FResource* Resource, FSubresourceIndex const& SubresourceIndex,
		const FState& PendingState,
		const FState& TargetState,
		void* CreateTrace, void* BeginTrace)
	{
		FString DebugName = GetResourceDebugName(Resource, SubresourceIndex);
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to begin a resource transition for resource %s whilst a previous asynchronous resource transition is already in progress. A call to RHIEndTransitions() must be made before the resource can be transitioned again.\n\n")
			TEXT("    --- Pending access states for this resource are:              %s\n")
			TEXT("    --- Attempted access states for the duplicate transition are: %s\n")
			TEXT("    --- Pending pipelines for this resource are:                  %s\n")
			TEXT("    --- Attempted pipelines for the duplicate transition are:     %s\n")
			TEXT("%s")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName,
			*GetRHIAccessName(PendingState.Access),
			*GetRHIAccessName(TargetState.Access),
			*GetRHIPipelineName(PendingState.Pipelines),
			*GetRHIPipelineName(TargetState.Pipelines),
			*GetReasonString_BeginBacktrace(CreateTrace, BeginTrace));
	}

	static inline FString GetReasonString_WrongPipeline(
		FResource* Resource, FSubresourceIndex const& SubresourceIndex,
		const FState& ActualCurrentState,
		const FState& CurrentStateFromRHI)
	{
		FString DebugName = GetResourceDebugName(Resource, SubresourceIndex);
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to begin a resource transition for resource %s on the wrong pipeline(s) (\"%s\"). The resource is currently accessible on the \"%s\" pipeline(s).\n\n")
			TEXT("    --- Current access states for this resource are: %s\n")
			TEXT("    --- Attempted access states are:                 %s\n\n")
			TEXT("    --- Ensure that resource transitions are issued on the correct pipeline.\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName,
			*GetRHIPipelineName(CurrentStateFromRHI.Pipelines),
			*GetRHIPipelineName(ActualCurrentState.Pipelines),
			*GetRHIAccessName(ActualCurrentState.Access),
			*GetRHIAccessName(CurrentStateFromRHI.Access));
	}

	static inline FString GetReasonString_IncorrectPreviousExplicitState(
		FResource* Resource, FSubresourceIndex const& SubresourceIndex,
		const FState& CurrentState,
		const FState& CurrentStateFromRHI)
	{
		FString DebugName = GetResourceDebugName(Resource, SubresourceIndex);
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("The explicit previous state \"%s\" does not match the tracked current state \"%s\" for the resource %s.\n")
			TEXT("    --- Allowed pipelines for this resource are:                           %s\n")
			TEXT("    --- Previous pipelines passed as part of the resource transition were: %s\n\n")
			TEXT("    --- The best solution is to correct the explicit previous state passed for the resource in the call to RHICreateTransition().\n")
			TEXT("    --- Alternatively, use ERHIAccess::Unknown if the actual previous state cannot be determined. Unknown previous resource states have a performance impact so should be avoided if possible.\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*GetRHIAccessName(CurrentStateFromRHI.Access),
			*GetRHIAccessName(CurrentState.Access),
			*DebugName,
			*GetRHIPipelineName(CurrentState.Pipelines),
			*GetRHIPipelineName(CurrentStateFromRHI.Pipelines));
	}

	static inline FString GetReasonString_IncorrectPreviousTrackedState(
		FResource* Resource, FSubresourceIndex const& SubresourceIndex,
		const FState& CurrentState,
		ERHIPipeline PipelineFromRHI)
	{
		FString DebugName = GetResourceDebugName(Resource, SubresourceIndex);
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("The tracked previous state \"%s\" does not match the tracked current state \"%s\" for the resource %s.\n")
			TEXT("    --- Allowed pipelines for this resource are:                           %s\n")
			TEXT("    --- Previous pipelines passed as part of the resource transition were: %s\n\n")
			TEXT("    --- The previous state was pulled from the last call to RHICmdList.SetTrackedAccess due to the use of ERHIAccess::Unknown. If this doesn't match the expected state, be sure to update the \n")
			TEXT("    --- tracked state after using manual low - level transitions. It is highly recommended to coalesce all subresources into the same state before relying on tracked previous states with \n")
			TEXT("    --- ERHIAccess::Unknown. RHICmdList.SetTrackedAccess applies to whole resources.\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*GetRHIAccessName(Resource->GetTrackedAccess()),
			*GetRHIAccessName(CurrentState.Access),
			*DebugName,
			*GetRHIPipelineName(CurrentState.Pipelines),
			*GetRHIPipelineName(PipelineFromRHI));
	}

	static inline FString GetReasonString_MismatchedEndTransition(
		FResource* Resource, FSubresourceIndex const& SubresourceIndex,
		const FState& TargetState,
		const FState& TargetStateFromRHI)
	{
		FString DebugName = GetResourceDebugName(Resource, SubresourceIndex);
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("The expected target state \"%s\" on pipe \"%s\" in end transition does not match the tracked target state \"%s\" on pipe \"%s\" for the resource %s.\n")
			TEXT("    --- The call to EndTransition() is mismatched with the another BeginTransition() with different states.\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*GetRHIAccessName(TargetStateFromRHI.Access),
			*GetRHIPipelineName(TargetState.Pipelines),
			*GetRHIAccessName(TargetState.Access),
			*GetRHIPipelineName(TargetStateFromRHI.Pipelines),
			*DebugName);
	}

	static inline FString GetReasonString_UnnecessaryTransition(
		FResource* Resource, FSubresourceIndex const& SubresourceIndex,
		const FState& CurrentState)
	{
		FString DebugName = GetResourceDebugName(Resource, SubresourceIndex);
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to begin a resource transition for the resource %s to the \"%s\" state on the \"%s\" pipe, but the resource is already in this state. The resource transition is unnecessary.\n")
			TEXT("    --- This is not fatal, but does have an effect on CPU and GPU performance. Consider refactoring rendering code to avoid unnecessary resource transitions.\n")
			TEXT("    --- RenderGraph (RDG) is capable of handling resource transitions automatically.\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName,
			*GetRHIAccessName(CurrentState.Access),
			*GetRHIPipelineName(CurrentState.Pipelines));
	}

	static inline FString GetReasonString_MismatchedAllUAVsOverlapCall(bool bAllow)
	{
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_REASON("UAV overlap mismatch")
			TEXT("Mismatched call to %sUAVOverlap.\n\n")
			TEXT("    --- Ensure all calls to RHICmdList.BeginUAVOverlap() are paired with a call to RHICmdList.EndUAVOverlap().\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			bAllow ? TEXT("Begin") : TEXT("End")
		);
	}

	static inline FString GetReasonString_MismatchedExplicitUAVOverlapCall(
		FResource* Resource, FSubresourceIndex const& SubresourceIndex,
		bool bAllow)
	{
		FString DebugName = GetResourceDebugName(Resource, SubresourceIndex);
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_REASON("UAV overlap mismatch")
			TEXT("Mismatched call to %sUAVOverlap(FRHIUnorderedAccessView*) for the resource %s.\n\n")
			TEXT("    --- Ensure all calls to RHICmdList.BeginUAVOverlap() are paired with a call to RHICmdList.EndUAVOverlap().\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			bAllow ? TEXT("Begin") : TEXT("End"),
			*DebugName
		);
	}

	static inline FString GetReasonString_UAVOverlap(
		FResource* Resource, FSubresourceIndex const& SubresourceIndex,
		const FState& CurrentState, const FState& RequiredState)
	{
		FString DebugName = GetResourceDebugName(Resource, SubresourceIndex);
		return FString::Printf(
			BARRIER_TRACKER_LOG_PREFIX_RESNAME
			TEXT("Attempted to access resource %s which was previously used with overlapping UAV access, but has not been transitioned since UAV overlap was disabled. A resource transition is required.\n\n")
			TEXT("    --- Allowed access states for this resource are: %s\n")
			TEXT("    --- Required access states are:                  %s\n")
			TEXT("    --- Allowed pipelines for this resource are:     %s\n")
			TEXT("    --- Required pipelines are:                      %s\n")
			BARRIER_TRACKER_LOG_SUFFIX,
			*DebugName,
			*DebugName,
			*GetRHIAccessName(CurrentState.Access),
			*GetRHIAccessName(RequiredState.Access),
			*GetRHIPipelineName(CurrentState.Pipelines),
			*GetRHIPipelineName(RequiredState.Pipelines));
	}

	static inline void* Log(FResource* Resource, FSubresourceIndex const& SubresourceIndex, void* CreateTrace, const TCHAR* TracePrefix, const TCHAR* Type, const TCHAR* LogStr)
	{
		void* Trace = CaptureBacktrace();

		FString BreadcrumbMessage = GetBreadcrumbPath();

		if (CreateTrace)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\n%s: Type: %s, %s, CreateTrace: 0x%p, %sTrace: 0x%p, %s\n"),
				*GetResourceDebugName(Resource, SubresourceIndex),
				Type,
				LogStr,
				CreateTrace,
				TracePrefix,
				Trace,
				*BreadcrumbMessage);
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\n%s: Type: %s, %s, Trace: 0x%p, %s\n"),
				*GetResourceDebugName(Resource, SubresourceIndex),
				Type,
				LogStr,
				Trace,
				*BreadcrumbMessage);
		}

		return Trace;
	}

	void FTransientState::Acquire(FResource* Resource, void* CreateTrace)
	{
		RHI_VALIDATION_CHECK(bTransient, *GetReasonString_AcquireNonTransient(Resource));
		RHI_VALIDATION_CHECK(Status == EStatus::None, *GetReasonString_DuplicateAcquireTransient(Resource, AcquireBacktrace, CreateTrace));
		Status = EStatus::Acquired;

		if (!AcquireBacktrace)
		{
			AcquireBacktrace = CreateTrace;
		}

		if (Resource->LoggingMode != ELoggingMode::None)
		{
			Log(Resource, {}, CreateTrace, TEXT("Acquire"), TEXT("Acquire"), TEXT("Transient Acquire"));
		}
	}

	void FTransientState::Discard(FResource* Resource, void* CreateTrace)
	{
		RHI_VALIDATION_CHECK(bTransient, *GetReasonString_DiscardNonTransient(Resource));

		RHI_VALIDATION_CHECK(Status != EStatus::None, *GetReasonString_DiscardWithoutAcquireTransient(Resource, CreateTrace));
		RHI_VALIDATION_CHECK(Status != EStatus::Discarded, *GetReasonString_DuplicateDiscardTransient(Resource, DiscardBacktrace, CreateTrace));
		Status = EStatus::Discarded;

		if (!DiscardBacktrace)
		{
			DiscardBacktrace = CreateTrace;
		}

		if (Resource->LoggingMode != ELoggingMode::None)
		{
			Log(Resource, {}, CreateTrace, TEXT("Discard"), TEXT("Discard"), TEXT("Transient Discard"));
		}
	}

	void FTransientState::AliasingOverlap(FResource* ResourceBefore, FResource* ResourceAfter, void* CreateTrace)
	{
		FTransientState& TransientStateBefore = ResourceBefore->TransientState;
		FTransientState& TransientStateAfter = ResourceAfter->TransientState;

		// Acquire should validate whether ResourceAfter is transient. We assume it is here.
		RHI_VALIDATION_CHECK(TransientStateBefore.bTransient, *GetReasonString_AliasingOverlapNonTransient(ResourceBefore, ResourceAfter));
		RHI_VALIDATION_CHECK(TransientStateBefore.IsDiscarded(), *GetReasonString_AliasingOverlapNonDiscarded(ResourceBefore, ResourceAfter, CreateTrace));

		if (ResourceBefore->LoggingMode != ELoggingMode::None)
		{
			Log(ResourceBefore, {}, CreateTrace, TEXT("AliasingOverlap"), TEXT("AliasingOverlap"), TEXT("Aliasing Overlap (Before)"));
		}

		if (ResourceAfter->LoggingMode != ELoggingMode::None)
		{
			Log(ResourceAfter, {}, CreateTrace, TEXT("AliasingOverlap"), TEXT("AliasingOverlap"), TEXT("Aliasing Overlap (After)"));
		}
	}

	void FResource::SetDebugName(const TCHAR* Name, const TCHAR* Suffix)
	{
		DebugName = Suffix
			? FString::Printf(TEXT("%s%s"), Name, Suffix)
			: Name;

		if (LoggingMode != ELoggingMode::Manual)
		{
			// Automatically enable/disable barrier logging if the resource name
			// does/doesn't match one in the AutoLogResourceNames array.
			if (Name)
			{
				for (FString const& Str : GetAutoLogResourceNames())
				{
					if (FCString::Stricmp(Name, *Str) == 0)
					{
						LoggingMode = ELoggingMode::Automatic;
						return;
					}
				}
			}

			LoggingMode = ELoggingMode::None;
		}
	}

	void FSubresourceState::BeginTransition(FResource* Resource, FSubresourceIndex const& SubresourceIndex, const FState& CurrentStateFromRHI, const FState& TargetState, EResourceTransitionFlags NewFlags, ERHIPipeline ExecutingPipeline, void* CreateTrace)
	{
		FPipelineState& State = States[ExecutingPipeline];

		void* BeginTrace = nullptr;
		if (Resource->LoggingMode != ELoggingMode::None 
#if LOG_UNNAMED_RESOURCES
			|| Resource->GetDebugName() == nullptr
#endif
			)
		{
			BeginTrace = Log(Resource, SubresourceIndex, CreateTrace, TEXT("Begin"), TEXT("BeginTransition"), *FString::Printf(TEXT("Current: (%s) New: (%s), Flags: %s, Executing Pipeline: %s"),
				*State.Current.ToString(),
				*TargetState.ToString(),
				*GetResourceTransitionFlagsName(NewFlags),
				*GetRHIPipelineName(ExecutingPipeline)
			));
		}

		if (Resource->TransientState.bTransient)
		{
			RHI_VALIDATION_CHECK(Resource->TransientState.IsAcquired() || (Resource->TransientState.IsDiscarded() && TargetState.Access == ERHIAccess::Discard), *GetReasonString_TransitionWithoutAcquire(Resource));
		}

		// Check we're not already transitioning
		RHI_VALIDATION_CHECK(!State.bTransitioning, *GetReasonString_DuplicateBeginTransition(Resource, SubresourceIndex, State.Current, TargetState, State.CreateTransitionBacktrace, BeginTrace));

		// Validate the explicit previous state from the RHI matches what we expect...
		{
			// Check for the correct pipeline
			RHI_VALIDATION_CHECK(EnumHasAllFlags(CurrentStateFromRHI.Pipelines, ExecutingPipeline), *GetReasonString_WrongPipeline(Resource, SubresourceIndex, State.Current, TargetState));

			if (CurrentStateFromRHI.Access == ERHIAccess::Unknown)
			{
				RHI_VALIDATION_CHECK(Resource->TrackedAccess == State.Previous.Access && CurrentStateFromRHI.Pipelines == State.Previous.Pipelines,
					*GetReasonString_IncorrectPreviousTrackedState(Resource, SubresourceIndex, State.Previous, CurrentStateFromRHI.Pipelines));
			}
			else
			{
				// Check the current RHI state passed in matches the tracked state for the resource.
				RHI_VALIDATION_CHECK(CurrentStateFromRHI.Access == State.Previous.Access && CurrentStateFromRHI.Pipelines == State.Previous.Pipelines,
					*GetReasonString_IncorrectPreviousExplicitState(Resource, SubresourceIndex, State.Previous, CurrentStateFromRHI));
			}
		}

		// Check for unnecessary transitions
		// @todo: this check is not particularly useful at the moment, as there are many unnecessary resource transitions.
		//RHI_VALIDATION_CHECK(CurrentState != TargetState, *GetReasonString_UnnecessaryTransition(Resource, SubresourceIndex, CurrentState));

		// Update the tracked state once all pipes have begun.
		State.Previous = TargetState;
		State.Current = TargetState;
		State.Flags = NewFlags;
		State.CreateTransitionBacktrace = CreateTrace;
		State.BeginTransitionBacktrace = BeginTrace;
		State.bUsedWithAllUAVsOverlap = false;
		State.bUsedWithExplicitUAVsOverlap = false;
		State.bTransitioning = true;

		// Replicate the state to other pipes that are not part of the begin pipe mask.
		for (ERHIPipeline OtherPipeline : GetRHIPipelines())
		{
			if (!EnumHasAnyFlags(CurrentStateFromRHI.Pipelines, OtherPipeline))
			{
				States[OtherPipeline] = State;
			}
		}
	}

	void FSubresourceState::EndTransition(FResource* Resource, FSubresourceIndex const& SubresourceIndex, const FState& CurrentStateFromRHI, const FState& TargetState, ERHIPipeline ExecutingPipeline, void* CreateTrace)
	{
		if (Resource->LoggingMode != ELoggingMode::None
#if LOG_UNNAMED_RESOURCES
			|| Resource->GetDebugName() == nullptr
#endif
			)
		{
			Log(Resource, SubresourceIndex, CreateTrace, TEXT("End"), TEXT("EndTransition"), *FString::Printf(TEXT("Access: %s, Pipeline: %s, Executing Pipeline: %s"),
				*GetRHIAccessName(TargetState.Access),
				*GetRHIPipelineName(TargetState.Pipelines),
				*GetRHIPipelineName(ExecutingPipeline)
			));
		}

		FPipelineState& State = States[ExecutingPipeline];

		// Check that we aren't ending a transition that never began.
		RHI_VALIDATION_CHECK(State.bTransitioning, TEXT("Unsolicited resource end transition call."));
		State.bTransitioning = false;
		State.BeginTransitionBacktrace = nullptr;

		if (Resource->TransientState.bTransient)
		{
			RHI_VALIDATION_CHECK(Resource->TransientState.IsAcquired() || (Resource->TransientState.IsDiscarded() && TargetState.Access == ERHIAccess::Discard), *GetReasonString_TransitionWithoutAcquire(Resource));
		}

		// Check that the end matches the begin.
		RHI_VALIDATION_CHECK(TargetState == State.Current, *GetReasonString_MismatchedEndTransition(Resource, SubresourceIndex, State.Current, TargetState));

		// Replicate the state to other pipes that are not part of the end pipe mask.
		for (ERHIPipeline OtherPipeline : GetRHIPipelines())
		{
			if (!EnumHasAnyFlags(TargetState.Pipelines, OtherPipeline))
			{
				States[OtherPipeline] = State;
			}
		}
	}

	void FSubresourceState::Assert(FResource* Resource, FSubresourceIndex const& SubresourceIndex, const FState& RequiredState, bool bAllowAllUAVsOverlap)
	{
		if (Resource->LoggingMode != ELoggingMode::None
#if LOG_UNNAMED_RESOURCES
			|| Resource->GetDebugName() == nullptr
#endif
			)
		{
			Log(Resource, SubresourceIndex, nullptr, nullptr, TEXT("Assert"), *FString::Printf(TEXT("Access: %s, Pipeline: %s"), 
				*GetRHIAccessName(RequiredState.Access),
				*GetRHIPipelineName(RequiredState.Pipelines)));
		}

		FPipelineState& State = States[RequiredState.Pipelines];

		// Check we're not trying to access the resource whilst a pending resource transition is in progress.
		RHI_VALIDATION_CHECK(!State.bTransitioning, *GetReasonString_AccessDuringTransition(Resource, SubresourceIndex, State.Current, RequiredState, State.CreateTransitionBacktrace, State.BeginTransitionBacktrace));

		// If UAV overlaps are now disabled, ensure the resource has been transitioned if it was previously used in UAV overlap state.
		RHI_VALIDATION_CHECK((bAllowAllUAVsOverlap || !State.bUsedWithAllUAVsOverlap) && (State.bExplicitAllowUAVOverlap || !State.bUsedWithExplicitUAVsOverlap), *GetReasonString_UAVOverlap(Resource, SubresourceIndex, State.Current, RequiredState));

		// Ensure the resource is in the required state for this operation
		RHI_VALIDATION_CHECK(EnumHasAllFlags(State.Current.Access, RequiredState.Access) && EnumHasAllFlags(State.Current.Pipelines, RequiredState.Pipelines), *GetReasonString_MissingBarrier(Resource, SubresourceIndex, State.Current, RequiredState));

		State.Previous = State.Current;

		if (EnumHasAnyFlags(RequiredState.Access, ERHIAccess::UAVMask | ERHIAccess::BVHWrite))
		{
			if (bAllowAllUAVsOverlap) { State.bUsedWithAllUAVsOverlap = true; }
			if (State.bExplicitAllowUAVOverlap) { State.bUsedWithExplicitUAVsOverlap = true; }
		}

		// Disable all non-compatible access types
		State.Current.Access = DecayResourceAccess(State.Current.Access, RequiredState.Access, bAllowAllUAVsOverlap || State.bExplicitAllowUAVOverlap);
	}

	void FSubresourceState::AssertTracked(FResource* Resource, FSubresourceIndex const& SubresourceIndex, const FState& RequiredState)
	{
		if (Resource->LoggingMode != ELoggingMode::None
#if LOG_UNNAMED_RESOURCES
			|| Resource->GetDebugName() == nullptr
#endif
			)
		{
			Log(Resource, SubresourceIndex, nullptr, nullptr, TEXT("AssertTracked"), *FString::Printf(TEXT("Access: %s, Pipeline %s"), *GetRHIAccessName(RequiredState.Access), *GetRHIPipelineName(RequiredState.Pipelines)));
		}

		FPipelineState& State = States[RequiredState.Pipelines];

		// Check we're not trying to access the resource whilst a pending resource transition is in progress.
		RHI_VALIDATION_CHECK(!State.bTransitioning, *GetReasonString_AccessDuringTransition(Resource, SubresourceIndex, State.Current, RequiredState, State.CreateTransitionBacktrace, State.BeginTransitionBacktrace));

		// Ensure the resource is in the required state for this operation
		RHI_VALIDATION_CHECK(State.Current.Access == RequiredState.Access, *GetReasonString_IncorrectTrackedAccess(Resource, SubresourceIndex, State.Current, RequiredState));
	}

	void FSubresourceState::SpecificUAVOverlap(FResource* Resource, FSubresourceIndex const& SubresourceIndex, ERHIPipeline Pipeline, bool bAllow)
	{
		if (Resource->LoggingMode != ELoggingMode::None
#if LOG_UNNAMED_RESOURCES
			|| Resource->GetDebugName() == nullptr
#endif
			)
		{
			Log(Resource, SubresourceIndex, nullptr, nullptr, TEXT("UAVOverlap"), *FString::Printf(TEXT("Allow: %s"), bAllow ? TEXT("True") : TEXT("False")));
		}

		FPipelineState& State = States[Pipeline];
		RHI_VALIDATION_CHECK(State.bExplicitAllowUAVOverlap != bAllow, *GetReasonString_MismatchedExplicitUAVOverlapCall(Resource, SubresourceIndex, bAllow));
		State.bExplicitAllowUAVOverlap = bAllow;
	}

	inline void FResource::EnumerateSubresources(FSubresourceRange const& SubresourceRange, TFunctionRef<void(FSubresourceState&, FSubresourceIndex const&)> Callback, bool bBeginTransition)
	{
		bool bWholeResource = SubresourceRange.IsWholeResource(*this);
		if (bWholeResource && SubresourceStates.Num() == 0)
		{
			Callback(WholeResourceState, FSubresourceIndex());
		}
		else
		{
			if (SubresourceStates.Num() == 0)
			{
				const uint32 NumSubresources = NumMips * NumArraySlices * NumPlanes;
				SubresourceStates.Reserve(NumSubresources);

				// Copy the whole resource state into all the subresource slots
				for (uint32 Index = 0; Index < NumSubresources; ++Index)
				{
					SubresourceStates.Add(WholeResourceState);
				}
			}

			uint32 LastMip = SubresourceRange.MipIndex + SubresourceRange.NumMips;
			uint32 LastArraySlice = SubresourceRange.ArraySlice + SubresourceRange.NumArraySlices;
			uint32 LastPlaneIndex = SubresourceRange.PlaneIndex + SubresourceRange.NumPlanes;

			for (uint32 PlaneIndex = SubresourceRange.PlaneIndex; PlaneIndex < LastPlaneIndex; ++PlaneIndex)
			{
				for (uint32 MipIndex = SubresourceRange.MipIndex; MipIndex < LastMip; ++MipIndex)
				{
					for (uint32 ArraySlice = SubresourceRange.ArraySlice; ArraySlice < LastArraySlice; ++ArraySlice)
					{
						uint32 SubresourceIndex = PlaneIndex + (MipIndex + ArraySlice * NumMips) * NumPlanes;
						Callback(SubresourceStates[SubresourceIndex], FSubresourceIndex(MipIndex, ArraySlice, PlaneIndex));
					}
				}
			}
		}

		if (bWholeResource && bBeginTransition && SubresourceStates.Num() != 0)
		{
			// Switch back to whole resource state tracking on begin transitions
			WholeResourceState = SubresourceStates[0];
			SubresourceStates.Reset();
		}
	}

	EReplayStatus FOperation::Replay(ERHIPipeline Pipeline, bool& bAllowAllUAVsOverlap, FBreadcrumbStack& Breadcrumbs) const
	{
		switch (Type)
		{
		case EOpType::PushBreadcrumb:
			Breadcrumbs.Push(Data_PushBreadcrumb.Breadcrumb);
			return EReplayStatus::Normal;

		case EOpType::PopBreadcrumb:
			if (!Breadcrumbs.IsEmpty())
			{
				delete[] Breadcrumbs.Last();
				Breadcrumbs.Pop();
			}
			return EReplayStatus::Normal;
		}

		FRHIValidationBreadcrumbScope BreadcrumbScope(Breadcrumbs);

		switch (Type)
		{
		case EOpType::Rename:
			Data_Rename.Resource->SetDebugName(Data_Rename.DebugName, Data_Rename.Suffix);
			delete[] Data_Rename.DebugName;
			Data_Rename.Resource->ReleaseOpRef();
			break;

		case EOpType::BeginTransition:
			Data_BeginTransition.Identity.Resource->EnumerateSubresources(Data_BeginTransition.Identity.SubresourceRange, [this, Pipeline](FSubresourceState& State, FSubresourceIndex const& SubresourceIndex)
			{
				State.BeginTransition(
					Data_BeginTransition.Identity.Resource,
					SubresourceIndex,
					Data_BeginTransition.PreviousState,
					Data_BeginTransition.NextState,
					Data_BeginTransition.Flags,
					Pipeline,
					Data_BeginTransition.CreateBacktrace);

			}, true);
			Data_BeginTransition.Identity.Resource->ReleaseOpRef();
			break;

		case EOpType::EndTransition:
			Data_EndTransition.Identity.Resource->EnumerateSubresources(Data_EndTransition.Identity.SubresourceRange, [this, Pipeline](FSubresourceState& State, FSubresourceIndex const& SubresourceIndex)
			{
				State.EndTransition(
					Data_EndTransition.Identity.Resource,
					SubresourceIndex,
					Data_EndTransition.PreviousState,
					Data_EndTransition.NextState,
					Pipeline,
					Data_EndTransition.CreateBacktrace);
			});
			Data_EndTransition.Identity.Resource->ReleaseOpRef();
			break;

		case EOpType::AliasingOverlap:
			FTransientState::AliasingOverlap(Data_AliasingOverlap.ResourceBefore, Data_AliasingOverlap.ResourceAfter, Data_AliasingOverlap.CreateBacktrace);
			Data_AliasingOverlap.ResourceBefore->ReleaseOpRef();
			Data_AliasingOverlap.ResourceAfter->ReleaseOpRef();
			break;

		case EOpType::SetTrackedAccess:
			Data_Assert.Identity.Resource->EnumerateSubresources(Data_SetTrackedAccess.Resource->GetWholeResourceRange(), [this, Pipeline](FSubresourceState& State, FSubresourceIndex const& SubresourceIndex)
			{
				State.AssertTracked(
					Data_SetTrackedAccess.Resource,
					SubresourceIndex,
					FState(Data_SetTrackedAccess.Access, Pipeline));
			});
			Data_SetTrackedAccess.Resource->TrackedAccess = Data_SetTrackedAccess.Access;
			Data_SetTrackedAccess.Resource->ReleaseOpRef();
			break;

		case EOpType::AcquireTransient:
			Data_AcquireTransient.Resource->TransientState.Acquire(Data_AcquireTransient.Resource, Data_AcquireTransient.CreateBacktrace);
			Data_AcquireTransient.Resource->ReleaseOpRef();
			break;

		case EOpType::DiscardTransient:
			Data_DiscardTransient.Resource->TransientState.Discard(Data_DiscardTransient.Resource, Data_DiscardTransient.CreateBacktrace);
			Data_AcquireTransient.Resource->ReleaseOpRef();
			break;

		case EOpType::Assert:
			Data_Assert.Identity.Resource->EnumerateSubresources(Data_Assert.Identity.SubresourceRange, [this, Pipeline, &bAllowAllUAVsOverlap](FSubresourceState& State, FSubresourceIndex const& SubresourceIndex)
			{
				State.Assert(
					Data_Assert.Identity.Resource,
					SubresourceIndex,
					Data_Assert.RequiredState,
					bAllowAllUAVsOverlap);
			});
			Data_Assert.Identity.Resource->ReleaseOpRef();
			break;

		case EOpType::Signal:
			if (Data_Signal.Pipeline != Pipeline)
			{
				break;
			}

			Data_Signal.Fence->bSignaled = true;
			return EReplayStatus::Signaled;

		case EOpType::Wait:
			if (Data_Wait.Pipeline != Pipeline)
			{
				break;
			}

			if (Data_Wait.Fence->bSignaled)
			{
				// The fence has been completed. Free it now.
				delete Data_Wait.Fence;

				return EReplayStatus::Normal;
			}
			else
			{
				return EReplayStatus::Waiting;
			}

		case EOpType::AllUAVsOverlap:
			RHI_VALIDATION_CHECK(bAllowAllUAVsOverlap != Data_AllUAVsOverlap.bAllow, *GetReasonString_MismatchedAllUAVsOverlapCall(Data_AllUAVsOverlap.bAllow));
			bAllowAllUAVsOverlap = Data_AllUAVsOverlap.bAllow;
			break;

		case EOpType::SpecificUAVOverlap:
			Data_SpecificUAVOverlap.Identity.Resource->EnumerateSubresources(Data_SpecificUAVOverlap.Identity.SubresourceRange, [this, Pipeline](FSubresourceState& State, FSubresourceIndex const& SubresourceIndex)
			{
				State.SpecificUAVOverlap(
					Data_SpecificUAVOverlap.Identity.Resource,
					SubresourceIndex,
					Pipeline,
					Data_SpecificUAVOverlap.bAllow);
			});
			Data_SpecificUAVOverlap.Identity.Resource->ReleaseOpRef();
			break;
		}

		return EReplayStatus::Normal;
	}

	void FTracker::AddOp(const RHIValidation::FOperation& Op)
	{
		if (GRHICommandList.Bypass() && CurrentList.Operations.Num() == 0)
		{
			auto& OpQueue = OpQueues[GetOpQueueIndex(Pipeline)];
			if (!EnumHasAllFlags(Op.Replay(Pipeline, OpQueue.bAllowAllUAVsOverlap, OpQueue.Breadcrumbs), EReplayStatus::Waiting))
			{
				return;
			}
		}

		CurrentList.Operations.Add(Op);
	}

	void FTracker::ReplayOpQueue(ERHIPipeline DstOpQueue, FOperationsList&& InOpsList)
	{
		int32 DstOpQueueIndex = GetOpQueueIndex(DstOpQueue);
		FOpQueueState& DstQueue = OpQueues[DstOpQueueIndex];

		// Replay any barrier operations to validate resource barrier usage.
		EReplayStatus Status;
		OpQueues[DstOpQueueIndex].bWaiting |= InOpsList.Incomplete();
		do
		{
			Status = EReplayStatus::Normal;
			for (int32 CurrentIndex = 0; CurrentIndex < int32(ERHIPipeline::Num); ++CurrentIndex)
			{
				const ERHIPipeline CurrentPipeline = ERHIPipeline(1 << CurrentIndex);
				FOpQueueState& CurrentQueue = OpQueues[CurrentIndex];
				if (CurrentQueue.bWaiting)
				{
					Status = CurrentQueue.Ops.Replay(CurrentPipeline, CurrentQueue.bAllowAllUAVsOverlap, CurrentQueue.Breadcrumbs);
					if (!EnumHasAllFlags(Status, EReplayStatus::Waiting))
					{
						CurrentQueue.Ops.Reset();
						if (CurrentIndex == DstOpQueueIndex && InOpsList.Incomplete())
						{
							Status |= InOpsList.Replay(CurrentPipeline, CurrentQueue.bAllowAllUAVsOverlap, CurrentQueue.Breadcrumbs);
							CurrentQueue.bWaiting = InOpsList.Incomplete();
						}
						else
						{
							CurrentQueue.bWaiting = false;
						}
					}

					if (EnumHasAllFlags(Status, EReplayStatus::Signaled))
					{
						// run through the queues again to release any waits
						break;
					}
				}
			}
		} while (EnumHasAllFlags(Status, EReplayStatus::Signaled));

		// enqueue incomplete operations
		if (InOpsList.Incomplete())
		{
			DstQueue.Ops.Append(InOpsList);
			InOpsList.Reset();
			DstQueue.bWaiting = true;
		}
	}


	void FUniformBufferResource::InitLifetimeTracking(uint64 FrameID, const void* Contents, EUniformBufferUsage Usage)
	{
		AllocatedFrameID = FrameID;
		UniformBufferUsage = Usage;
		bContainsNullContents = Contents == nullptr;

#if CAPTURE_UNIFORMBUFFER_ALLOCATION_BACKTRACES
		AllocatedCallstack = (UniformBufferUsage != UniformBuffer_MultiFrame) ? RHIValidation::CaptureBacktrace() : nullptr;
#else
		AllocatedCallstack = nullptr;
#endif
	}

	void FUniformBufferResource::UpdateAllocation(uint64 FrameID)
	{
		AllocatedFrameID = FrameID;
		bContainsNullContents = false;

#if CAPTURE_UNIFORMBUFFER_ALLOCATION_BACKTRACES
		AllocatedCallstack = (UniformBufferUsage != UniformBuffer_MultiFrame) ? RHIValidation::CaptureBacktrace() : nullptr;
#else
		AllocatedCallstack = nullptr;
#endif
	}

	void FUniformBufferResource::ValidateLifeTime()
	{
		FValidationRHI* ValidateRHI = (FValidationRHI*)GDynamicRHI;

		RHI_VALIDATION_CHECK(bContainsNullContents == false, TEXT("Uniform buffer created with null contents is now being bound for rendering on an RHI context. The contents must first be updated."));

		if (UniformBufferUsage != UniformBuffer_MultiFrame && AllocatedFrameID != ValidateRHI->RHIThreadFrameID)
		{
			FString ErrorMessage = TEXT("Non MultiFrame Uniform buffer has been allocated in a previous frame. The data could have been deleted already!");
			if (AllocatedCallstack != nullptr)
			{
				ErrorMessage += FString::Printf(TEXT("\nAllocation callstack: (void**)0x%p,32"), AllocatedCallstack);
			}
			RHI_VALIDATION_CHECK(false, *ErrorMessage);
		}
	}

	
	FTracker::FOpQueueState FTracker::OpQueues[int32(ERHIPipeline::Num)] = {};

	void* CaptureBacktrace()
	{
		// Back traces will leak. Don't leave this turned on.
		const uint32 MaxDepth = 32;
		uint64* Backtrace = new uint64[MaxDepth];
		FPlatformStackWalk::CaptureStackBackTrace(Backtrace, MaxDepth);

		return Backtrace;
	}

	bool ValidateDimension(EShaderCodeResourceBindingType Type, FRHIViewDesc::EDimension Dimension, bool SRV)
	{
		// Ignore invalid types
		if (Type == EShaderCodeResourceBindingType::Invalid)
		{
			return true;
		}

		if (IsResourceBindingTypeSRV(Type) != SRV)
		{
			return false;
		}

		if (Type == EShaderCodeResourceBindingType::Texture2D || Type == EShaderCodeResourceBindingType::RWTexture2D || Type == EShaderCodeResourceBindingType::Texture2DMS)
		{
			return Dimension == FRHIViewDesc::EDimension::Texture2D;
		}
		else if (Type == EShaderCodeResourceBindingType::Texture2DArray || Type == EShaderCodeResourceBindingType::RWTexture2DArray)
		{
			return Dimension == FRHIViewDesc::EDimension::Texture2DArray;
		}
		else if (Type == EShaderCodeResourceBindingType::Texture3D || Type == EShaderCodeResourceBindingType::RWTexture3D)
		{
			return Dimension == FRHIViewDesc::EDimension::Texture3D;
		}
		else if (Type == EShaderCodeResourceBindingType::TextureCube || Type == EShaderCodeResourceBindingType::RWTextureCube)
		{
			return Dimension == FRHIViewDesc::EDimension::TextureCube;
		}
		else if (Type == EShaderCodeResourceBindingType::TextureCubeArray)
		{
			return Dimension == FRHIViewDesc::EDimension::TextureCubeArray;
		}

		return false;
	}

	bool ValidateDimension(EShaderCodeResourceBindingType Type, ETextureDimension Dimension, bool SRV)
	{
		// Ignore invalid types
		if (Type == EShaderCodeResourceBindingType::Invalid)
		{
			return true;
		}

		if (Type == EShaderCodeResourceBindingType::Texture2D || Type == EShaderCodeResourceBindingType::RWTexture2D || Type == EShaderCodeResourceBindingType::Texture2DMS)
		{
			return Dimension == ETextureDimension::Texture2D;
		}
		else if (Type == EShaderCodeResourceBindingType::Texture2DArray || Type == EShaderCodeResourceBindingType::RWTexture2DArray)
		{
			return Dimension == ETextureDimension::Texture2DArray;
		}
		else if (Type == EShaderCodeResourceBindingType::Texture3D || Type == EShaderCodeResourceBindingType::RWTexture3D)
		{
			return Dimension == ETextureDimension::Texture3D;
		}
		else if (Type == EShaderCodeResourceBindingType::TextureCube || Type == EShaderCodeResourceBindingType::RWTextureCube)
		{
			return Dimension == ETextureDimension::TextureCube;
		}
		else if (Type == EShaderCodeResourceBindingType::TextureCubeArray)
		{
			return Dimension == ETextureDimension::TextureCubeArray;
		}

		return false;
	}

	bool ValidateBuffer(EShaderCodeResourceBindingType Type, FRHIViewDesc::EBufferType BufferType, bool SRV)
	{
		// Ignore invalid types
		if (Type == EShaderCodeResourceBindingType::Invalid)
		{
			return true;
		}

		if (IsResourceBindingTypeSRV(Type) != SRV)
		{
			return false;
		}

		if (Type == EShaderCodeResourceBindingType::ByteAddressBuffer || Type == EShaderCodeResourceBindingType::RWByteAddressBuffer)
		{
			return BufferType == FRHIViewDesc::EBufferType::Raw;
		}
		else if (Type == EShaderCodeResourceBindingType::StructuredBuffer || Type == EShaderCodeResourceBindingType::RWStructuredBuffer)
		{
			return BufferType == FRHIViewDesc::EBufferType::Structured;
		}
		else if (Type == EShaderCodeResourceBindingType::Buffer || Type == EShaderCodeResourceBindingType::RWBuffer)
		{
			return BufferType == FRHIViewDesc::EBufferType::Typed;
		}
		else if (Type == EShaderCodeResourceBindingType::RaytracingAccelerationStructure)
		{
			return BufferType == FRHIViewDesc::EBufferType::AccelerationStructure;
		}
		
		return false;
	}

	/** Validates that the SRV is conform to what the shader expects */
	void ValidateShaderResourceView(const FRHIShader* RHIShaderBase, uint32 BindIndex, const FRHIShaderResourceView* SRV)
	{
#if RHI_INCLUDE_SHADER_DEBUG_DATA
		if (SRV)
		{
			auto const ViewIdentity = SRV->GetViewIdentity();

			static const auto GetSRVName = [](const FRHIShaderResourceView* SRV, auto& ViewIdentity) -> FString
			{
				FString SRVName;
				if (ViewIdentity.Resource)
				{
					SRVName = ViewIdentity.Resource->GetDebugName();
				}
				if (SRVName.IsEmpty())
				{
					SRVName = SRV->GetOwnerName().ToString();
				}

				return SRVName;
			};

			// DebugStrideValidationData is supposed to be already sorted
			static const auto ShaderCodeValidationStridePredicate = [](const FShaderCodeValidationStride& lhs, const FShaderCodeValidationStride& rhs) -> bool { return lhs.BindPoint < rhs.BindPoint; };
			FShaderCodeValidationStride SRVValidationStride = { BindIndex , ViewIdentity.Stride };

			int32 FoundIndex =  Algo::BinarySearch(RHIShaderBase->DebugStrideValidationData, SRVValidationStride, ShaderCodeValidationStridePredicate);
			if (FoundIndex != INDEX_NONE)
			{
				FString SRVName = GetSRVName(SRV, ViewIdentity);
				uint16 ExpectedStride = RHIShaderBase->DebugStrideValidationData[FoundIndex].Stride;
				if (ExpectedStride != SRVValidationStride.Stride)
				{
					
					FString ErrorMessage = FString::Printf(TEXT("Shader %s: Buffer stride for \"%s\" must match structure size declared in the shader"), RHIShaderBase->GetShaderName(), *SRVName);
					ErrorMessage += FString::Printf(TEXT("\nBind point: %d, HLSL size: %d, Buffer Size: %d"), BindIndex, ExpectedStride, SRVValidationStride.Stride);
					RHI_VALIDATION_CHECK(false, *ErrorMessage);
				}
			}

			// Validate Type
			if (!RHIShaderBase->DebugSRVTypeValidationData.Num())
				return;

			static const auto ShaderCodeValidationTypePredicate = [](const FShaderCodeValidationType& lhs, const FShaderCodeValidationType& rhs) -> bool { return lhs.BindPoint < rhs.BindPoint; };
			
			FShaderCodeValidationType SRVValidationType = { BindIndex , EShaderCodeResourceBindingType::Invalid };
			FoundIndex = Algo::BinarySearch(RHIShaderBase->DebugSRVTypeValidationData, SRVValidationType, ShaderCodeValidationTypePredicate);

			if (FoundIndex != INDEX_NONE)
			{
				EShaderCodeResourceBindingType ExpectedType = RHIShaderBase->DebugSRVTypeValidationData[FoundIndex].Type;

				if (SRV->IsTexture())
				{
					if (!ValidateDimension(ExpectedType, SRV->GetDesc().Texture.SRV.Dimension, true))
					{
						FString SRVName = GetSRVName(SRV, ViewIdentity);
						FString ErrorMessage = FString::Printf(TEXT("Shader %s: Dimension for SRV \"%s\" must match type declared in the shader"), RHIShaderBase->GetShaderName(), *SRVName);
						ErrorMessage += FString::Printf(TEXT("\nBind point: %d, HLSL Type: %s, Actual Dimension: %s"),
							BindIndex, 
							GetShaderCodeResourceBindingTypeName(ExpectedType),
							FRHIViewDesc::GetTextureDimensionString(SRV->GetDesc().Texture.SRV.Dimension));
						RHI_VALIDATION_CHECK(false, *ErrorMessage);
					}
				}
				else if (SRV->IsBuffer())
				{
					if (!ValidateBuffer(ExpectedType, SRV->GetDesc().Buffer.SRV.BufferType, true))
					{
						FString SRVName = GetSRVName(SRV, ViewIdentity);
						FString ErrorMessage = FString::Printf(TEXT("Shader %s: Buffer type for SRV \"%s\" must match buffer type declared in the shader"), RHIShaderBase->GetShaderName(), *SRVName);
						ErrorMessage += FString::Printf(TEXT("\nBind point: %d, HLSL Type: %s, Actual Type: %s"),
							BindIndex,
							GetShaderCodeResourceBindingTypeName(ExpectedType),
							FRHIViewDesc::GetBufferTypeString(SRV->GetDesc().Buffer.SRV.BufferType));
						RHI_VALIDATION_CHECK(false, *ErrorMessage);
					}
				}
			}
			else
			{ 
				FString SRVName = GetSRVName(SRV, ViewIdentity);
				FString ErrorMessage = FString::Printf(TEXT("Shader %s: No bind point found for SRV \"%s\" possible UAV/SRV mismatch"), RHIShaderBase->GetShaderName(), *SRVName);
				if (SRV->IsTexture())
				{
					ErrorMessage += FString::Printf(TEXT("\nBind point: %d, Type: %s"),
						BindIndex,
						FRHIViewDesc::GetTextureDimensionString(SRV->GetDesc().Texture.SRV.Dimension));
				}
				else
				{
					ErrorMessage += FString::Printf(TEXT("\nBind point: %d, Type: %s"),
						BindIndex,
						FRHIViewDesc::GetBufferTypeString(SRV->GetDesc().Buffer.SRV.BufferType));
				}
				RHI_VALIDATION_CHECK(false, *ErrorMessage);
			}
		}
#endif
	}

	/** Validates that the SRV is conform to what the shader expects */
	void ValidateShaderResourceView(const FRHIShader* RHIShaderBase, uint32 BindIndex, const FRHITexture* Texture)
	{
#if RHI_INCLUDE_SHADER_DEBUG_DATA
		if (Texture)
		{
			// Validate Type
			if (!RHIShaderBase->DebugSRVTypeValidationData.Num())
				return;

			static const auto ShaderCodeValidationTypePredicate = [](const FShaderCodeValidationType& lhs, const FShaderCodeValidationType& rhs) -> bool { return lhs.BindPoint < rhs.BindPoint; };

			FShaderCodeValidationType SRVValidationType = { BindIndex , EShaderCodeResourceBindingType::Invalid };
			int32 FoundIndex = Algo::BinarySearch(RHIShaderBase->DebugSRVTypeValidationData, SRVValidationType, ShaderCodeValidationTypePredicate);

			if (FoundIndex != INDEX_NONE)
			{
				EShaderCodeResourceBindingType ExpectedType = RHIShaderBase->DebugSRVTypeValidationData[FoundIndex].Type;

				if (!ValidateDimension(ExpectedType, Texture->GetDesc().Dimension, true))
				{
					FString ErrorMessage = FString::Printf(TEXT("Shader %s: Dimension for Texture %s at BindIndex \"%d\" must match type declared in the shader"), 
						RHIShaderBase->GetShaderName(),
						*Texture->GetName().ToString(),
						BindIndex);
					ErrorMessage += FString::Printf(TEXT("\nBind point: %d, HLSL Type: %s, Actual Dimension: %s"),
						BindIndex,
						GetShaderCodeResourceBindingTypeName(ExpectedType),
						GetTextureDimensionString(Texture->GetDesc().Dimension));
					RHI_VALIDATION_CHECK(false, *ErrorMessage);
				}
			}
			else
			{
				FString ErrorMessage = FString::Printf(TEXT("Shader %s: No bind point found at BindIndex \"%d\" possible UAV/SRV mismatch"), RHIShaderBase->GetShaderName(), BindIndex);
					ErrorMessage += FString::Printf(TEXT("\nBind point: %d, Type: %s"),
						BindIndex,
						GetTextureDimensionString(Texture->GetDesc().Dimension));
				RHI_VALIDATION_CHECK(false, *ErrorMessage);
			}
		}
#endif
	}

	/** Validates that the UAV is conform to what the shader expects */
	void ValidateUnorderedAccessView(const FRHIShader* RHIShaderBase, uint32 BindIndex, const FRHIUnorderedAccessView* UAV)
	{
#if RHI_INCLUDE_SHADER_DEBUG_DATA
		if (UAV)
		{
			auto const ViewIdentity = UAV->GetViewIdentity();

			static const auto GetUAVName = [](const FRHIUnorderedAccessView* UAV, auto& ViewIdentity) -> FString
			{
				FString UAVName;
				if (ViewIdentity.Resource)
				{
					UAVName = ViewIdentity.Resource->GetDebugName();
				}
				if (UAVName.IsEmpty())
				{
					UAVName = UAV->GetOwnerName().ToString();
				}

				return UAVName;
			};

			// Validate Type
			if (!RHIShaderBase->DebugUAVTypeValidationData.Num())
				return;

			static const auto ShaderCodeValidationTypePredicate = [](const FShaderCodeValidationType& lhs, const FShaderCodeValidationType& rhs) -> bool { return lhs.BindPoint < rhs.BindPoint; };

			FShaderCodeValidationType SRVValidationType = { BindIndex , EShaderCodeResourceBindingType::Invalid };
			int32 FoundIndex = Algo::BinarySearch(RHIShaderBase->DebugUAVTypeValidationData, SRVValidationType, ShaderCodeValidationTypePredicate);

			if (FoundIndex != INDEX_NONE)
			{
				EShaderCodeResourceBindingType ExpectedType = RHIShaderBase->DebugUAVTypeValidationData[FoundIndex].Type;

				if (UAV->IsTexture())
				{
					if (!ValidateDimension(ExpectedType, UAV->GetDesc().Texture.UAV.Dimension, false))
					{
						FString UAVName = GetUAVName(UAV, ViewIdentity);
						FString ErrorMessage = FString::Printf(TEXT("Shader %s: Dimension for UAV \"%s\" must match type declared in the shader"), RHIShaderBase->GetShaderName(), *UAVName);
						ErrorMessage += FString::Printf(TEXT("\nBind point: %d, HLSL Type: %s, Actual Dimension: %s"), 
							BindIndex,
							GetShaderCodeResourceBindingTypeName(ExpectedType),
							FRHIViewDesc::GetTextureDimensionString(UAV->GetDesc().Texture.SRV.Dimension));
						RHI_VALIDATION_CHECK(false, *ErrorMessage);
					}
				}
				else if (UAV->IsBuffer())
				{
					if (!ValidateBuffer(ExpectedType, UAV->GetDesc().Buffer.UAV.BufferType, false))
					{
						FString UAVName = GetUAVName(UAV, ViewIdentity);
						FString ErrorMessage = FString::Printf(TEXT("Shader %s: Buffer type for UAV \"%s\" must match buffer type declared in the shader"), RHIShaderBase->GetShaderName(), *UAVName);
						ErrorMessage += FString::Printf(TEXT("\nBind point: %d, HLSL Type: %s, Actual Type: %s"),
							BindIndex,
							GetShaderCodeResourceBindingTypeName(ExpectedType),
							FRHIViewDesc::GetBufferTypeString(UAV->GetDesc().Buffer.UAV.BufferType));
						RHI_VALIDATION_CHECK(false, *ErrorMessage);
					}
				}
			}
			else
			{
				FString UAVName = GetUAVName(UAV, ViewIdentity);
				FString ErrorMessage = FString::Printf(TEXT("Shader %s: No bind point found for UAV \"%s\" possible UAV/SRV mismatch"), RHIShaderBase->GetShaderName(), *UAVName);

				if (UAV->IsTexture())
				{
					ErrorMessage += FString::Printf(TEXT("\nBind point: %d, Type: %s"),
						BindIndex,
						FRHIViewDesc::GetTextureDimensionString(UAV->GetDesc().Texture.SRV.Dimension));
				}
				else
				{
					ErrorMessage += FString::Printf(TEXT("\nBind point: %d, Type: %s"),
						BindIndex,
						FRHIViewDesc::GetBufferTypeString(UAV->GetDesc().Buffer.SRV.BufferType));
				}
				RHI_VALIDATION_CHECK(false, *ErrorMessage);
			}
		}
#endif
	}

	/** Validates that the Uniform conforms to what the shader expects */
	void ValidateUniformBuffer(const FRHIShader* RHIShaderBase, uint32 BindIndex, FRHIUniformBuffer* UB)
	{
#if RHI_INCLUDE_SHADER_DEBUG_DATA
		if (UB)
		{
			// Validate Type
			static const auto ShaderCodeValidationUBSizePredicate = [](const FShaderCodeValidationUBSize& lhs, const FShaderCodeValidationUBSize& rhs) -> bool { return lhs.BindPoint < rhs.BindPoint; };

			FShaderCodeValidationUBSize SRVValidationSize = { BindIndex , 0 };
			int32 FoundIndex = Algo::BinarySearch(RHIShaderBase->DebugUBSizeValidationData, SRVValidationSize, ShaderCodeValidationUBSizePredicate);
			if (FoundIndex != INDEX_NONE)
			{
				uint32_t Size = RHIShaderBase->DebugUBSizeValidationData[FoundIndex].Size;

				if(Size > 0 && Size > UB->GetSize())
				{
					const FRHIUniformBufferLayout& Layout = UB->GetLayout();

					FString ErrorMessage = FString::Printf(TEXT("Shader %s: Uniform buffer \"%s\" has unexpected size"), RHIShaderBase->GetShaderName(), *Layout.GetDebugName());
					ErrorMessage += FString::Printf(TEXT("\nBind point: %d, HLSL size: %d, Actual size: %d"), BindIndex, Size, UB->GetSize());
					RHI_VALIDATION_CHECK(false, *ErrorMessage);
				}
			}
		}
#endif
	}
}


//-----------------------------------------------------------------------------
//	Validation Transient Resource Allocator
//-----------------------------------------------------------------------------

#define TRANSIENT_RESOURCE_LOG_PREFIX_REASON(ReasonString) TEXT("RHI validation failed: " ReasonString "\n\n")\
	TEXT("--------------------------------------------------------------------\n")\
	TEXT("         RHI Transient Resource Allocation Validation Error		  \n")\
	TEXT("--------------------------------------------------------------------\n")\
	TEXT("\n")

#define TRANSIENT_RESOURCE_LOG_SUFFIX TEXT("\n")\
	TEXT("--------------------------------------------------------------------\n")\
	TEXT("\n")

FValidationTransientResourceAllocator::~FValidationTransientResourceAllocator()
{
	checkf(!RHIAllocator, TEXT("Release was not called on FRHITransientResourceAllocator."));
}

FRHITransientTexture* FValidationTransientResourceAllocator::CreateTexture(const FRHITextureCreateInfo& InCreateInfo, const TCHAR* InDebugName, uint32 InPassIndex)
{
	check(FRHITextureCreateInfo::CheckValidity(InCreateInfo, InDebugName));

	FRHITransientTexture* TransientTexture = RHIAllocator->CreateTexture(InCreateInfo, InDebugName, InPassIndex);

	if (!TransientTexture)
	{
		return nullptr;
	}

	FRHITexture* RHITexture = TransientTexture->GetRHI();

	// Store allocation data
	FAllocatedResourceData ResourceData;
	ResourceData.DebugName = InDebugName;
	ResourceData.ResourceType = FAllocatedResourceData::EType::Texture;
	ResourceData.Texture.Flags = InCreateInfo.Flags;
	ResourceData.Texture.Format = InCreateInfo.Format;
	ResourceData.Texture.ArraySize = InCreateInfo.ArraySize;
	ResourceData.Texture.NumMips = InCreateInfo.NumMips;
	AllocatedResourceMap.Add(RHITexture, ResourceData);

	if (RHIValidation::FResource* Resource = RHITexture->GetTrackerResource())
	{
		if (!Resource->IsBarrierTrackingInitialized())
		{
			RHITexture->InitBarrierTracking(InCreateInfo.NumMips, InCreateInfo.ArraySize * (InCreateInfo.IsTextureCube() ? 6 : 1), InCreateInfo.Format, InCreateInfo.Flags, ERHIAccess::Discard, InDebugName);
		}
		else
		{
			AllocatedResourcesToInit.Emplace(RHITexture, ResourceData);
		}
	}

	return TransientTexture;
}

FRHITransientBuffer* FValidationTransientResourceAllocator::CreateBuffer(const FRHIBufferCreateInfo& InCreateInfo, const TCHAR* InDebugName, uint32 InPassIndex)
{
	FRHITransientBuffer* TransientBuffer = RHIAllocator->CreateBuffer(InCreateInfo, InDebugName, InPassIndex);

	if (!TransientBuffer)
	{
		return nullptr;
	}

	FRHIBuffer* RHIBuffer = TransientBuffer->GetRHI();

	// Store allocation data
	FAllocatedResourceData ResourceData;
	ResourceData.DebugName = InDebugName;
	ResourceData.ResourceType = FAllocatedResourceData::EType::Buffer;
	AllocatedResourceMap.Add(RHIBuffer, ResourceData);

	if (!RHIBuffer->IsBarrierTrackingInitialized())
	{
		RHIBuffer->InitBarrierTracking(ERHIAccess::Discard, InDebugName);
	}
	else
	{
		AllocatedResourcesToInit.Emplace(RHIBuffer, ResourceData);
	}

	return TransientBuffer;
}

void FValidationTransientResourceAllocator::DeallocateMemory(FRHITransientTexture* InTransientTexture, uint32 InPassIndex)
{
	check(InTransientTexture);

	RHIAllocator->DeallocateMemory(InTransientTexture, InPassIndex);

	checkf(AllocatedResourceMap.Contains(InTransientTexture->GetRHI()), TEXT("DeallocateMemory called on texture %s, but it is not marked as allocated."), InTransientTexture->GetName());
	AllocatedResourceMap.Remove(InTransientTexture->GetRHI());
}

void FValidationTransientResourceAllocator::DeallocateMemory(FRHITransientBuffer* InTransientBuffer, uint32 InPassIndex)
{
	check(InTransientBuffer);

	RHIAllocator->DeallocateMemory(InTransientBuffer, InPassIndex);

	checkf(AllocatedResourceMap.Contains(InTransientBuffer->GetRHI()), TEXT("DeallocateMemory called on buffer %s, but it is not marked as allocated."), InTransientBuffer->GetName());
	AllocatedResourceMap.Remove(InTransientBuffer->GetRHI());
}

void FValidationTransientResourceAllocator::Flush(FRHICommandListImmediate& RHICmdList, FRHITransientAllocationStats* OutHeapStats)
{
	RHICmdList.EnqueueLambda([AllocatedResourcesToInit = MoveTemp(AllocatedResourcesToInit)](FRHICommandListImmediate& InRHICmdList)
	{
		// Tracking will be re-initialized, so we need to flush any remaining references.
		static_cast<FValidationContext&>(InRHICmdList.GetContext()).FlushValidationOps();
		InitBarrierTracking(AllocatedResourcesToInit);
	});

	RHIAllocator->Flush(RHICmdList, OutHeapStats);
}

void FValidationTransientResourceAllocator::Release(FRHICommandListImmediate& RHICmdList)
{
	// Check all allocated resource data and make sure all memory is freed again
	{
		if (AllocatedResourceMap.Num() > 0)
		{
			FString ErrorMessage = FString::Printf(
				TRANSIENT_RESOURCE_LOG_PREFIX_REASON("Open transient allocations")
				TEXT("%d Transient Resource allocations still have memory allocated. Call 'DeallocateMemory' on all transient allocated resources prior to releasing the allocator.\n\n")
				TEXT("Resources with Allocated Memory:\n"),
				AllocatedResourceMap.Num());

			for (const auto& KeyValue : AllocatedResourceMap)
			{
				const FAllocatedResourceData& ResourceData = KeyValue.Value;

				ErrorMessage += FString::Printf(TEXT("         %s (%s)\n"), *ResourceData.DebugName, ResourceData.ResourceType == FAllocatedResourceData::EType::Texture ? TEXT("Texture") : TEXT("Buffer"));
			}
			ErrorMessage += FString::Printf(TRANSIENT_RESOURCE_LOG_SUFFIX);
			FValidationRHI::ReportValidationFailure(*ErrorMessage);
		}
	}

	RHIAllocator->Release(RHICmdList);
	RHIAllocator = nullptr;
	delete this;
}

void FValidationTransientResourceAllocator::InitBarrierTracking(const FAllocatedResourceDataArray& AllocatedResourcesToInit)
{
	// Barrier tracking initialization has to happen on the RHI thread, because RHI resources are pooled and reused.

	for (const auto& Entry : AllocatedResourcesToInit)
	{
		FRHIResource* Resource = Entry.Key;
		const FAllocatedResourceData& ResourceData = Entry.Value;

		switch (ResourceData.ResourceType)
		{
		case FAllocatedResourceData::EType::Texture:
		{
			FRHITexture* Texture = static_cast<FRHITexture*>(Resource);

			int32 ArraySize = ResourceData.Texture.ArraySize;

			if (Texture->GetTextureCube() != nullptr)
			{
				ArraySize *= 6;
			}

			Texture->InitBarrierTracking(ResourceData.Texture.NumMips, ArraySize, ResourceData.Texture.Format, ResourceData.Texture.Flags, ERHIAccess::Discard, *ResourceData.DebugName);
		}
		break;
		case FAllocatedResourceData::EType::Buffer:
		{
			FRHIBuffer* Buffer = static_cast<FRHIBuffer*>(Resource);
			Buffer->InitBarrierTracking(ERHIAccess::Discard, *ResourceData.DebugName);
		}
		break;
		}
	}
}

#endif	// ENABLE_RHI_VALIDATION
