// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Util.h: D3D RHI utility implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "EngineModule.h"
#include "RendererInterface.h"
#include "CoreGlobals.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HAL/ExceptionHandling.h"
#if PLATFORM_WINDOWS
#include "HAL/PlatformCrashContext.h"
#endif
#include "GenericPlatform/GenericPlatformCrashContext.h"

#define D3DERR(x) case x: ErrorCodeText = TEXT(#x); break;
#define LOCTEXT_NAMESPACE "Developer.MessageLog"

// GPU crashes are nonfatal on windows/nonshipping so as not to interfere with GPU crash dump processing
#if PLATFORM_WINDOWS || !UE_BUILD_SHIPPING
  #define D3D12RHI_GPU_CRASH_LOG_VERBOSITY Error
#else
  #define D3D12RHI_GPU_CRASH_LOG_VERBOSITY Fatal
#endif

template<typename PerDeviceFunction>
void FD3D12DynamicRHI::ForEachDevice(ID3D12Device* inDevice, const PerDeviceFunction& pfPerDeviceFunction)
{
	for (uint32 AdapterIndex = 0; AdapterIndex < GetNumAdapters(); ++AdapterIndex)
	{
		FD3D12Adapter& D3D12Adapter = GetAdapter(AdapterIndex);
		for (uint32 GPUIndex : FRHIGPUMask::All())
		{
			FD3D12Device* D3D12Device = D3D12Adapter.GetDevice(GPUIndex);
			if (inDevice == nullptr || D3D12Device->GetDevice() == inDevice)
			{
				pfPerDeviceFunction(D3D12Device);
			}
		}
	}
}

static FString GetUniqueName()
{
	static int64 ID = 0;
	const int64 UniqueID = FPlatformAtomics::InterlockedIncrement(&ID);
	const FString UniqueName = FString::Printf(TEXT("D3D12RHIObjectUniqueName%lld"), UniqueID);
	return UniqueName;
}

void SetName(ID3D12Object* const Object, const TCHAR* const Name)
{
#if NAME_OBJECTS
	if (Object && Name)
	{
		VERIFYD3D12RESULT(Object->SetName(Name));
	}
	else if (Object)
	{
		VERIFYD3D12RESULT(Object->SetName(*GetUniqueName()));
	}
#else
	UNREFERENCED_PARAMETER(Object);
	UNREFERENCED_PARAMETER(Name);
#endif
}

void SetName(FD3D12Resource* const Resource, const TCHAR* const Name)
{
#if NAME_OBJECTS
	// Special case for FD3D12Resources because we also store the name as a member in FD3D12Resource
	if (Resource && Name)
	{
		Resource->SetName(Name);
	}
	else if (Resource)
	{
		Resource->SetName(*GetUniqueName());
	}
#else
	UNREFERENCED_PARAMETER(Resource);
	UNREFERENCED_PARAMETER(Name);
#endif
}

static FString GetD3D12DeviceHungErrorString(HRESULT ErrorCode)
{
	FString ErrorCodeText;

	switch (ErrorCode)
	{
		D3DERR(DXGI_ERROR_DEVICE_HUNG)
		D3DERR(DXGI_ERROR_DEVICE_REMOVED)
		D3DERR(DXGI_ERROR_DEVICE_RESET)
		D3DERR(DXGI_ERROR_DRIVER_INTERNAL_ERROR)
		D3DERR(DXGI_ERROR_INVALID_CALL)
		default:
			ErrorCodeText = FString::Printf(TEXT("%08X"), (int32)ErrorCode);
	}

	return ErrorCodeText;
}

static FString GetD3D12ErrorString(HRESULT ErrorCode, ID3D12Device* Device)
{
	FString ErrorCodeText;

	switch (ErrorCode)
	{
		D3DERR(S_OK);
		D3DERR(D3D11_ERROR_FILE_NOT_FOUND)
		D3DERR(D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS)
		D3DERR(E_FAIL)
		D3DERR(E_INVALIDARG)
		D3DERR(E_OUTOFMEMORY)
		D3DERR(DXGI_ERROR_INVALID_CALL)
		D3DERR(DXGI_ERROR_WAS_STILL_DRAWING)
		D3DERR(E_NOINTERFACE)
		D3DERR(DXGI_ERROR_DEVICE_REMOVED)
#if PLATFORM_WINDOWS
		EMBED_DXGI_ERROR_LIST(D3DERR, )
#endif
		default:
			ErrorCodeText = FString::Printf(TEXT("%08X"), (int32)ErrorCode);
	}

	if (ErrorCode == DXGI_ERROR_DEVICE_REMOVED && Device)
	{
		HRESULT hResDeviceRemoved = Device->GetDeviceRemovedReason();
		ErrorCodeText += FString(TEXT(" with Reason: ")) + GetD3D12DeviceHungErrorString(hResDeviceRemoved);
	}

	return ErrorCodeText;
}

#undef D3DERR

static FString GetD3D12TextureFlagString(uint32 TextureFlags)
{
	FString TextureFormatText = TEXT("");

	if (TextureFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		TextureFormatText += TEXT("D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ");
	}

	if (TextureFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		TextureFormatText += TEXT("D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL ");
	}

	if (TextureFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
	{
		TextureFormatText += TEXT("D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE ");
	}

	if (TextureFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		TextureFormatText += TEXT("D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS ");
	}
	return TextureFormatText;
}

/* Export GPU breadcrumbs data as part of the crash payload. RHI breadcrumbs are preferred over other types and will overwrite if necessary. */
static void ExportBreadcrumbDataAsCrashPayload(const FString& BreadcrumbSource, const FString& GPUQueueName, const TArray<FBreadcrumbNode>& Breadcrumbs)
{
	const FString& CurrentSource = FGenericCrashContext::GetGPUBreadcrumbsSource();

	if (CurrentSource.IsEmpty() || BreadcrumbSource == CurrentSource || BreadcrumbSource == TEXT("RHI"))
	{
		if (CurrentSource != BreadcrumbSource)
		{
			FGenericCrashContext::ResetGPUBreadcrumbsData();
		}

		FGenericCrashContext::SetGPUBreadcrumbsSource(BreadcrumbSource);
		FGenericCrashContext::SetGPUBreadcrumbs(GPUQueueName, Breadcrumbs);
	}
}

FBreadcrumbNode CollectBreadcrumbNode(
	D3D12RHI::FD3DGPUProfiler& GPUProfiler,
	const TSharedPtr<FBreadcrumbStack>& Stack,
	const FBreadcrumbStack::FScope& Scope)
{
	FBreadcrumbNode Node;

	const volatile uint32* Markers = reinterpret_cast<uint32*>(Stack->CPUAddress);
	const bool bHaveMarkerIndex = (Scope.MarkerIndex < Stack->MaxMarkers);
	Node.State = bHaveMarkerIndex ? (EBreadcrumbState)Markers[Scope.MarkerIndex] : EBreadcrumbState::Overflow;

	const FString* EventName = GPUProfiler.FindEventString(Scope.NameCRC);
	Node.Name = EventName ? *EventName : TEXT("Unknown");

	if (Node.State == EBreadcrumbState::Active)
	{
		for (uint32 Child = Scope.Child; Child != 0; Child = Stack->Scopes[Child].Sibling)
		{
			Node.Children.Emplace(CollectBreadcrumbNode(GPUProfiler, Stack, Stack->Scopes[Child]));
		}
	}

	return Node;
};

void BuildBreadcrumbsDumpString(const FBreadcrumbNode& Breadcrumb, FString& OutString, uint32 Indent)
{
	const TCHAR* Prefix = TEXT("  ");
	if (Breadcrumb.State == EBreadcrumbState::Finished)
	{
		Prefix = TEXT("| ");
	}
	else if (Breadcrumb.State == EBreadcrumbState::Active)
	{
		Prefix = TEXT("> ");
	}

	const TCHAR* Suffix = TEXT("");
	if (Breadcrumb.State == EBreadcrumbState::Overflow)
	{
		Suffix = TEXT(" [overflow]");
	}

	for (uint32 Idx = 0; Idx < Indent * 2; ++Idx)
	{
		OutString.AppendChar(' ');
	}

	OutString.Append(FString::Printf(TEXT("Breadcrumbs: %s%s [%s]%s\n"), Prefix, *Breadcrumb.Name, Breadcrumb.GetStateString(), Suffix));

	for (const FBreadcrumbNode& Child : Breadcrumb.Children)
	{
		BuildBreadcrumbsDumpString(Child, OutString, Indent + 1);
	}
}

/** Log the GPU progress of the given queue to the Error log if breadcrumb data is available */
static bool LogBreadcrumbData(D3D12RHI::FD3DGPUProfiler& GPUProfiler, FD3D12Queue& Queue)
{
	FString GPUQueueName = FString::Printf(TEXT("%s Queue %d"), GetD3DCommandQueueTypeName(Queue.QueueType),
		Queue.Device->GetGPUIndex());

	FString GpuProgress = FString::Printf(TEXT("[GPUBreadCrumb]\t%s\n"), *GPUQueueName);

	TArray<TSharedPtr<FBreadcrumbStack>, TInlineAllocator<8>> UniqueStacks;

	while (const FD3D12Payload* Payload = Queue.PendingInterrupt.Peek())
	{
		for (const TSharedPtr<FBreadcrumbStack>& Stack : Payload->BreadcrumbStacks)
		{
			UniqueStacks.AddUnique(Stack);
		}
		Queue.PendingInterrupt.Pop();
	}

	TArray<FBreadcrumbNode> Nodes;
	for (const TSharedPtr<FBreadcrumbStack>& Stack : UniqueStacks)
	{
		if (!Stack->Scopes.IsEmpty())
		{
			if (Stack->ContextId > 0)
			{
				uint32 Scope = 0;
				do
				{
					Nodes.Emplace(CollectBreadcrumbNode(GPUProfiler, Stack, Stack->Scopes[Scope]));
					Scope = Stack->Scopes[Scope].Sibling;
				} while (Scope != 0);
			}
		}
	}

	for (const FBreadcrumbNode& Node : Nodes)
	{
		BuildBreadcrumbsDumpString(Node, GpuProgress, 0);
	}

	UE_LOG(LogD3D12RHI, Error, TEXT("%s"), *GpuProgress);

	if (!Nodes.IsEmpty())
	{
		ExportBreadcrumbDataAsCrashPayload(TEXT("RHI"), GPUQueueName, MoveTemp(Nodes));
	}

	const FD3D12DiagnosticBufferData* DiagnosticData = Queue.GetDiagnosticBufferData();
	if (DiagnosticData && DiagnosticData->Counter)
	{
		const uint32 Line = DiagnosticData->Payload.AsUint[0];
		const FString* File = UE::RHICore::GetDiagnosticMessage(DiagnosticData->Payload.AsUint[1]);
		const FString* Message = UE::RHICore::GetDiagnosticMessage(DiagnosticData->Payload.AsUint[2]);
		if (File && Message)
		{
			UE_LOG(LogD3D12RHI, Error, TEXT("[GPUBreadCrumb]\t\tShader assertion failed - %s:%d - %s"), **File, Line, **Message);
		}
		else
		{
			UE_LOG(LogD3D12RHI, Error, TEXT("[GPUBreadCrumb]\t\tShader assertion failed! ID: 0x%08X (%d)"), DiagnosticData->MessageID, DiagnosticData->MessageID);
		}

		{
			const int32* Payload = DiagnosticData->Payload.AsInt;
			if (Payload[0] < 0 || Payload[1] < 0 || Payload[2] < 0 || Payload[3] < 0)
			{
				UE_LOG(LogD3D12RHI, Error,
					TEXT("[GPUBreadCrumb]\t\tPayload  [int32]: %d %d %d %d"),
					Payload[0], Payload[1], Payload[2], Payload[3]);
			}
		}

		{
			const uint32* Payload = DiagnosticData->Payload.AsUint;
			UE_LOG(LogD3D12RHI, Error,
				TEXT("[GPUBreadCrumb]\t\tPayload [uint32]: %u %u %u %u"),
				Payload[0], Payload[1], Payload[2], Payload[3]);
		}

		{
			const uint32* Payload = DiagnosticData->Payload.AsUint;
			UE_LOG(LogD3D12RHI, Error,
				TEXT("[GPUBreadCrumb]\t\tPayload    [hex]: 0x%08X 0x%08X 0x%08X 0x%08X"),
				Payload[0], Payload[1], Payload[2], Payload[3]);
		}

		{
			const float* Payload = DiagnosticData->Payload.AsFloat;
			UE_LOG(LogD3D12RHI, Error,
				TEXT("[GPUBreadCrumb]\t\tPayload  [float]: %f %f %f %f"),
				Payload[0], Payload[1], Payload[2], Payload[3]);
		}
	}

	GLog->Panic();

	return true;
}

/** Log the GPU progress of the given Device to the Error log if breadcrumb data is available */
static void LogBreadcrumbData(ID3D12Device* Device)
{
	UE_LOG(LogD3D12RHI, Error, TEXT("[GPUBreadCrumb] Last tracked GPU operations:"));

	bool bValidData = true;

	// Check all the devices
	FD3D12DynamicRHI* D3D12RHI = FD3D12DynamicRHI::GetD3DRHI();
	D3D12RHI->ForEachDevice(Device, [&](FD3D12Device* Device)
	{
		for (uint32 QueueTypeIndex = 0; QueueTypeIndex < (uint32)ED3D12QueueType::Count; ++QueueTypeIndex)
		{
			bValidData = bValidData && LogBreadcrumbData(Device->GetGPUProfiler(), Device->GetQueue((ED3D12QueueType)QueueTypeIndex));
		}
	});

	if (!bValidData)
	{
		UE_LOG(LogD3D12RHI, Error, TEXT("No Valid GPU Breadcrumb data found. Use -gpucrashdebugging to collect GPU progress when debugging GPU crashes."));
	}
}

#if PLATFORM_WINDOWS

static TArrayView<D3D12_DRED_BREADCRUMB_CONTEXT> GetBreadcrumbContexts(const D3D12_AUTO_BREADCRUMB_NODE* Node)
{
	return {};
}

static TArrayView<D3D12_DRED_BREADCRUMB_CONTEXT> GetBreadcrumbContexts(const D3D12_AUTO_BREADCRUMB_NODE1* Node)
{
	return MakeArrayView<D3D12_DRED_BREADCRUMB_CONTEXT>(Node->pBreadcrumbContexts, Node->BreadcrumbContextsCount);
}

struct FDred_1_1
{
	FDred_1_1(ID3D12Device* Device)
	{
		if (SUCCEEDED(Device->QueryInterface(IID_PPV_ARGS(Data.GetInitReference()))))
		{
			D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
			if (SUCCEEDED(Data->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput)))
			{
				BreadcrumbHead = DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
			}
		}
	}
	TRefCountPtr<ID3D12DeviceRemovedExtendedData> Data;
	const D3D12_AUTO_BREADCRUMB_NODE* BreadcrumbHead = nullptr;
};

struct FDred_1_2
{
	FDred_1_2(ID3D12Device* Device)
	{
		if (SUCCEEDED(Device->QueryInterface(IID_PPV_ARGS(Data.GetInitReference()))))
		{
			D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 DredAutoBreadcrumbsOutput;
			if (SUCCEEDED(Data->GetAutoBreadcrumbsOutput1(&DredAutoBreadcrumbsOutput)))
			{
				BreadcrumbHead = DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
			}
		}
	}
	TRefCountPtr<ID3D12DeviceRemovedExtendedData1> Data;
	const D3D12_AUTO_BREADCRUMB_NODE1* BreadcrumbHead = nullptr;
};

// Should match all values from D3D12_AUTO_BREADCRUMB_OP
static const TCHAR* BreadcrumbOpNames[] =
{
	TEXT("SetMarker"),
	TEXT("BeginEvent"),
	TEXT("EndEvent"),
	TEXT("DrawInstanced"),
	TEXT("DrawIndexedInstanced"),
	TEXT("ExecuteIndirect"),
	TEXT("Dispatch"),
	TEXT("CopyBufferRegion"),
	TEXT("CopyTextureRegion"),
	TEXT("CopyResource"),
	TEXT("CopyTiles"),
	TEXT("ResolveSubresource"),
	TEXT("ClearRenderTargetView"),
	TEXT("ClearUnorderedAccessView"),
	TEXT("ClearDepthStencilView"),
	TEXT("ResourceBarrier"),
	TEXT("ExecuteBundle"),
	TEXT("Present"),
	TEXT("ResolveQueryData"),
	TEXT("BeginSubmission"),
	TEXT("EndSubmission"),
	TEXT("DecodeFrame"),
	TEXT("ProcessFrames"),
	TEXT("AtomicCopyBufferUint"),
	TEXT("AtomicCopyBufferUint64"),
	TEXT("ResolveSubresourceRegion"),
	TEXT("WriteBufferImmediate"),
	TEXT("DecodeFrame1"),
	TEXT("SetProtectedResourceSession"),
	TEXT("DecodeFrame2"),
	TEXT("ProcessFrames1"),
	TEXT("BuildRaytracingAccelerationStructure"),
	TEXT("EmitRaytracingAccelerationStructurePostBuildInfo"),
	TEXT("CopyRaytracingAccelerationStructure"),
	TEXT("DispatchRays"),
	TEXT("InitializeMetaCommand"),
	TEXT("ExecuteMetaCommand"),
	TEXT("EstimateMotion"),
	TEXT("ResolveMotionVectorHeap"),
	TEXT("SetPipelineState1"),
	TEXT("InitializeExtensionCommand"),
	TEXT("ExecuteExtensionCommand"),
};
static_assert(UE_ARRAY_COUNT(BreadcrumbOpNames) == D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND + 1, "OpNames array length mismatch");

/** 
 * Calculate the number of active scopes in the case of a DRED history where the number of 
 * EndEvent operations does not match the number of BeginEvent operations.
 * Practically, this would be the number of "missing" BeginEvent operations that, if added at
 * the beginning of the history, would balance out all EndEvent operations found later on.
 */
template <typename FDredNode_T>
static uint32 CalculateDREDUnknownActiveScopes(const FDredNode_T* DredNode)
{
	check(DredNode);

	int32 NumOpenEvents = 0;
	int32 MaxUnknownActiveScopes = 0;
	for (uint32 Op = 0; Op < DredNode->BreadcrumbCount; ++Op)
	{
		D3D12_AUTO_BREADCRUMB_OP BreadcrumbOp = DredNode->pCommandHistory[Op];
		if (BreadcrumbOp == D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT)
		{
			NumOpenEvents++;
		}
		else if (BreadcrumbOp == D3D12_AUTO_BREADCRUMB_OP_ENDEVENT)
		{
			NumOpenEvents--;
		}

		MaxUnknownActiveScopes = FMath::Min(NumOpenEvents, MaxUnknownActiveScopes);
	}

	return FMath::Abs(MaxUnknownActiveScopes);
}

template <typename FDredNode_T>
static TArray<FBreadcrumbNode> CollectDREDBreadcrumbNodes(const FDredNode_T* DredNode)
{
	check(DredNode && DredNode->pLastBreadcrumbValue);
	uint32 LastCompletedOp = *DredNode->pLastBreadcrumbValue;
	if (LastCompletedOp == DredNode->BreadcrumbCount || LastCompletedOp == 0)
	{
		return {};
	}

	TMap<uint32, const wchar_t*> ContextStrings;
	for (const D3D12_DRED_BREADCRUMB_CONTEXT& Context : GetBreadcrumbContexts(DredNode))
	{
		ContextStrings.Add(Context.BreadcrumbIndex, Context.pContextString);
	}

	// Create a root node that will hold all events as children. The root itself will be discarded.
	FBreadcrumbNode Root;
	Root.Name = TEXT("");
	Root.State = EBreadcrumbState::Invalid;

	TArray<FBreadcrumbNode*> ParentChain = { &Root };

	// If we have open scopes, create them now as "Unknown events".
	uint32 NumOpenScopes = CalculateDREDUnknownActiveScopes(DredNode);
	for (uint32 i = 0; i < NumOpenScopes; ++i)
	{
		FBreadcrumbNode& UnknownNode = ParentChain.Last()->Children.Emplace_GetRef();
		UnknownNode.Name = TEXT("Unknown event");
		UnknownNode.State = EBreadcrumbState::Active;
		ParentChain.Push(&UnknownNode);
	}

	for (uint32 Op = 0; Op < DredNode->BreadcrumbCount; ++Op)
	{
		D3D12_AUTO_BREADCRUMB_OP BreadcrumbOp = DredNode->pCommandHistory[Op];
		bool bCompleted = Op < LastCompletedOp;
		auto OpContextStr = ContextStrings.Find(Op);

		if (BreadcrumbOp == D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT)
		{
			// This is a begin event, potentially with children events.
			FBreadcrumbNode& BreadcrumbNode = ParentChain.Last()->Children.Emplace_GetRef();
			BreadcrumbNode.Name = OpContextStr ? *OpContextStr : TEXT("Unknown event");
			BreadcrumbNode.State = bCompleted ? EBreadcrumbState::Active : EBreadcrumbState::NotStarted;

			ParentChain.Push(&BreadcrumbNode);
		}
		else if (BreadcrumbOp == D3D12_AUTO_BREADCRUMB_OP_ENDEVENT)
		{
			FBreadcrumbNode* Parent = ParentChain.Pop();
			if (Parent->State == EBreadcrumbState::Invalid)
			{
				// If we reach this point, the DRED breadcrumbs are malformed, and some
				// basic invariants around matching BeginEvent/EndEvent do not hold.
				// Return gracefully and do not attempt to process further.
				return {};
			}

			// This is the end event for the parent node. Mark the whole event as finished
			// if this end event was completed.
			if (bCompleted && Parent->State == EBreadcrumbState::Active)
			{
				Parent->State = EBreadcrumbState::Finished;
			}
		}
		else
		{
			// This is a miscellaneous event between a BeginEvent and an EndEvent.
			const TCHAR* OpName = (BreadcrumbOp < UE_ARRAY_COUNT(BreadcrumbOpNames)) ? BreadcrumbOpNames[BreadcrumbOp] : TEXT("Unknown Op");

			FBreadcrumbNode& BreadcrumbNode = ParentChain.Last()->Children.Emplace_GetRef();
			if (OpContextStr)
			{
				BreadcrumbNode.Name = FString::Printf(TEXT("%s [%s]"), OpName, *OpContextStr);
			}
			else
			{
				BreadcrumbNode.Name = OpName;
			}
			BreadcrumbNode.State = bCompleted ? EBreadcrumbState::Finished : EBreadcrumbState::NotStarted;
		}
	}

	return Root.Children;
}

/** Log the DRED data to Error log if available */
template <typename FDred_T>
static bool LogDREDData(ID3D12Device* Device, bool bTrackingAllAllocations, D3D12_GPU_VIRTUAL_ADDRESS& OutPageFaultGPUAddress)
{
	// Should match all valid values from D3D12_DRED_ALLOCATION_TYPE
	static const TCHAR* AllocTypesNames[] =
	{
		TEXT("CommandQueue"),
		TEXT("CommandAllocator"),
		TEXT("PipelineState"),
		TEXT("CommandList"),
		TEXT("Fence"),
		TEXT("DescriptorHeap"),
		TEXT("Heap"),
		TEXT("Unknown"),				// Unknown type - missing enum value in D3D12_DRED_ALLOCATION_TYPE
		TEXT("QueryHeap"),
		TEXT("CommandSignature"),
		TEXT("PipelineLibrary"),
		TEXT("VideoDecoder"),
		TEXT("Unknown"),				// Unknown type - missing enum value in D3D12_DRED_ALLOCATION_TYPE
		TEXT("VideoProcessor"),
		TEXT("Unknown"),				// Unknown type - missing enum value in D3D12_DRED_ALLOCATION_TYPE
		TEXT("Resource"),
		TEXT("Pass"),
		TEXT("CryptoSession"),
		TEXT("CryptoSessionPolicy"),
		TEXT("ProtectedResourceSession"),
		TEXT("VideoDecoderHeap"),
		TEXT("CommandPool"),
		TEXT("CommandRecorder"),
		TEXT("StateObjectr"),
		TEXT("MetaCommand"),
		TEXT("SchedulingGroup"),
		TEXT("VideoMotionEstimator"),
		TEXT("VideoMotionVectorHeap"),
		TEXT("VideoExtensionCommand"),
	};
	static_assert(UE_ARRAY_COUNT(AllocTypesNames) == D3D12_DRED_ALLOCATION_TYPE_VIDEO_EXTENSION_COMMAND - D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE + 1, "AllocTypes array length mismatch");

	bool bHasValidBreadcrumbData = false;
	FDred_T Dred(Device);
	if (Dred.Data.IsValid())
	{
		if (Dred.BreadcrumbHead)
		{
			UE_LOG(LogD3D12RHI, Error, TEXT("DRED: Last tracked GPU operations:"));

			FString ContextStr;
			TMap<int32, const wchar_t*> ContextStrings;

			uint32 TracedCommandLists = 0;
			auto Node = Dred.BreadcrumbHead;
			while (Node && Node->pLastBreadcrumbValue)
			{
				int32 LastCompletedOp = *Node->pLastBreadcrumbValue;

				if (LastCompletedOp != Node->BreadcrumbCount && LastCompletedOp != 0)
				{
					bHasValidBreadcrumbData = true;
					UE_LOG(LogD3D12RHI, Error, TEXT("DRED: Commandlist \"%s\" on CommandQueue \"%s\", %d completed of %d"), Node->pCommandListDebugNameW, Node->pCommandQueueDebugNameW, LastCompletedOp, Node->BreadcrumbCount);
					TracedCommandLists++;

					int32 FirstOp = FMath::Max(LastCompletedOp - 100, 0);
					int32 LastOp = FMath::Min(LastCompletedOp + 20, int32(Node->BreadcrumbCount) - 1);

					ContextStrings.Reset();
					for (const D3D12_DRED_BREADCRUMB_CONTEXT& Context : GetBreadcrumbContexts(Node))
					{
						ContextStrings.Add(Context.BreadcrumbIndex, Context.pContextString);
					}

					for (int32 Op = FirstOp; Op <= LastOp; ++Op)
					{
						D3D12_AUTO_BREADCRUMB_OP BreadcrumbOp = Node->pCommandHistory[Op];

						auto OpContextStr = ContextStrings.Find(Op);
						if (OpContextStr)
						{
							ContextStr = " [";
							ContextStr += *OpContextStr;
							ContextStr += "]";
						}
						else
						{
							ContextStr.Reset();
						}

						const TCHAR* OpName = (BreadcrumbOp < UE_ARRAY_COUNT(BreadcrumbOpNames)) ? BreadcrumbOpNames[BreadcrumbOp] : TEXT("Unknown Op");
						UE_LOG(LogD3D12RHI, Error, TEXT("\tOp: %d, %s%s%s"), Op, OpName, *ContextStr, (Op + 1 == LastCompletedOp) ? TEXT(" - LAST COMPLETED") : TEXT(""));
					}

					// Collect and export breadcrumb data separately as part of the crash payload.
					TArray<FBreadcrumbNode> Breadcrumbs = CollectDREDBreadcrumbNodes(Node);
					if (!Breadcrumbs.IsEmpty())
					{
						ExportBreadcrumbDataAsCrashPayload(TEXT("DRED"), Node->pCommandQueueDebugNameW, MoveTemp(Breadcrumbs));
					}
				}

				Node = Node->pNext;
			}

			if (TracedCommandLists == 0)
			{
				UE_LOG(LogD3D12RHI, Error, TEXT("DRED: No command list found with active outstanding operations (all finished or not started yet)."));
			}
		}
		else
		{
			UE_LOG(LogD3D12RHI, Error, TEXT("DRED: No breadcrumb head found."));
		}

		FPlatformCrashContext::SetEngineData(TEXT("RHI.DREDHasBreadcrumbData"), bHasValidBreadcrumbData ? TEXT("true") : TEXT("false"));

		bool bHasValidPageFaultData = false;
		D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
		if (SUCCEEDED(Dred.Data->GetPageFaultAllocationOutput(&DredPageFaultOutput)) && DredPageFaultOutput.PageFaultVA != 0)
		{
			bHasValidPageFaultData = true;
			OutPageFaultGPUAddress = DredPageFaultOutput.PageFaultVA;
			UE_LOG(LogD3D12RHI, Error, TEXT("DRED: PageFault at VA GPUAddress \"0x%llX\""), (long long)DredPageFaultOutput.PageFaultVA);
			
			const D3D12_DRED_ALLOCATION_NODE* Node = DredPageFaultOutput.pHeadExistingAllocationNode;
			if (Node)
			{
				UE_LOG(LogD3D12RHI, Error, TEXT("DRED: Active objects with VA ranges that match the faulting VA:"));
				while (Node)
				{
					// When tracking all allocations then empty named dummy resources (heap & buffer)
					// are created for each texture to extract the GPUBaseAddress so don't write these out
					if (!bTrackingAllAllocations || Node->ObjectNameW)
					{
						int32 alloc_type_index = Node->AllocationType - D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE;
						const TCHAR* AllocTypeName = (alloc_type_index < UE_ARRAY_COUNT(AllocTypesNames)) ? AllocTypesNames[alloc_type_index] : TEXT("Unknown Alloc");
						UE_LOG(LogD3D12RHI, Error, TEXT("\tName: %s (Type: %s)"), Node->ObjectNameW, AllocTypeName);
					}
					Node = Node->pNext;
				}
			}

			Node = DredPageFaultOutput.pHeadRecentFreedAllocationNode;
			if (Node)
			{
				UE_LOG(LogD3D12RHI, Error, TEXT("DRED: Recent freed objects with VA ranges that match the faulting VA:"));
				while (Node)
				{
					// See comments above
					if (!bTrackingAllAllocations || Node->ObjectNameW)
					{
						int32 alloc_type_index = Node->AllocationType - D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE;
						const TCHAR* AllocTypeName = (alloc_type_index < UE_ARRAY_COUNT(AllocTypesNames)) ? AllocTypesNames[alloc_type_index] : TEXT("Unknown Alloc");
						UE_LOG(LogD3D12RHI, Error, TEXT("\tName: %s (Type: %s)"), Node->ObjectNameW, AllocTypeName);
					}

					Node = Node->pNext;
				}
			}
		}
		else
		{
			UE_LOG(LogD3D12RHI, Error, TEXT("DRED: No PageFault data."));
		}

		FPlatformCrashContext::SetEngineData(TEXT("RHI.DREDHasPageFaultData"), bHasValidPageFaultData ? TEXT("true") : TEXT("false"));

		return true;
	}
	else
	{
		return false;
	}
}


namespace D3D12RHI
{


static FString MakeResourceDescDebugString(const D3D12_RESOURCE_DESC& Desc)
{
	FString ResourceDescString;
	switch (Desc.Dimension)
	{
	default:
		ResourceDescString = TEXT("Unknown");
		break;
	case D3D12_RESOURCE_DIMENSION_BUFFER:
		ResourceDescString = FString::Printf(TEXT("Buffer %d bytes"), Desc.Width);
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		ResourceDescString = FString::Printf(TEXT("Texture %dx%dx%d %s"), Desc.Width, Desc.Height, Desc.DepthOrArraySize, LexToString(Desc.Format));
	}
	return ResourceDescString;
}

void LogPageFaultData(FD3D12Adapter* InAdapter, FD3D12Device* InDevice, D3D12_GPU_VIRTUAL_ADDRESS InPageFaultAddress)
{
	if (InPageFaultAddress == 0)
	{
		return;
	}

	FD3D12ManualFence& FrameFence = InAdapter->GetFrameFence();

	UE_LOG(LogD3D12RHI, Error, TEXT("PageFault: PageFault at VA GPUAddress \"0x%llX\" (GPU %d)"), (long long)InPageFaultAddress, InDevice->GetGPUIndex());
	uint64 CachedFenceValue = FrameFence.GetCompletedFenceValue(false);
	uint64 ActualFenceValue = FrameFence.GetCompletedFenceValue(true);
	uint64 NextFenceValue = FrameFence.GetNextFenceToSignal();
	UE_LOG(LogD3D12RHI, Error, TEXT("PageFault: Last completed frame ID: %d (cached: %d) - Current frame ID: %d"), ActualFenceValue, CachedFenceValue, NextFenceValue);
	UE_LOG(LogD3D12RHI, Error, TEXT("PageFault: Logging all resource enabled: %s"), InAdapter->IsTrackingAllAllocations() ? TEXT("Yes") : TEXT("No"));

	// Try and find all current allocations near that range
	static const int64 CheckRangeRadius = 16 * 1024 * 1024;
	TArray<FD3D12Adapter::FAllocatedResourceResult> OverlappingResources;
	InAdapter->FindResourcesNearGPUAddress(InPageFaultAddress, CheckRangeRadius, OverlappingResources);
	UE_LOG(LogD3D12RHI, Error, TEXT("PageFault: Found %d active tracked resources in %3.2f MB range of page fault address"), OverlappingResources.Num(), CheckRangeRadius / (1024.0f * 1024));
	if (OverlappingResources.Num() > 0)
	{
		uint32 PrintCount = FMath::Min(OverlappingResources.Num(), 100);
		for (uint32 Index = 0; Index < PrintCount; ++Index)
		{
			FD3D12Adapter::FAllocatedResourceResult OverlappingResource = OverlappingResources[Index];
			D3D12_GPU_VIRTUAL_ADDRESS ResourceAddress = OverlappingResource.Allocation->GetGPUVirtualAddress();

			const FD3D12Resource* Resource = OverlappingResource.Allocation->GetResource();
			FString ResourceDescString = MakeResourceDescDebugString(Resource->GetDesc());

			UE_LOG(LogD3D12RHI, Error, TEXT("\tGPU Address: [0x%llX .. 0x%llX] - Size: %lld bytes, %3.2f MB - Distance to page fault: %lld bytes, %3.2f MB - Transient: %d - Name: %s - Desc: %s"),
				(uint64)ResourceAddress,
				(uint64)ResourceAddress + OverlappingResource.Allocation->GetSize(),
				OverlappingResource.Allocation->GetSize(),
				OverlappingResource.Allocation->GetSize() / (1024.0f * 1024), 
				OverlappingResource.Distance,
				OverlappingResource.Distance / (1024.0f * 1024), 
				OverlappingResource.Allocation->IsTransient(), 
				*Resource->GetName().ToString(),
				*ResourceDescString);
		}
	}

	// Try and find all current heaps containing the page fault address
	TArray<FD3D12Heap*> OverlappingHeaps;
	InAdapter->FindHeapsContainingGPUAddress(InPageFaultAddress, OverlappingHeaps);
	UE_LOG(LogD3D12RHI, Error, TEXT("PageFault: Found %d active heaps containing page fault address"), OverlappingHeaps.Num());
	for (int32 Index = 0; Index < OverlappingHeaps.Num(); ++Index)
	{
		FD3D12Heap* Heap = OverlappingHeaps[Index];
		UE_LOG(LogD3D12RHI, Error, TEXT("\tGPU Address: \"0x%llX\" - Size: %3.2f MB - Name: %s"),
			(long long)Heap->GetGPUVirtualAddress(), Heap->GetHeapDesc().SizeInBytes / (1024.0f * 1024), *(Heap->GetName().ToString()));
	}

	// Try and find all released allocations within the faulting address
	TArray<FD3D12Adapter::FReleasedAllocationData> ReleasedResources;
	InAdapter->FindReleasedAllocationData(InPageFaultAddress, ReleasedResources);
	UE_LOG(LogD3D12RHI, Error, TEXT("PageFault: Found %d released resources containing the page fault address during last 100 frames"), ReleasedResources.Num());
	if (ReleasedResources.Num() > 0)
	{
		uint32 PrintCount = FMath::Min(ReleasedResources.Num(), 100);
		for (uint32 Index = 0; Index < PrintCount; ++Index)
		{
			FD3D12Adapter::FReleasedAllocationData& AllocationData = ReleasedResources[Index];

			FString ResourceDescString = MakeResourceDescDebugString(AllocationData.ResourceDesc);

			UE_LOG(LogD3D12RHI, Error, TEXT("\tGPU Address: [0x%llX .. 0x%llX] - Size: %lld bytes, %3.2f MB - FrameID: %4d - DefragFree: %d - Transient: %d - Heap: %d - Name: %s - Desc: %s"),
				(uint64)AllocationData.GPUVirtualAddress,
				(uint64)AllocationData.GPUVirtualAddress + AllocationData.AllocationSize,
				AllocationData.AllocationSize,
				AllocationData.AllocationSize / (1024.0f * 1024),
				AllocationData.ReleasedFrameID,
				AllocationData.bDefragFree,
				AllocationData.bTransient,
				AllocationData.bHeap,
				*AllocationData.ResourceName.ToString(),
				*ResourceDescString);
		}
	}
}

} // namespace D3D12RHI

void LogMemoryInfo(FD3D12Adapter* InAdapter)
{	
	const FD3D12MemoryInfo& MemoryInfo = InAdapter->GetMemoryInfo();

	UE_LOG(LogD3D12RHI, Error, TEXT("Memory Info from frame ID %d:"), MemoryInfo.UpdateFrameNumber);
	UE_LOG(LogD3D12RHI, Error, TEXT("\tBudget:\t%7.2f MB"), MemoryInfo.LocalMemoryInfo.Budget / (1024.0f * 1024));
	UE_LOG(LogD3D12RHI, Error, TEXT("\tUsed:\t%7.2f MB"), MemoryInfo.LocalMemoryInfo.CurrentUsage / (1024.0f * 1024));
}

#endif  // PLATFORM_WINDOWS

extern CORE_API bool GIsGPUCrashed;

static void TerminateOnOutOfMemory(ID3D12Device* InDevice, HRESULT D3DResult, bool bCreatingTextures)
{
#if PLATFORM_WINDOWS
	// send telemetry event with current adapter's memory info
	FD3D12DynamicRHI* D3D12RHI = FD3D12DynamicRHI::GetD3DRHI();
	FD3D12Adapter* Adapter = nullptr;
	D3D12RHI->ForEachDevice(InDevice, [&](FD3D12Device* IterationDevice)
		{
			if (InDevice == IterationDevice->GetDevice())
			{
				Adapter = IterationDevice->GetParentAdapter();
			}
		});
	// if InDevice == nullptr, just pick the first available adapter
	if (!Adapter && D3D12RHI->GetNumAdapters() == 1)
	{
		check(!InDevice);
		Adapter = &D3D12RHI->GetAdapter(0);
	}
	if (Adapter)
	{
		const auto& MemoryInfo = Adapter->GetMemoryInfo().LocalMemoryInfo;
		FCoreDelegates::GetGPUOutOfMemoryDelegate().Broadcast(MemoryInfo.Budget, MemoryInfo.CurrentUsage);
	}

	if (!FApp::IsUnattended())
	{
		if (bCreatingTextures)
		{
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *LOCTEXT("OutOfVideoMemoryTextures", "Out of video memory trying to allocate a texture! Make sure your video card has the minimum required memory, try lowering the resolution and/or closing other applications that are running. Exiting...").ToString(), TEXT("Error"));
		}
		else
		{
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *NSLOCTEXT("D3D12RHI", "OutOfMemory", "Out of video memory trying to allocate a rendering resource. Make sure your video card has the minimum required memory, try lowering the resolution and/or closing other applications that are running. Exiting...").ToString(), TEXT("Error"));
		}
	}

#if STATS
	GetRendererModule().DebugLogOnCrash();
#endif

	static IConsoleVariable* GPUCrashOOM = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashOnOutOfMemory"));
	const bool bGPUCrashOOM = GPUCrashOOM && GPUCrashOOM->GetInt();
	if (bGPUCrashOOM)
	{
		// If no device provided then try and log the DRED status of each device
		D3D12RHI->ForEachDevice(InDevice, [&](FD3D12Device* IterationDevice)
			{
				if (InDevice == nullptr || InDevice == IterationDevice->GetDevice())
				{
					FD3D12Adapter* Adapter = IterationDevice->GetParentAdapter();
					LogMemoryInfo(Adapter);
				}
			});
	}
	
	UE_LOG(LogD3D12RHI, Fatal, TEXT("Out of video memory trying to allocate a rendering resource"));
	if (!bGPUCrashOOM)
	{
		// Exit silently without reporting a crash because an OOM is not necessarily our fault		
		FPlatformMisc::RequestExit(true, TEXT("D3D12Util.TerminateOnOutOfMemory"));
	}

#else // PLATFORM_WINDOWS
	UE_LOG(LogInit, Fatal, TEXT("Out of video memory trying to allocate a rendering resource"));
#endif // !PLATFORM_WINDOWS
}

namespace D3D12RHI
{
	void TerminateOnGPUCrash(ID3D12Device* InDevice)
	{		
		// This function can be called outside of VerifyD3D12Result & co, so it uses its own critical section to make sure it's not re-entered.
		static FCriticalSection cs;
		cs.Lock();

		// Mark critical and gpu crash
		GIsCriticalError = true;
		GIsGPUCrashed = true;

		// Check GPU heartbeat - will trace Aftermath state
		if (GDynamicRHI)
		{
			GDynamicRHI->CheckGpuHeartbeat();
		}

		// Log RHI independent breadcrumbing data
		LogBreadcrumbData(InDevice);

		FD3D12DynamicRHI* D3D12RHI = FD3D12DynamicRHI::GetD3DRHI();
#if PLATFORM_WINDOWS
		// If no device provided then try and log the DRED status of each device
		D3D12RHI->ForEachDevice(InDevice, [&](FD3D12Device* IterationDevice)
			{
				if (InDevice == nullptr || InDevice == IterationDevice->GetDevice())
				{
					D3D12_GPU_VIRTUAL_ADDRESS PageFaultAddress = 0;
					bool bIsTrackingAllAllocations = IterationDevice->GetParentAdapter()->IsTrackingAllAllocations();
					if (!LogDREDData<FDred_1_2>(IterationDevice->GetDevice(), bIsTrackingAllAllocations, PageFaultAddress))
					{
						if (!LogDREDData<FDred_1_1>(IterationDevice->GetDevice(), bIsTrackingAllAllocations, PageFaultAddress))
						{
							UE_LOG(LogD3D12RHI, Error, TEXT("DRED: could not find DRED data (might not be enabled or available). Run with -dred or -gpucrashdebugging to enable dred if available."));
						}						
					}

					FD3D12Adapter* Adapter = IterationDevice->GetParentAdapter();
					LogPageFaultData(Adapter, IterationDevice, PageFaultAddress);
					LogMemoryInfo(Adapter);
				}
			});
#endif  // PLATFORM_WINDOWS

#if NV_AFTERMATH
		GFSDK_Aftermath_CrashDump_Status AftermathStatus{};
		if (GDX12NVAfterMathEnabled)
		{
			GFSDK_Aftermath_GetCrashDumpStatus(&AftermathStatus);
			if (AftermathStatus != GFSDK_Aftermath_CrashDump_Status_Unknown && AftermathStatus != GFSDK_Aftermath_CrashDump_Status_NotStarted)
			{
				const float StartTime = FPlatformTime::Seconds();
				const float EndTime = StartTime + GDX12NVAfterMathDumpWaitTime;
				while (AftermathStatus != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed
					&& AftermathStatus != GFSDK_Aftermath_CrashDump_Status_Finished
					&& FPlatformTime::Seconds() < EndTime)
				{
					FPlatformProcess::Sleep(0.01f);
					GFSDK_Aftermath_GetCrashDumpStatus(&AftermathStatus);
				}
			}
		}
#endif
		
		// Build the error message
		FTextBuilder ErrorMessage;
		ErrorMessage.AppendLine(LOCTEXT("GPU Crashed", "GPU Crashed or D3D Device Removed.\n"));
		if (D3D12RHI->GetAdapter().GetGPUCrashDebuggingModes() != ED3D12GPUCrashDebuggingModes::None)
		{
			ErrorMessage.AppendLine(LOCTEXT("GPU Crash Debugging enabled", "Check log for GPU state information."));
		}
		else
		{
			ErrorMessage.AppendLine(LOCTEXT("GPU Crash Debugging disabled", "Use -gpucrashdebugging to enable GPU state tracking."));
		}

		// And info on gpu crash dump as well
#if NV_AFTERMATH
		if (AftermathStatus == GFSDK_Aftermath_CrashDump_Status_Finished)
		{
			ErrorMessage.AppendLine(LOCTEXT("GPU CrashDump", "\nA GPU mini dump was be saved in the Logs folder."));
		}
#endif
		
		// Make sure the log is flushed!
		GLog->Panic();

		// Show message box or trace information
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!FApp::IsUnattended() && !IsDebuggerPresent())
		{
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage.ToText().ToString(), TEXT("Error"));
		}
		else
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		{
			UE_LOG(LogD3D12RHI, D3D12RHI_GPU_CRASH_LOG_VERBOSITY, TEXT("%s"), *ErrorMessage.ToText().ToString());
		}

		// hard break here when the debugger is attached
		if (IsDebuggerPresent())
		{
			UE_DEBUG_BREAK();
		}

#if PLATFORM_WINDOWS
		ReportGPUCrash(TEXT("GPU Crash dump Triggered"), nullptr);
#endif

		// Force shutdown, we can't do anything useful anymore.
		FPlatformMisc::RequestExit(true, TEXT("D3D12Util.TerminateOnGPUCrash"));
	}

	// It's possible for multiple threads to catch GPU crashes or other D3D errors at the same time. Make sure we only log the error once by acquiring
	// this critical section inside VerifyD3D12Result and VerifyD3D12CreateTextureResult (and never releasing it, because those functions don't return).
	static FCriticalSection GD3DCallFailedCS;

	void VerifyD3D12Result(HRESULT D3DResult, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line, ID3D12Device* Device, FString Message)
	{
		check(FAILED(D3DResult));
		
		GD3DCallFailedCS.Lock();

		const FString& ErrorString = GetD3D12ErrorString(D3DResult, Device);
		UE_LOG(LogD3D12RHI, Error, TEXT("%s failed \n at %s:%u \n with error %s\n%s"), ANSI_TO_TCHAR(Code), ANSI_TO_TCHAR(Filename), Line, *ErrorString, *Message);
		
		if (D3DResult == E_OUTOFMEMORY)
		{
			TerminateOnOutOfMemory(Device, D3DResult, false);
		}
		else if (D3DResult == DXGI_ERROR_DEVICE_REMOVED || D3DResult == DXGI_ERROR_DEVICE_HUNG || D3DResult == DXGI_ERROR_DEVICE_RESET)
		{
			TerminateOnGPUCrash(Device);
		}

		// Make sure the log is flushed!
		GLog->Panic();

		UE_LOG(LogD3D12RHI, Fatal, TEXT("%s failed \n at %s:%u \n with error %s\n%s"), ANSI_TO_TCHAR(Code), ANSI_TO_TCHAR(Filename), Line, *ErrorString, *Message);

		// Force shutdown, we can't do anything useful anymore.
		FPlatformMisc::RequestExit(true, TEXT("D3D12Util.VerifyD3D12Result"));
	}

	void VerifyD3D12CreateTextureResult(HRESULT D3DResult, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line, const D3D12_RESOURCE_DESC& TextureDesc, ID3D12Device* Device)
	{
		check(FAILED(D3DResult));

		GD3DCallFailedCS.Lock();

		const FString ErrorString = GetD3D12ErrorString(D3DResult, nullptr);
		const TCHAR* D3DFormatString = UE::DXGIUtilities::GetFormatString(TextureDesc.Format);

		UE_LOG(LogD3D12RHI, Error,
			TEXT("%s failed \n at %s:%u \n with error %s, \n Size=%ix%ix%i Format=%s(0x%08X), NumMips=%i, Flags=%s"),
			ANSI_TO_TCHAR(Code),
			ANSI_TO_TCHAR(Filename),
			Line,
			*ErrorString,
			TextureDesc.Width,
			TextureDesc.Height,
			TextureDesc.DepthOrArraySize,
			D3DFormatString,
			TextureDesc.Format,
			TextureDesc.MipLevels,
			*GetD3D12TextureFlagString(TextureDesc.Flags));

		// Terminate with device removed but we don't have any GPU crash dump information
		if (D3DResult == DXGI_ERROR_DEVICE_REMOVED || D3DResult == DXGI_ERROR_DEVICE_HUNG || D3DResult == DXGI_ERROR_DEVICE_RESET)
		{
			TerminateOnGPUCrash(Device);
		}
		else if (D3DResult == E_OUTOFMEMORY)
		{
			TerminateOnOutOfMemory(Device, D3DResult, true);

#if STATS
			GetRendererModule().DebugLogOnCrash();
#endif // STATS
		}

		// Make sure the log is flushed!
		GLog->Panic();

		UE_LOG(LogD3D12RHI, Fatal,
			TEXT("%s failed \n at %s:%u \n with error %s, \n Size=%ix%ix%i Format=%s(0x%08X), NumMips=%i, Flags=%s"),
			ANSI_TO_TCHAR(Code),
			ANSI_TO_TCHAR(Filename),
			Line,
			*ErrorString,
			TextureDesc.Width,
			TextureDesc.Height,
			TextureDesc.DepthOrArraySize,
			D3DFormatString,
			TextureDesc.Format,
			TextureDesc.MipLevels,
			*GetD3D12TextureFlagString(TextureDesc.Flags));

		// Force shutdown, we can't do anything useful anymore.
		FPlatformMisc::RequestExit(true, TEXT("D3D12Util.VerifyD3D12CreateTextureResult"));
	}

	void VerifyComRefCount(IUnknown* Object, int32 ExpectedRefs, const TCHAR* Code, const TCHAR* Filename, int32 Line)
	{
		int32 NumRefs;

		if (Object)
		{
			Object->AddRef();
			NumRefs = Object->Release();

			checkSlow(NumRefs != ExpectedRefs);

			if (NumRefs != ExpectedRefs)
			{
				UE_LOG(
					LogD3D12RHI,
					Error,
					TEXT("%s:(%d): %s has %d refs, expected %d"),
					Filename,
					Line,
					Code,
					NumRefs,
					ExpectedRefs
					);
			}
		}
	}
}

void FD3D12QuantizedBoundShaderState::InitShaderRegisterCounts(const D3D12_RESOURCE_BINDING_TIER ResourceBindingTier, const FShaderCodePackedResourceCounts& Counts, FShaderRegisterCounts& Shader, bool bAllowUAVs)
{
	uint32 MaxSRVs = MAX_SRVS;
	uint32 MaxSamplers = MAX_SAMPLERS;
	uint32 MaxUAVs = MAX_UAVS;
	uint32 MaxCBs = MAX_CBS;

	// On tier 1 & 2 HW the actual descriptor table size used during the draw/dispatch must match that of the
	// root signature so we round the size up to the closest power of 2 to accomplish 2 goals: 1) keep the size of
	// the table closer to the required size to limit descriptor heap usage due to required empty descriptors,
	// 2) encourage root signature reuse by having other shader root signature table sizes fall within the size rounding.
	// Sampler and Shader resouce view table sizes must match signature on Tier 1 hardware and Constant buffer and
	// Unorded access views table sizes must match signature on tier 2 hardware. On hardware > tier 2 the actual descriptor
	// table size used during the draw/dispatch doesn't need to match the root signature size so we encourage reuse by using
	// the max size. More info here: https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support,
	// https://en.wikipedia.org/wiki/Feature_levels_in_Direct3D

	// To reduce the size of the root signature, we only allow UAVs for certain shaders. 
	// This code makes the assumption that the engine only uses UAVs at the PS or CS shader stages.
	check(bAllowUAVs || (!bAllowUAVs && Counts.NumUAVs == 0));

	if (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_1)
	{
		Shader.SamplerCount = (Counts.NumSamplers > 0) ? FMath::Min(MaxSamplers, FMath::RoundUpToPowerOfTwo(Counts.NumSamplers)) : Counts.NumSamplers;
		Shader.ShaderResourceCount = (Counts.NumSRVs > 0) ? FMath::Min(MaxSRVs, FMath::RoundUpToPowerOfTwo(Counts.NumSRVs)) : Counts.NumSRVs;
	}
	else
	{
		Shader.SamplerCount = Counts.NumSamplers > 0 ? MaxSamplers : 0;
		Shader.ShaderResourceCount = Counts.NumSRVs > 0 ? MaxSRVs : 0;
	}

	if (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
	{
		Shader.ConstantBufferCount = (Counts.NumCBs > MAX_ROOT_CBVS) ? FMath::Min(MaxCBs, FMath::RoundUpToPowerOfTwo(Counts.NumCBs)) : Counts.NumCBs;
		Shader.UnorderedAccessCount = (Counts.NumUAVs > 0 && bAllowUAVs) ? FMath::Min(MaxUAVs, FMath::RoundUpToPowerOfTwo(Counts.NumUAVs)) : 0;
	}
	else
	{
		Shader.ConstantBufferCount = (Counts.NumCBs > MAX_ROOT_CBVS) ? MaxCBs : Counts.NumCBs;
		Shader.UnorderedAccessCount = (Counts.NumUAVs > 0 && bAllowUAVs) ? MaxUAVs : 0;
	}
}

bool NeedsAgsIntrinsicsSpace(const FD3D12ShaderData& ShaderData)
{
#if D3D12RHI_NEEDS_VENDOR_EXTENSIONS
	for (const FShaderCodeVendorExtension& Extension : ShaderData.VendorExtensions)
	{
		if (Extension.VendorId == EGpuVendorId::Amd)
		{
			// https://github.com/GPUOpen-LibrariesAndSDKs/AGS_SDK/blob/master/ags_lib/hlsl/ags_shader_intrinsics_dx12.hlsl
			return true;
		}
	}
#endif

	return false;
}

static void SetBoundShaderStateFlags(FD3D12QuantizedBoundShaderState& OutQBSS, const FD3D12ShaderData* ShaderData)
{
	if (ShaderData)
	{
		OutQBSS.bUseDiagnosticBuffer |= ShaderData->UsesDiagnosticBuffer();
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
		OutQBSS.bUseDirectlyIndexedResourceHeap |= ShaderData->UsesBindlessResources();
		OutQBSS.bUseDirectlyIndexedSamplerHeap |= ShaderData->UsesBindlessSamplers();
#endif
		if (GRHISupportsShaderRootConstants)
		{
			OutQBSS.bUseRootConstants |= ShaderData->UsesRootConstants();
		}
	}
}

static void QuantizeBoundShaderStateCommon(
	FD3D12QuantizedBoundShaderState& OutQBSS,
	const FD3D12ShaderData* ShaderData,
	D3D12_RESOURCE_BINDING_TIER ResourceBindingTier,
	EShaderVisibility ShaderVisibility,
	bool bAllowUAVs = false
)
{
	if (ShaderData)
	{
		FD3D12QuantizedBoundShaderState::InitShaderRegisterCounts(ResourceBindingTier, ShaderData->ResourceCounts, OutQBSS.RegisterCounts[ShaderVisibility], bAllowUAVs);
		OutQBSS.bNeedsAgsIntrinsicsSpace |= NeedsAgsIntrinsicsSpace(*ShaderData);
	}

	SetBoundShaderStateFlags(OutQBSS, ShaderData);
}

static bool IsCompatibleWithBindlessSamplers(const FD3D12ShaderData* ShaderData)
{
	if (ensure(ShaderData))
	{
		return ShaderData->UsesBindlessSamplers()
			|| ShaderData->ResourceCounts.NumSamplers == 0;
	}
	return true;
}

static bool IsCompatibleWithBindlessResources(const FD3D12ShaderData* ShaderData)
{
	if (ensure(ShaderData))
	{
		return ShaderData->UsesBindlessResources()
			|| (ShaderData->ResourceCounts.NumSRVs + ShaderData->ResourceCounts.NumUAVs) == 0;
	}
	return true;
}

inline bool BSSUsesRootConstants(const FBoundShaderStateInput& BSS)
{
	if (!GRHISupportsShaderRootConstants)
	{
		return false;
	}

	TArray<const FD3D12ShaderData*, TInlineAllocator<5>> ShaderData;

	ShaderData.Add(FD3D12DynamicRHI::ResourceCast(BSS.GetVertexShader()));
#if PLATFORM_SUPPORTS_MESH_SHADERS
	ShaderData.Add(FD3D12DynamicRHI::ResourceCast(BSS.GetMeshShader()));
	ShaderData.Add(FD3D12DynamicRHI::ResourceCast(BSS.GetAmplificationShader()));
#endif
	ShaderData.Add(FD3D12DynamicRHI::ResourceCast(BSS.GetPixelShader()));
	ShaderData.Add(FD3D12DynamicRHI::ResourceCast(BSS.GetGeometryShader()));

	bool bUsesRootConstants = false;
	for (int32 DataIndex = 0; DataIndex < ShaderData.Num(); ++DataIndex)
	{
		if (ShaderData[DataIndex] == nullptr)
		{
			continue;
		}

		bUsesRootConstants = EnumHasAnyFlags(ShaderData[DataIndex]->ResourceCounts.UsageFlags, EShaderResourceUsageFlags::RootConstants);

		if (bUsesRootConstants)
		{
			break;
		}
	}

	return bUsesRootConstants;
}
	
const FD3D12RootSignature* FD3D12Adapter::GetRootSignature(const FBoundShaderStateInput& BSS)
{
#if USE_STATIC_ROOT_SIGNATURE

	if (BSSUsesRootConstants(BSS))
	{
		return &StaticGraphicsWithConstantsRootSignature;
	}
	else
	{
		return &StaticGraphicsRootSignature;
	}

#else //! USE_STATIC_ROOT_SIGNATURE

	// BSS quantizer. There is a 1:1 mapping of quantized bound shader state objects to root signatures.
	// The objective is to allow a single root signature to represent many bound shader state objects.
	// The bigger the quantization step sizes, the fewer the root signatures.
	FD3D12QuantizedBoundShaderState QBSS{};

	QBSS.bAllowIAInputLayout = BSS.VertexDeclarationRHI != nullptr;	// Does the root signature need access to vertex buffers?

	const D3D12_RESOURCE_BINDING_TIER ResourceBindingTier = GetResourceBindingTier();

	QuantizeBoundShaderStateCommon(QBSS, FD3D12DynamicRHI::ResourceCast(BSS.GetVertexShader()),        ResourceBindingTier, SV_Vertex);
#if PLATFORM_SUPPORTS_MESH_SHADERS
	QuantizeBoundShaderStateCommon(QBSS, FD3D12DynamicRHI::ResourceCast(BSS.GetMeshShader()),          ResourceBindingTier, SV_Mesh);
	QuantizeBoundShaderStateCommon(QBSS, FD3D12DynamicRHI::ResourceCast(BSS.GetAmplificationShader()), ResourceBindingTier, SV_Amplification);
#endif
	QuantizeBoundShaderStateCommon(QBSS, FD3D12DynamicRHI::ResourceCast(BSS.GetPixelShader()),         ResourceBindingTier, SV_Pixel, true /*bAllowUAVs*/);
	QuantizeBoundShaderStateCommon(QBSS, FD3D12DynamicRHI::ResourceCast(BSS.GetGeometryShader()),      ResourceBindingTier, SV_Geometry);

#if DO_CHECK && PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (QBSS.bUseDirectlyIndexedResourceHeap || QBSS.bUseDirectlyIndexedSamplerHeap)
	{
		struct FGenericShaderPair
		{
			const FD3D12ShaderData* Data;
			const FRHIGraphicsShader* RHI;
		};
		const FGenericShaderPair ShaderDatas[] =
		{
			{ FD3D12DynamicRHI::ResourceCast(BSS.GetVertexShader()), BSS.GetVertexShader() },
#if PLATFORM_SUPPORTS_MESH_SHADERS
			{ FD3D12DynamicRHI::ResourceCast(BSS.GetMeshShader()), BSS.GetMeshShader() },
			{ FD3D12DynamicRHI::ResourceCast(BSS.GetAmplificationShader()), BSS.GetAmplificationShader() },
#endif
			{ FD3D12DynamicRHI::ResourceCast(BSS.GetPixelShader()), BSS.GetPixelShader() },
			{ FD3D12DynamicRHI::ResourceCast(BSS.GetGeometryShader()), BSS.GetGeometryShader() },
		};

		for (const FGenericShaderPair& ShaderPair : ShaderDatas)
		{
			if (ShaderPair.RHI)
			{
				if (QBSS.bUseDirectlyIndexedResourceHeap)
				{
					checkf(IsCompatibleWithBindlessResources(ShaderPair.Data), TEXT("Mismatched dynamic resource usage. %s doesn't support binding with stages that use dynamic resources"), ShaderPair.RHI->GetShaderName());
				}
				if (QBSS.bUseDirectlyIndexedSamplerHeap)
				{
					checkf(IsCompatibleWithBindlessSamplers(ShaderPair.Data), TEXT("Mismatched dynamic resource usage. %s doesn't support binding with stages that use dynamic samplers"), ShaderPair.RHI->GetShaderName());
				}
			}
		}
	}
#endif

	return RootSignatureManager.GetRootSignature(QBSS);

#endif //! USE_STATIC_ROOT_SIGNATURE
}

const FD3D12RootSignature* FD3D12Adapter::GetRootSignature(const FD3D12ComputeShader* ComputeShader)
{
#if USE_STATIC_ROOT_SIGNATURE

	if (ComputeShader->UsesRootConstants() && GRHISupportsShaderRootConstants)
	{
		return &StaticComputeWithConstantsRootSignature;
	}
	else
	{
		return &StaticComputeRootSignature;
	}

#else //! USE_STATIC_ROOT_SIGNATURE

	check(ComputeShader);

	// BSS quantizer. There is a 1:1 mapping of quantized bound shader state objects to root signatures.
	// The objective is to allow a single root signature to represent many bound shader state objects.
	// The bigger the quantization step sizes, the fewer the root signatures.
	FD3D12QuantizedBoundShaderState QBSS{};

	QuantizeBoundShaderStateCommon(QBSS, ComputeShader, GetResourceBindingTier(), SV_All, true /*bAllowUAVs*/);

	check(QBSS.bAllowIAInputLayout == false); // No access to vertex buffers needed

	return RootSignatureManager.GetRootSignature(QBSS);

#endif //! USE_STATIC_ROOT_SIGNATURE
}

#if D3D12_RHI_RAYTRACING

const FD3D12RootSignature* FD3D12Adapter::GetGlobalRayTracingRootSignature()
{
#if USE_STATIC_ROOT_SIGNATURE

	return &StaticRayTracingGlobalRootSignature;

#else //!USE_STATIC_ROOT_SIGNATURE

	FD3D12QuantizedBoundShaderState QBSS{};
	FShaderRegisterCounts& QBSSRegisterCounts = QBSS.RegisterCounts[SV_All];

	QBSS.RootSignatureType = RS_RayTracingGlobal;
	QBSS.bUseDiagnosticBuffer = true;

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	QBSS.bUseDirectlyIndexedResourceHeap = bBindlessResourcesAllowed;
	QBSS.bUseDirectlyIndexedSamplerHeap = bBindlessSamplersAllowed;
#endif

	QBSSRegisterCounts.SamplerCount = MAX_SAMPLERS;
	QBSSRegisterCounts.ShaderResourceCount = MAX_SRVS;
	QBSSRegisterCounts.ConstantBufferCount = MAX_CBS;
	QBSSRegisterCounts.UnorderedAccessCount = MAX_UAVS;

	return RootSignatureManager.GetRootSignature(QBSS);

#endif //! USE_STATIC_ROOT_SIGNATURE
}

const FD3D12RootSignature* FD3D12Adapter::GetRootSignature(const FD3D12RayTracingShader* RayTracingShader)
{
#if USE_STATIC_ROOT_SIGNATURE

	switch (RayTracingShader->GetFrequency())
	{
	default:
		checkNoEntry(); // Unexpected shader target frequency
		return nullptr;

	case SF_RayGen:
		return &StaticRayTracingGlobalRootSignature;

	case SF_RayHitGroup:
	case SF_RayCallable:
	case SF_RayMiss:
		return &StaticRayTracingLocalRootSignature;
	}

#else //! USE_STATIC_ROOT_SIGNATURE

	FD3D12QuantizedBoundShaderState QBSS{};

	FShaderRegisterCounts& QBSSRegisterCounts = QBSS.RegisterCounts[SV_All];

	switch (RayTracingShader->GetFrequency())
	{
	case SF_RayGen:
		// Shared conservative root signature layout is used for all raygen and miss shaders.
		return GetGlobalRayTracingRootSignature();

	case SF_RayHitGroup:
	case SF_RayCallable:
	case SF_RayMiss:
	{
		// Local root signature is used for hit group shaders, using the exact number of resources to minimize shader binding table record size.
		check(RayTracingShader);
		const FShaderCodePackedResourceCounts& Counts = RayTracingShader->ResourceCounts;

		QBSS.RootSignatureType = RS_RayTracingLocal;

		QBSSRegisterCounts.SamplerCount = Counts.NumSamplers;
		QBSSRegisterCounts.ShaderResourceCount = Counts.NumSRVs;
		QBSSRegisterCounts.ConstantBufferCount = Counts.NumCBs;
		QBSSRegisterCounts.UnorderedAccessCount = Counts.NumUAVs;

		check(QBSSRegisterCounts.SamplerCount <= MAX_SAMPLERS);
		check(QBSSRegisterCounts.ShaderResourceCount <= MAX_SRVS);
		check(QBSSRegisterCounts.ConstantBufferCount <= MAX_CBS);
		check(QBSSRegisterCounts.UnorderedAccessCount <= MAX_UAVS);

		break;
	}
	default:
		checkNoEntry(); // Unexpected shader target frequency
	}

	SetBoundShaderStateFlags(QBSS, RayTracingShader);

	return RootSignatureManager.GetRootSignature(QBSS);

#endif //! USE_STATIC_ROOT_SIGNATURE
}
#endif // D3D12_RHI_RAYTRACING

FD3D12BoundRenderTargets::FD3D12BoundRenderTargets(FD3D12RenderTargetView** RTArray, uint32 NumActiveRTs, FD3D12DepthStencilView* DSView)
{
	FMemory::Memcpy(RenderTargetViews, RTArray, sizeof(RenderTargetViews));
	DepthStencilView = DSView;
	NumActiveTargets = NumActiveRTs;
}

FD3D12BoundRenderTargets::~FD3D12BoundRenderTargets()
{
}

void LogExecuteCommandLists(uint32 NumCommandLists, ID3D12CommandList* const* ppCommandLists)
{
	for (uint32 i = 0; i < NumCommandLists; i++)
	{
		ID3D12CommandList* const pCurrentCommandList = ppCommandLists[i];
		UE_LOG(LogD3D12RHI, Log, TEXT("*** [tid:%08x] EXECUTE (CmdList: %016llX) %u/%u ***"), FPlatformTLS::GetCurrentThreadId(), pCurrentCommandList, i + 1, NumCommandLists);
	}
}

FString ConvertToResourceStateString(uint32 ResourceState)
{
	if (ResourceState == 0)
	{
		return TEXT("D3D12_RESOURCE_STATE_COMMON");
	}

	if (ResourceState == D3D12_RESOURCE_STATE_TBD)
	{
		return TEXT("D3D12_RESOURCE_STATE_TBD");
	}

	const TCHAR* ResourceStateNames[] =
	{
		TEXT("D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER"),
		TEXT("D3D12_RESOURCE_STATE_INDEX_BUFFER"),
		TEXT("D3D12_RESOURCE_STATE_RENDER_TARGET"),
		TEXT("D3D12_RESOURCE_STATE_UNORDERED_ACCESS"),
		TEXT("D3D12_RESOURCE_STATE_DEPTH_WRITE"),
		TEXT("D3D12_RESOURCE_STATE_DEPTH_READ"),
		TEXT("D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE"),
		TEXT("D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE"),
		TEXT("D3D12_RESOURCE_STATE_STREAM_OUT"),
		TEXT("D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT"),
		TEXT("D3D12_RESOURCE_STATE_COPY_DEST"),
		TEXT("D3D12_RESOURCE_STATE_COPY_SOURCE"),
		TEXT("D3D12_RESOURCE_STATE_RESOLVE_DEST"),
		TEXT("D3D12_RESOURCE_STATE_RESOLVE_SOURCE"),
	};

	FString ResourceStateString;
	uint16 NumStates = 0;
	for (uint16 i = 0; ResourceState && i < ARRAYSIZE(ResourceStateNames); i++)
	{
		if (ResourceState & 1)
		{
			if (NumStates > 0)
			{
				ResourceStateString += " | ";
			}

			ResourceStateString += ResourceStateNames[i];
			NumStates++;
		}
		ResourceState = ResourceState >> 1;
	}
	return ResourceStateString;
}

void LogResourceBarriers(TConstArrayView<D3D12_RESOURCE_BARRIER> Barriers, ID3D12CommandList* const pCommandList)
{
	// Configure what resource barriers are logged.
	const bool bLogAll = false;
	const bool bLogTransitionDepth = true;
	const bool bLogTransitionRenderTarget = true;
	const bool bLogTransitionUAV = true;

	// Create the state bit mask to indicate what barriers should be logged.
	uint32 ShouldLogMask = bLogAll ? static_cast<uint32>(-1) : 0;
	ShouldLogMask |= bLogTransitionDepth ? D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE : 0;
	ShouldLogMask |= bLogTransitionRenderTarget ? D3D12_RESOURCE_STATE_RENDER_TARGET : 0;
	ShouldLogMask |= bLogTransitionUAV ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : 0;

	for (int32 i = 0; i < Barriers.Num(); i++)
	{
		const D3D12_RESOURCE_BARRIER& currentBarrier = Barriers[i];

		switch (currentBarrier.Type)
		{
		case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
		{
			const FString StateBefore = ConvertToResourceStateString(static_cast<uint32>(currentBarrier.Transition.StateBefore));
			const FString StateAfter = ConvertToResourceStateString(static_cast<uint32>(currentBarrier.Transition.StateAfter));

			bool bShouldLog = bLogAll;
			if (!bShouldLog)
			{
				// See if we should log this transition.
				for (uint32 j = 0; (j < 2) && !bShouldLog; j++)
				{
					const D3D12_RESOURCE_STATES& State = (j == 0) ? currentBarrier.Transition.StateBefore : currentBarrier.Transition.StateAfter;
					bShouldLog = (State & ShouldLogMask) > 0;
				}
			}

			if (bShouldLog)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("*** BARRIER (CmdList: %016llX) %u/%u: %016llX (Sub: %u), %s -> %s"), pCommandList, i + 1, Barriers.Num(),
					currentBarrier.Transition.pResource,
					currentBarrier.Transition.Subresource,
					*StateBefore,
					*StateAfter);
			}
			break;
		}

		case D3D12_RESOURCE_BARRIER_TYPE_UAV:
			UE_LOG(LogD3D12RHI, Log, TEXT("*** BARRIER (CmdList: %016llX) %u/%u: UAV Barrier"), pCommandList, i + 1, Barriers.Num());
			break;

		case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
			UE_LOG(LogD3D12RHI, Log, TEXT("*** BARRIER (CmdList: %016llX) %u/%u: Aliasing Barrier, %016llX -> %016llX"), pCommandList, i + 1, Barriers.Num(), currentBarrier.Aliasing.pResourceBefore, currentBarrier.Aliasing.pResourceAfter);
			break;

		default:
			check(false);
			break;
		}
	}
}


D3D12_RESOURCE_STATES GetD3D12ResourceState(ERHIAccess InRHIAccess, bool InIsAsyncCompute)
{
	// Add switch for common states (should cover all writeable states)
	switch (InRHIAccess)
	{
	// all single write states
	case ERHIAccess::RTV:					return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case ERHIAccess::UAVMask:		
	case ERHIAccess::UAVCompute:	
	case ERHIAccess::UAVGraphics:			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	case ERHIAccess::DSVWrite:				return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	case ERHIAccess::CopyDest:				return D3D12_RESOURCE_STATE_COPY_DEST;
	case ERHIAccess::ResolveDst:			return D3D12_RESOURCE_STATE_RESOLVE_DEST;
	case ERHIAccess::Present:				return D3D12_RESOURCE_STATE_PRESENT;

	// Generic read for mask read states
	case ERHIAccess::ReadOnlyMask:	
	case ERHIAccess::ReadOnlyExclusiveMask:	return D3D12_RESOURCE_STATE_GENERIC_READ;
	default:
	{
		// Special case for DSV read & write (Depth write allows depth read as well in D3D)
		if (InRHIAccess == ERHIAccess(ERHIAccess::DSVRead | ERHIAccess::DSVWrite))
		{
			return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}
		else
		{
			// Should be combination from read only flags (write flags covered above)
			check(!(EnumHasAnyFlags(InRHIAccess, ERHIAccess::WritableMask)));
			check(EnumHasAnyFlags(InRHIAccess, ERHIAccess::ReadOnlyMask));

			D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON;

			// Translate the requested after state to a D3D state
			if (EnumHasAnyFlags(InRHIAccess, ERHIAccess::SRVGraphics) && !InIsAsyncCompute)
			{
				State |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}
			if (EnumHasAnyFlags(InRHIAccess, ERHIAccess::SRVCompute))
			{
				State |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}
			if (EnumHasAnyFlags(InRHIAccess, ERHIAccess::VertexOrIndexBuffer))
			{
				State |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER;
			}
			if (EnumHasAnyFlags(InRHIAccess, ERHIAccess::CopySrc))
			{
				State |= D3D12_RESOURCE_STATE_COPY_SOURCE;
			}
			if (EnumHasAnyFlags(InRHIAccess, ERHIAccess::IndirectArgs))
			{
				State |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
			}
			if (EnumHasAnyFlags(InRHIAccess, ERHIAccess::ResolveSrc))
			{
				State |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
			}
			if (EnumHasAnyFlags(InRHIAccess, ERHIAccess::DSVRead))
			{
				State |= D3D12_RESOURCE_STATE_DEPTH_READ;
			}
#if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
			if (EnumHasAnyFlags(InRHIAccess, ERHIAccess::ShadingRateSource) && GRHISupportsAttachmentVariableRateShading)
			{
				State |= D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;
			}
#endif

			// Should have at least one valid state
			check(State != D3D12_RESOURCE_STATE_COMMON);

			return State;
		}
	}
	}
}

//==================================================================================================================================
// CResourceState
// Tracking of per-resource or per-subresource state
//==================================================================================================================================
//----------------------------------------------------------------------------------------------------------------------------------
void CResourceState::Initialize(uint32 SubresourceCount)
{
	check(0 == m_SubresourceState.Num());

	// Allocate space for per-subresource tracking structures
	check(SubresourceCount > 0);
	m_SubresourceState.SetNumUninitialized(SubresourceCount);
	check(m_SubresourceState.Num() == SubresourceCount);

	// No internal transition yet
	bHasInternalTransition = 0;

	// All subresources start out in an unknown state
	SetResourceState(D3D12_RESOURCE_STATE_TBD);

	// Unknown hidden resource state
	SetUAVHiddenResourceState(D3D12_RESOURCE_STATE_TBD);
}

//----------------------------------------------------------------------------------------------------------------------------------
bool CResourceState::AreAllSubresourcesSame() const
{
	return m_AllSubresourcesSame && (m_ResourceState != D3D12_RESOURCE_STATE_TBD);
}

//----------------------------------------------------------------------------------------------------------------------------------
bool CResourceState::CheckResourceState(D3D12_RESOURCE_STATES State) const
{
	if (m_AllSubresourcesSame)
	{
		return State == m_ResourceState;
	}
	else
	{
		// All subresources must be individually checked
		const uint32 numSubresourceStates = m_SubresourceState.Num();
		for (uint32 i = 0; i < numSubresourceStates; i++)
		{
			if (m_SubresourceState[i] != State)
			{
				return false;
			}
		}

		return true;
	}
}

//----------------------------------------------------------------------------------------------------------------------------------
bool CResourceState::CheckResourceStateInitalized() const
{
	return m_SubresourceState.Num() > 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
D3D12_RESOURCE_STATES CResourceState::GetSubresourceState(uint32 SubresourceIndex) const
{
	if (m_AllSubresourcesSame)
	{
		return static_cast<D3D12_RESOURCE_STATES>(m_ResourceState);
	}
	else
	{
		check(SubresourceIndex < static_cast<uint32>(m_SubresourceState.Num()));
		return m_SubresourceState[SubresourceIndex];
	}
}

//----------------------------------------------------------------------------------------------------------------------------------

bool CResourceState::CheckAllSubresourceSame()
{
	// already marked same?
	if (m_AllSubresourcesSame)
	{
		return true;
	}
	else
	{
		D3D12_RESOURCE_STATES State = m_SubresourceState[0];

		// All subresources must be individually checked
		const uint32 numSubresourceStates = m_SubresourceState.Num();
		for (uint32 i = 1; i < numSubresourceStates; i++)
		{
			if (m_SubresourceState[i] != State)
			{
				return false;
			}
		}

		SetResourceState(State);

		return true;
	}
}

//----------------------------------------------------------------------------------------------------------------------------------
void CResourceState::SetResourceState(D3D12_RESOURCE_STATES State)
{
	m_AllSubresourcesSame = 1;

	// m_ResourceState is restricted to 31 bits.  Ensure State can be properly represented.
	check((State & (1 << 31)) == 0);

	m_ResourceState = *reinterpret_cast<uint32*>(&State);

	// State is now tracked per-resource, so m_SubresourceState should not be read.
#if UE_BUILD_DEBUG
	const uint32 numSubresourceStates = m_SubresourceState.Num();
	for (uint32 i = 0; i < numSubresourceStates; i++)
	{
		m_SubresourceState[i] = D3D12_RESOURCE_STATE_CORRUPT;
	}
#endif
}

//----------------------------------------------------------------------------------------------------------------------------------
void CResourceState::SetSubresourceState(uint32 SubresourceIndex, D3D12_RESOURCE_STATES State)
{
	// If setting all subresources, or the resource only has a single subresource, set the per-resource state
	if (SubresourceIndex == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES ||
		m_SubresourceState.Num() == 1)
	{
		SetResourceState(State);
	}
	else
	{
		check(SubresourceIndex < static_cast<uint32>(m_SubresourceState.Num()));

		// If state was previously tracked on a per-resource level, then transition to per-subresource tracking
		if (m_AllSubresourcesSame)
		{
			const uint32 numSubresourceStates = m_SubresourceState.Num();
			for (uint32 i = 0; i < numSubresourceStates; i++)
			{
				m_SubresourceState[i] = static_cast<D3D12_RESOURCE_STATES>(m_ResourceState);
			}

			m_AllSubresourcesSame = 0;

			// State is now tracked per-subresource, so m_ResourceState should not be read.
#if UE_BUILD_DEBUG
			m_ResourceState = D3D12_RESOURCE_STATE_CORRUPT;
#endif
		}

		m_SubresourceState[SubresourceIndex] = State;
	}
}

#if ASSERT_RESOURCE_STATES
// Forward declarations are required for the template functions
template bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12View* pView, const D3D12_RESOURCE_STATES& State);

bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12View* pView, const D3D12_RESOURCE_STATES& State)
{
	// Check the view
	if (!pView)
	{
		// No need to check null views
		return true;
	}

	return AssertResourceState(pCommandList, pView->GetResource(), State, pView->GetViewSubresourceSubset());
}

bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12Resource* pResource, const D3D12_RESOURCE_STATES& State, uint32 Subresource)
{
	// Check the resource
	if (!pResource)
	{
		// No need to check null resources
		// Some dynamic SRVs haven't been mapped and updated yet so they actually don't have any backing resources.
		return true;
	}

	FD3D12ViewSubset ViewSubset(Subresource, pResource->GetMipLevels(), pResource->GetArraySize(), pResource->GetPlaneCount());
	return AssertResourceState(pCommandList, pResource, State, ViewSubset);
}

bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12Resource* pResource, const D3D12_RESOURCE_STATES& State, const FD3D12ViewSubset& ViewSubset)
{
#if PLATFORM_WINDOWS
	// Check the resource
	if (!pResource)
	{
		// No need to check null resources
		// Some dynamic SRVs haven't been mapped and updated yet so they actually don't have any backing resources.
		return true;
	}

	// Can only verify resource states if the debug layer is used
	static const bool bWithD3DDebug = GRHIGlobals.IsDebugLayerEnabled;
	if (!bWithD3DDebug)
	{
		UE_LOG(LogD3D12RHI, Fatal, TEXT("*** AssertResourceState requires the debug layer ***"));
		return false;
	}

	// Get the debug command queue
	TRefCountPtr<ID3D12DebugCommandList> pDebugCommandList;
	VERIFYD3D12RESULT(pCommandList->QueryInterface(pDebugCommandList.GetInitReference()));

	// Get the underlying resource
	ID3D12Resource* pD3D12Resource = pResource->GetResource();
	check(pD3D12Resource);

	// For each subresource in the view...
	for (uint32 SubresourceIndex : ViewSubset)
	{
		const bool bGoodState = !!pDebugCommandList->AssertResourceState(pD3D12Resource, SubresourceIndex, State);
		if (!bGoodState)
		{
			return false;
		}
	}
#endif // PLATFORM_WINDOWS

	return true;
}
#endif

//
// Stat declarations.
//

DEFINE_STAT(STAT_D3D12PresentTime);
DEFINE_STAT(STAT_D3D12CustomPresentTime);

DEFINE_STAT(STAT_D3D12NumCommandAllocators);
DEFINE_STAT(STAT_D3D12NumCommandLists);
DEFINE_STAT(STAT_D3D12NumQueryHeaps);
DEFINE_STAT(STAT_D3D12NumPSOs);

DEFINE_STAT(STAT_D3D12TexturesAllocated);
DEFINE_STAT(STAT_D3D12TexturesReleased);
DEFINE_STAT(STAT_D3D12CreateTextureTime);
DEFINE_STAT(STAT_D3D12LockTextureTime);
DEFINE_STAT(STAT_D3D12UnlockTextureTime);
DEFINE_STAT(STAT_D3D12CreateBufferTime);
DEFINE_STAT(STAT_D3D12CopyToStagingBufferTime);
DEFINE_STAT(STAT_D3D12LockBufferTime);
DEFINE_STAT(STAT_D3D12UnlockBufferTime);
DEFINE_STAT(STAT_D3D12CommitTransientResourceTime);
DEFINE_STAT(STAT_D3D12DecommitTransientResourceTime);

DEFINE_STAT(STAT_D3D12UAVBarriers);

DEFINE_STAT(STAT_D3D12NewBoundShaderStateTime);
DEFINE_STAT(STAT_D3D12CreateBoundShaderStateTime);
DEFINE_STAT(STAT_D3D12NumBoundShaderState);
DEFINE_STAT(STAT_D3D12SetBoundShaderState);

DEFINE_STAT(STAT_D3D12UpdateUniformBufferTime);

DEFINE_STAT(STAT_D3D12CommitResourceTables);
DEFINE_STAT(STAT_D3D12SetTextureInTableCalls);

DEFINE_STAT(STAT_D3D12DispatchShaderBundle);

DEFINE_STAT(STAT_D3D12ClearShaderResourceViewsTime);
DEFINE_STAT(STAT_D3D12SetShaderResourceViewTime);
DEFINE_STAT(STAT_D3D12SetUnorderedAccessViewTime);
DEFINE_STAT(STAT_D3D12CommitGraphicsConstants);
DEFINE_STAT(STAT_D3D12CommitComputeConstants);
DEFINE_STAT(STAT_D3D12SetShaderUniformBuffer);

DEFINE_STAT(STAT_D3D12ApplyStateTime);
DEFINE_STAT(STAT_D3D12ApplyStateRebuildPSOTime);
DEFINE_STAT(STAT_D3D12ApplyStateFindPSOTime);
DEFINE_STAT(STAT_D3D12ApplyStateSetSRVTime);
DEFINE_STAT(STAT_D3D12ApplyStateSetUAVTime);
DEFINE_STAT(STAT_D3D12ApplyStateSetVertexBufferTime);
DEFINE_STAT(STAT_D3D12ApplyStateSetConstantBufferTime);
DEFINE_STAT(STAT_D3D12ClearMRT);

DEFINE_STAT(STAT_D3D12ExecuteCommandListTime);
DEFINE_STAT(STAT_D3D12WaitForFenceTime);

DEFINE_STAT(STAT_D3D12UsedVideoMemory);
DEFINE_STAT(STAT_D3D12UsedSystemMemory);
DEFINE_STAT(STAT_D3D12AvailableVideoMemory);
DEFINE_STAT(STAT_D3D12DemotedVideoMemory);
DEFINE_STAT(STAT_D3D12TotalVideoMemory);

DEFINE_STAT(STAT_D3D12MemoryCurrentTotal);
DEFINE_STAT(STAT_D3D12RenderTargets);
DEFINE_STAT(STAT_D3D12UAVTextures);
DEFINE_STAT(STAT_D3D12Textures);
DEFINE_STAT(STAT_D3D12UAVBuffers);
DEFINE_STAT(STAT_D3D12RTBuffers);
DEFINE_STAT(STAT_D3D12Buffer);
DEFINE_STAT(STAT_D3D12TransientHeaps);

DEFINE_STAT(STAT_D3D12RenderTargetStandAloneAllocated);
DEFINE_STAT(STAT_D3D12UAVTextureStandAloneAllocated);
DEFINE_STAT(STAT_D3D12TextureStandAloneAllocated);
DEFINE_STAT(STAT_D3D12UAVBufferStandAloneAllocated);
DEFINE_STAT(STAT_D3D12BufferStandAloneAllocated);

DEFINE_STAT(STAT_D3D12RenderTargetStandAloneCount);
DEFINE_STAT(STAT_D3D12UAVTextureStandAloneCount);
DEFINE_STAT(STAT_D3D12TextureStandAloneCount);
DEFINE_STAT(STAT_D3D12UAVBufferStandAloneCount);
DEFINE_STAT(STAT_D3D12BufferStandAloneCount);

DEFINE_STAT(STAT_D3D12TextureAllocatorAllocated);
DEFINE_STAT(STAT_D3D12TextureAllocatorUnused);
DEFINE_STAT(STAT_D3D12TextureAllocatorCount);

DEFINE_STAT(STAT_D3D12BufferPoolMemoryAllocated);
DEFINE_STAT(STAT_D3D12BufferPoolMemoryUsed);
DEFINE_STAT(STAT_D3D12BufferPoolMemoryFree);
DEFINE_STAT(STAT_D3D12BufferPoolAlignmentWaste);
DEFINE_STAT(STAT_D3D12BufferPoolPageCount);
DEFINE_STAT(STAT_D3D12BufferPoolFullPages);
DEFINE_STAT(STAT_D3D12BufferPoolFragmentation);
DEFINE_STAT(STAT_D3D12BufferPoolFragmentationPercentage);

DEFINE_STAT(STAT_D3D12UploadPoolMemoryAllocated);
DEFINE_STAT(STAT_D3D12UploadPoolMemoryUsed);
DEFINE_STAT(STAT_D3D12UploadPoolMemoryFree);
DEFINE_STAT(STAT_D3D12UploadPoolAlignmentWaste);
DEFINE_STAT(STAT_D3D12UploadPoolPageCount);
DEFINE_STAT(STAT_D3D12UploadPoolFullPages);

DEFINE_STAT(STAT_D3D12ReservedResourcePhysical);

DEFINE_STAT(STAT_UniqueSamplers);

DEFINE_STAT(STAT_ViewHeapChanged);
DEFINE_STAT(STAT_SamplerHeapChanged);

DEFINE_STAT(STAT_NumViewOnlineDescriptorHeaps);
DEFINE_STAT(STAT_NumSamplerOnlineDescriptorHeaps);
DEFINE_STAT(STAT_NumReuseableSamplerOnlineDescriptorTables);
DEFINE_STAT(STAT_NumReuseableSamplerOnlineDescriptors);
DEFINE_STAT(STAT_NumReservedViewOnlineDescriptors);
DEFINE_STAT(STAT_NumReservedSamplerOnlineDescriptors);
DEFINE_STAT(STAT_NumReusedSamplerOnlineDescriptors);

DEFINE_STAT(STAT_GlobalViewHeapFreeDescriptors);
DEFINE_STAT(STAT_GlobalViewHeapReservedDescriptors);
DEFINE_STAT(STAT_GlobalViewHeapUsedDescriptors);
DEFINE_STAT(STAT_GlobalViewHeapWastedDescriptors);
DEFINE_STAT(STAT_GlobalViewHeapBlockAllocations);

DEFINE_STAT(STAT_ViewOnlineDescriptorHeapMemory);
DEFINE_STAT(STAT_SamplerOnlineDescriptorHeapMemory);

DEFINE_STAT(STAT_ExplicitSamplerDescriptorHeaps);
DEFINE_STAT(STAT_ExplicitSamplerDescriptors);

DEFINE_STAT(STAT_ExplicitViewDescriptorHeaps);
DEFINE_STAT(STAT_ExplicitViewDescriptors);

DEFINE_STAT(STAT_ExplicitMaxUsedSamplerDescriptors);
DEFINE_STAT(STAT_ExplicitUsedSamplerDescriptors);
DEFINE_STAT(STAT_ExplicitUsedViewDescriptors);

#undef LOCTEXT_NAMESPACE
