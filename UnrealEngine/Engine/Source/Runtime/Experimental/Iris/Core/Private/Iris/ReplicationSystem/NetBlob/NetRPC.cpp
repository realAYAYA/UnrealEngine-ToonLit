// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetBlob/NetRPC.h"

#include "Iris/Core/BitTwiddling.h"
#include "Net/Core/Trace/NetDebugName.h"
#include "Net/Core/Trace/NetTrace.h"
#include "Net/Core/Misc/NetContext.h"
#include "Iris/ReplicationSystem/NetBlob/NetRPCHandler.h"
#include "Iris/ReplicationSystem/ObjectReferenceCache.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Iris/Serialization/NetReferenceCollector.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/ReplicationSystem/ReplicationOperationsInternal.h"
#include "Iris/ReplicationSystem/ReplicationProtocol.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/ObjectNetSerializer.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Core/IrisLog.h"
#include "Containers/ArrayView.h"
#include "ProfilingDebugging/CsvProfiler.h"

DEFINE_LOG_CATEGORY_STATIC(LogIrisRpc, Log, All);

namespace UE::Net::Private
{

static bool NetRPC_GetFunctionLocator(const UReplicationSystem* ReplicationSystem, const FNetObjectReference& ObjectReference, const UFunction* Function, FNetRPC::FFunctionLocator& OutFunctionLocator, const FReplicationStateMemberFunctionDescriptor*& OutFunctionDescriptor);

static UObject* NetRPC_GetObject(FNetSerializationContext& Context, const FNetObjectReference& ObjectReference);
static UObject* NetRPC_GetObject(FNetSerializationContext& Context, const FNetObjectReference& RootObjectReference, const FNetObjectReference& SubObjectReference);
static UObject* NetRPC_GetRootObject(FNetSerializationContext& Context, const FNetObjectReference& ObjectReference);

static bool NetRPC_GetFunctionAndObject(FNetSerializationContext& Context, const FNetObjectReference& RootObjectReference, const FNetObjectReference& SubObjectReferece, const FNetRPC::FFunctionLocator& FunctionLocator, const FReplicationStateMemberFunctionDescriptor*& OutFunctionDescriptor, TWeakObjectPtr<UObject>& OutObject);

static FString NetRPC_GetDebugFunctionName(const UReplicationSystem* ReplicationSystem, const FNetObjectReference& ObjectReference, const FNetRPC::FFunctionLocator& FunctionLocator);
static FString NetRPC_GetDebugObjectRefName(FNetSerializationContext& Context, const FNetObjectReference& ObjectReference, UObject* ObjectPtr = nullptr);

static const FName NetError_InvalidNetObjectReference("Invalid NetObjectRefererence");
static const FName NetError_UnknownFunction("Unknown RPC");
static const FName NetError_FunctionCallNotAllowed("RPC call not allowed");

FNetRPC::FNetRPC(const FNetBlobCreationInfo& CreationInfo)
: FNetObjectAttachment(CreationInfo)
, FunctionLocator({})
, Function(nullptr)
{
}

FNetRPC::~FNetRPC()
{
}

TArrayView<const FNetObjectReference> FNetRPC::GetExports() const
{
	return ReferencesToExport.IsValid() ? MakeArrayView(*ReferencesToExport) : MakeArrayView<const FNetObjectReference>(nullptr, 0);
}

void FNetRPC::SerializeWithObject(FNetSerializationContext& Context, FNetRefHandle RefHandle) const
{
	UE_NET_TRACE_DYNAMIC_NAME_SCOPE(BlobDescriptor->DebugName, *Context.GetBitStreamWriter(), Context.GetTraceCollector(), ENetTraceVerbosity::Verbose);

	FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();

	const uint32 HeaderPos = Writer.GetPosBits();
	InternalSerializeHeader(Context);
	if (Context.HasError())
	{
		return;
	}

	SerializeFunctionLocator(Context);
	if (Context.HasError())
	{
		return;
	}

	InternalSerializeSubObjectReference(Context, RefHandle);
	if (Context.HasError())
	{
		return;
	}

	InternalSerializeBlob(Context);
	if (Context.HasError())
	{
		return;
	}

	if (!Writer.IsOverflown())
	{
		// Re-serialize the final size value in the header
		const uint32 RPCSize = Writer.GetPosBits() - HeaderPos;
		FNetBitStreamWriteScope WriteScope(Writer, HeaderPos);
		InternalSerializeHeader(Context, RPCSize);
	}
}

void FNetRPC::DeserializeWithObject(FNetSerializationContext& Context, FNetRefHandle RefHandle)
{
	// We don't know the function name until much later.
	UE_NET_TRACE_NAMED_SCOPE(TraceScope, RPC, *Context.GetBitStreamReader(), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	// Store the next valid position after the NetRPC data.
	const uint32 HeaderPos = Context.GetBitStreamReader()->GetPosBits();
	const uint32 PostNetRPCPos = HeaderPos + InternalDeserializeHeader(Context);
	if (Context.HasErrorOrOverflow())
	{
		return;
	}

	DeserializeFunctionLocator(Context);
	if (Context.HasErrorOrOverflow())
	{
		return;
	}

	InternalDeserializeSubObjectReference(Context, RefHandle);
	if (Context.HasErrorOrOverflow())
	{
		return;
	}

	bool bResolveSucceeded = ResolveFunctionAndObject(Context);
	if (Context.HasErrorOrOverflow())
	{
		return;
	}
	
	if (!bResolveSucceeded)
	{
		UE_LOG(LogIrisRpc, Error, TEXT("DeserializeWithObject::Skipping RPC due missing object or function."));

		// Stop deserializing and seek past the entire payload if the resolve failed
		Context.GetBitStreamReader()->Seek(PostNetRPCPos);
		return;
	}

	UE_NET_TRACE_SET_SCOPE_NAME(TraceScope, BlobDescriptor->DebugName);
	InternalDeserializeBlob(Context);
	
	if (!Context.HasErrorOrOverflow())
	{
		// Just because the serialization didn't detect an error doesn't mean everything is ok. Validate stream position.
		if (PostNetRPCPos != Context.GetBitStreamReader()->GetPosBits())
		{
			UE_LOG(LogIrisRpc, Error, TEXT("Bitstream mismatch while deserializing function %s. Actual stream position: %u Expected stream position: %u"), ToCStr(BlobDescriptor->DebugName), Context.GetBitStreamReader()->GetPosBits(), PostNetRPCPos);
			ensureMsgf(PostNetRPCPos == Context.GetBitStreamReader()->GetPosBits(), TEXT("Bitstream mismatch while deserializing function %s. Actual stream position: %u Expected stream position: %u"), ToCStr(BlobDescriptor->DebugName), Context.GetBitStreamReader()->GetPosBits(), PostNetRPCPos);
			Context.GetBitStreamReader()->Seek(PostNetRPCPos);
			Context.SetError(GNetError_BitStreamError);
			// Make sure the RPC won't be exeuted regardless of how errors are handled.
			Function = nullptr;
		}
	}
}

void FNetRPC::Serialize(FNetSerializationContext& Context) const
{
	UE_NET_TRACE_DYNAMIC_NAME_SCOPE(BlobDescriptor->DebugName, *Context.GetBitStreamWriter(), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();
	
	const uint32 HeaderPos = Writer.GetPosBits();
	InternalSerializeHeader(Context);
	if (Context.HasError())
	{
		return;
	}

	InternalSerializeObjectReference(Context);
	if (Context.HasError())
	{
		return;
	}

	SerializeFunctionLocator(Context);
	if (Context.HasError())
	{
		return;
	}

	InternalSerializeBlob(Context);
	if (Context.HasError())
	{
		return;
	}

	if (!Writer.IsOverflown())
	{
		// Re-serialize the final size value in the header
		const uint32 RPCSize = Writer.GetPosBits() - HeaderPos;
		FNetBitStreamWriteScope WriteScope(Writer, HeaderPos);
		InternalSerializeHeader(Context, RPCSize);
	}
}

void FNetRPC::Deserialize(FNetSerializationContext& Context)
{
	UE_NET_TRACE_NAMED_SCOPE(TraceScope, RPC, *Context.GetBitStreamReader(), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	// Store the next valid bit position after the NetRPC data.
	const uint32 HeaderPos = Context.GetBitStreamReader()->GetPosBits();
	const uint32 PostNetRPCPos = HeaderPos + InternalDeserializeHeader(Context);
	if (Context.HasErrorOrOverflow())
	{
		return;
	}

	InternalDeserializeObjectReference(Context);
	if (Context.HasErrorOrOverflow())
	{
		return;
	}

	DeserializeFunctionLocator(Context);
	if (Context.HasErrorOrOverflow())
	{
		return;
	}
	
	bool bResolveSucceeded = ResolveFunctionAndObject(Context);
	if (Context.HasErrorOrOverflow())
	{
		return;
	}

	if (!bResolveSucceeded)
	{
		UE_LOG(LogIrisRpc, Error, TEXT("DeserializeWithObject::Skipping RPC due missing object or function."));
		// Stop deserializing and seek past the entire payload when the Resolve fails
		Context.GetBitStreamReader()->Seek(PostNetRPCPos);
		return;
	}
	
	UE_NET_TRACE_SET_SCOPE_NAME(TraceScope, BlobDescriptor->DebugName);

	InternalDeserializeBlob(Context);
	
	if (!Context.HasErrorOrOverflow())
	{
		check(PostNetRPCPos == Context.GetBitStreamReader()->GetPosBits());
	}
}

void FNetRPC::InternalSerializeHeader(FNetSerializationContext& Context, int32 PayloadSize /*= -1*/) const
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	// Pre-serialize the header before we know the final RPC payload size
	if (PayloadSize == -1)
	{
		UE_NET_TRACE_SCOPE(RPCSize, *Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
		Writer->WriteBits(0u, HeaderSizeBitCount);
	}
	// Write the final size once available
	else
	{
		checkf(PayloadSize > 0 && PayloadSize <= MaxRpcSizeInBits, TEXT("FNetRPC can only support a payload size of %u bits (RPC %s cost %u bits)"), MaxRpcSizeInBits, ToCStr(Function->GetName()), PayloadSize);
		Writer->WriteBits(PayloadSize, HeaderSizeBitCount);
	}
}

uint32 FNetRPC::InternalDeserializeHeader(FNetSerializationContext& Context)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();
	UE_NET_TRACE_SCOPE(RPCSize, *Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	return Reader->ReadBits(HeaderSizeBitCount);
}

void FNetRPC::SerializeFunctionLocator(FNetSerializationContext& Context) const
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	UE_NET_TRACE_SCOPE(FunctionLocator, *Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	const uint16 MaxValue = FMath::Max(FunctionLocator.DescriptorIndex, FunctionLocator.FunctionIndex);
	const uint32 NibbleCount = (GetBitsNeeded(MaxValue | uint16(1)) + 3) >> 2U;

	Writer->WriteBits(NibbleCount - 1U, 2U);
	Writer->WriteBits(FunctionLocator.DescriptorIndex, NibbleCount*4U);
	Writer->WriteBits(FunctionLocator.FunctionIndex, NibbleCount*4U);
}

void FNetRPC::DeserializeFunctionLocator(FNetSerializationContext& Context)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();
	UE_NET_TRACE_SCOPE(FunctionLocator, *Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	const uint32 NibbleCount = Reader->ReadBits(2U) + 1U;
	FunctionLocator.DescriptorIndex = static_cast<uint16>(Reader->ReadBits(NibbleCount*4U));
	FunctionLocator.FunctionIndex = static_cast<uint16>(Reader->ReadBits(NibbleCount*4U));
}

void FNetRPC::InternalSerializeObjectReference(FNetSerializationContext& Context) const
{
	UE_NET_TRACE_SCOPE(TargetObject, *Context.GetBitStreamWriter(), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
	SerializeObjectReference(Context);
}

void FNetRPC::InternalDeserializeObjectReference(FNetSerializationContext& Context)
{
	UE_NET_TRACE_SCOPE(TargetObject, *Context.GetBitStreamReader(), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
	DeserializeObjectReference(Context);
}

void FNetRPC::InternalSerializeSubObjectReference(FNetSerializationContext& Context, FNetRefHandle RefHandle) const
{
	UE_NET_TRACE_SCOPE(TargetObject, *(Context.GetBitStreamWriter()), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
	SerializeSubObjectReference(Context, RefHandle);
}

void FNetRPC::InternalDeserializeSubObjectReference(FNetSerializationContext& Context, FNetRefHandle RefHandle)
{
	UE_NET_TRACE_SCOPE(TargetObject, *(Context.GetBitStreamReader()), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
	DeserializeSubObjectReference(Context, RefHandle);
}

void FNetRPC::InternalSerializeBlob(FNetSerializationContext& Context) const
{
	UE_NET_TRACE_SCOPE(FunctionParams, *Context.GetBitStreamWriter(), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
	SerializeBlob(Context);
}

void FNetRPC::InternalDeserializeBlob(FNetSerializationContext& Context)
{
	UE_NET_TRACE_SCOPE(FunctionParams, *Context.GetBitStreamReader(), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
	DeserializeBlob(Context);
}

bool FNetRPC::ResolveFunctionAndObject(FNetSerializationContext& Context)
{
	// At this point we need a valid handle and FunctionLocator
	if (!NetObjectReference.GetRefHandle().IsValid())
	{
		// This can occur if sending side had queued up rpcs to object being invalidated
		return false;
	}

	const FReplicationStateMemberFunctionDescriptor* FunctionDescriptor = nullptr;
	if (!NetRPC_GetFunctionAndObject(Context, NetObjectReference, TargetObjectReference, FunctionLocator, FunctionDescriptor, ObjectPtr))
	{
		return false;
	}

	Function = FunctionDescriptor->Function;

	// Patch up NetBlobFlags based on function flags.
	if (Function)
	{
		if ((Function->FunctionFlags & FUNC_NetReliable) != 0)
		{
			CreationInfo.Flags |= ENetBlobFlags::Reliable;
		}

		// The sending side will set Ordered on unicast/reliable RPCs so we're restoring that flag. Unicast RPCs are ordered with respect to other reliable and unicast RPCs whereas multicast RPCs are not.
		if ((Function->FunctionFlags & FUNC_NetMulticast) == 0)
		{
			CreationInfo.Flags |= UE::Net::ENetBlobFlags::Ordered;
		}
	}

	// Set the BlobDescriptor even if it has zero size so that we can trace with a meaningful name.
	BlobDescriptor = FunctionDescriptor->Descriptor;

	if (FunctionDescriptor->Descriptor->InternalSize)
	{
		QuantizedBlobState = FQuantizedBlobState(FunctionDescriptor->Descriptor->InternalSize, FunctionDescriptor->Descriptor->InternalAlignment);
	}

	return true;
}

FNetRPC* FNetRPC::Create(UReplicationSystem* ReplicationSystem, const FNetBlobCreationInfo& CreationInfo, const FNetObjectReference& ObjectReference, const UFunction* Function, const void* FunctionParameters)
{
	while (const UFunction* SuperFunction = Function->GetSuperFunction())
	{
		Function = SuperFunction;
	};

	FFunctionLocator FunctionLocator = {};
	const FReplicationStateMemberFunctionDescriptor* FunctionDescriptor = nullptr;
	if (!NetRPC_GetFunctionLocator(ReplicationSystem, ObjectReference, Function, FunctionLocator, FunctionDescriptor))
	{
		return nullptr;
	}

	const FReplicationStateDescriptor* BlobDescriptor = FunctionDescriptor->Descriptor;
	FQuantizedBlobState QuantizedBlobState;

	// Don't spend CPU cycles on quantizing zero parameters
	if (BlobDescriptor != nullptr && BlobDescriptor->InternalSize)
	{
		QuantizedBlobState = FQuantizedBlobState(BlobDescriptor->InternalSize, BlobDescriptor->InternalAlignment);

		// Setup Context
		FNetSerializationContext Context;
		FInternalNetSerializationContext InternalContext(ReplicationSystem);
		
		Context.SetInternalContext(&InternalContext);

		// Quantize the function parameters
		FReplicationStateOperations::Quantize(Context, QuantizedBlobState.GetStateBuffer(), static_cast<const uint8*>(FunctionParameters), BlobDescriptor);
	}

	FNetRPC* NetRPC = new FNetRPC(CreationInfo);
	NetRPC->SetFunctionLocator(FunctionLocator);
	NetRPC->Function = Function;
	if (BlobDescriptor != nullptr)
	{
		if (BlobDescriptor->HasObjectReference())
		{
			// Collect all references and add them to potential exports
			using namespace UE::Net::Private;
			{
				FNetSerializationContext LocalContext;
				FNetReferenceCollector Collector(ENetReferenceCollectorTraits::OnlyCollectReferencesThatCanBeExported);
				const FNetSerializerChangeMaskParam InitStateChangeMaskInfo = { 0 };
				FReplicationStateOperationsInternal::CollectReferences(LocalContext, Collector, InitStateChangeMaskInfo, QuantizedBlobState.GetStateBuffer(), BlobDescriptor);

				if (Collector.GetCollectedReferences().Num())
				{
					NetRPC->ReferencesToExport = MakeUnique<FNetRPCExportsArray>();
					for (const FNetReferenceCollector::FReferenceInfo& Info : MakeArrayView(Collector.GetCollectedReferences()))
					{
						NetRPC->ReferencesToExport->AddUnique(Info.Reference);
					}

					NetRPC->CreationInfo.Flags |= ENetBlobFlags::HasExports;
				}
			}
		}

		NetRPC->SetState(BlobDescriptor, MoveTemp(QuantizedBlobState));
	}

	return NetRPC;
}

void FNetRPC::CallFunction(FNetRPCCallContext& CallContext)
{
#if UE_NET_IRIS_CSV_STATS
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(HandleRPC);
#endif

	FNetSerializationContext& Context = CallContext.GetNetSerializationContext();

	const UReplicationSystem* ReplicationSystem = Context.GetInternalContext()->ReplicationSystem;

	// Check whether we are ok with calling the function
	UObject* Object = ObjectPtr.Get();

	if (Object == nullptr)
	{
		Object = NetRPC_GetObject(Context, NetObjectReference, TargetObjectReference);
	}

	if (Object == nullptr || Function == nullptr)
	{
		if (!TargetObjectReference.IsValid())
		{
			UE_LOG(LogIrisRpc, Error, TEXT("Rejected RPC %s due to missing object or function for object: %s."),
				*(Function?Function->GetName():NetRPC_GetDebugFunctionName(ReplicationSystem, NetObjectReference, FunctionLocator)),
				*NetRPC_GetDebugObjectRefName(Context, NetObjectReference, Object));
		}
		else
		{
			UE_LOG(LogIrisRpc, Error, TEXT("Rejected RPC %s due to missing object or function for subobject: %s of rootobject: %s"),
				*(Function ? Function->GetName() : NetRPC_GetDebugFunctionName(ReplicationSystem, NetObjectReference, FunctionLocator)),
				*NetRPC_GetDebugObjectRefName(Context, TargetObjectReference, Object), *NetRPC_GetDebugObjectRefName(Context, NetObjectReference));
		}
		return;
	}

	const bool bIsServer = ReplicationSystem->IsServer();
	if (bIsServer)
	{
		if ((Function->FunctionFlags & FUNC_NetServer) == 0)
		{
			UE_LOG(LogIrisRpc, Error, TEXT("Rejected %s RPC function %s due to access rights. %s : %s"), (Function->FunctionFlags & FUNC_NetReliable ? TEXT("reliable") : TEXT("unreliable")), ToCStr(Function->GetName()), *NetObjectReference.ToString(), *Object->GetFullName());
			Context.SetError(NetError_FunctionCallNotAllowed);
			return;
		}

		if (!IsServerAllowedToExecuteRPC(Context))
		{
			// Cause error?
			UE_LOG(LogIrisRpc, Error, TEXT("Rejected %s RPC function %s due to server not allowed to execute access rights. %s : %s"), (Function->FunctionFlags & FUNC_NetReliable ? TEXT("reliable") : TEXT("unreliable")), ToCStr(Function->GetName()), *NetObjectReference.ToString(), *Object->GetFullName()); 
			return;
		}

	}
	else
	{
		if ((Function->FunctionFlags & (FUNC_NetClient | FUNC_NetMulticast)) == 0)
		{
			UE_LOG(LogIrisRpc, Error, TEXT("Rejected %s RPC function %s due to access rights. %s : %s"), (Function->FunctionFlags & FUNC_NetReliable ? TEXT("reliable") : TEXT("unreliable")), ToCStr(Function->GetName()), *NetObjectReference.ToString(), *Object->GetFullName());
			return;
		}
	}

	if (UE_LOG_ACTIVE(LogIrisRpc, Verbose))
	{
		bool bLogRpc = true;

		// Suppress spammy engine RPCs. This could be made a configable list in the future.
		const FString& FunctionName = Function->GetName();
		if (   FunctionName.Contains(TEXT("ServerUpdateCamera"))
			|| FunctionName.Contains(TEXT("ClientAckGoodMove"))
			|| FunctionName.Contains(TEXT("ServerMove")))
		{
			bLogRpc = false;
		}
		
		UE_CLOG(bLogRpc, LogIrisRpc, Verbose, TEXT("Calling %hs RPC function %s for %s : %s"), (Function->FunctionFlags & FUNC_NetReliable ? "reliable" : "unreliable"), ToCStr(Function->GetName()), *NetObjectReference.ToString(), *Object->GetFullName());
	}

	// Setup function parameters
	uint8* FunctionParameters = nullptr;
	if (Function->ParmsSize > 0)
	{
		check(BlobDescriptor.IsValid());

		FunctionParameters = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
		FMemory::Memzero(FunctionParameters, Function->ParmsSize);
		if (!EnumHasAnyFlags(BlobDescriptor->Traits, EReplicationStateTraits::IsSourceTriviallyConstructible))
		{
			const FReplicationStateMemberDescriptor* MemberDescriptors = BlobDescriptor->MemberDescriptors;
			const FReplicationStateMemberPropertyDescriptor* MemberPropertyDescriptors = BlobDescriptor->MemberPropertyDescriptors;
			const FProperty** MemberProperties = BlobDescriptor->MemberProperties;
			for (uint32 MemberIt = 0, MemberCount = BlobDescriptor->MemberCount; MemberIt < MemberCount; ++MemberIt)
			{
				const FReplicationStateMemberPropertyDescriptor& MemberPropertyDescriptor = MemberPropertyDescriptors[MemberIt];
		
				// InitializeValue operates on the entire static array so make sure not to call it other than for the first element.
				if (MemberPropertyDescriptor.ArrayIndex == 0)
				{
					const FProperty* Property = MemberProperties[MemberIt];
					// We will dequantize all parameters to this buffer so we don't care if zero is the wrong value as it will be overwritten anyway.
					// So we only need to initialize the value if it's complex, such as having virtual functions.
					if (!Property->HasAnyPropertyFlags(CPF_ZeroConstructor | CPF_IsPlainOldData))
					{
						const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
						Property->InitializeValue(FunctionParameters + MemberDescriptor.ExternalMemberOffset);
					}
				}
			}
		}
		
		FReplicationStateOperations::Dequantize(Context, FunctionParameters, QuantizedBlobState.GetStateBuffer(), BlobDescriptor);
	}

	// Forward function
	if (const FForwardNetRPCCallMulticastDelegate& Delegate = CallContext.GetForwardNetRPCCallDelegate(); Delegate.IsBound())
	{
		UObject* RootObject = NetRPC_GetRootObject(Context, NetObjectReference);
		UObject* SubObject = (Object != RootObject ? Object : static_cast<UObject*>(nullptr));
		Delegate.Broadcast(RootObject, SubObject, const_cast<UFunction*>(Function), FunctionParameters);
	}

	// Call function
	{
		UE::Net::FScopedNetContextRPC CallingRPC;
		Object->ProcessEvent(const_cast<UFunction*>(Function), FunctionParameters);
	}

	// Deinitialize function parameters
	if (FunctionParameters != nullptr)
	{

		if (!EnumHasAnyFlags(BlobDescriptor->Traits, EReplicationStateTraits::IsSourceTriviallyDestructible))
		{
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && (ParamIt->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm; ++ParamIt)
			{
				ParamIt->DestroyValue_InContainer(FunctionParameters);
			}
		}
	}
}

bool FNetRPC::IsServerAllowedToExecuteRPC(FNetSerializationContext& Context) const
{
	const FNetRefHandle Handle = NetObjectReference.GetRefHandle();
	const UReplicationSystem* ReplicationSystem = Context.GetInternalContext()->ReplicationSystem;

	const uint32 OwningConnectionId = ReplicationSystem->GetOwningNetConnection(Handle);
	const uint32 ExecutingConnectionId = Context.GetLocalConnectionId();
	const bool bTargetIsOwnedByConnection = OwningConnectionId == ExecutingConnectionId;
	return bTargetIsOwnedByConnection;
}

static bool NetRPC_GetFunctionLocator(const UReplicationSystem* ReplicationSystem, const FNetObjectReference& ObjectReference, const UFunction* Function, FNetRPC::FFunctionLocator& OutFunctionLocator, const FReplicationStateMemberFunctionDescriptor*& OutFunctionDescriptor)
{
	const UObjectReplicationBridge* Bridge = Cast<UObjectReplicationBridge>(ReplicationSystem->GetReplicationBridge());
	if (!ensure(Bridge != nullptr))
	{
		return false;
	}

	const FReplicationProtocol* Protocol = ReplicationSystem->GetReplicationProtocol(ObjectReference.GetRefHandle());
	if (!ensure(Protocol != nullptr))
	{
		return false;
	}

	for (const FReplicationStateDescriptor*& Descriptor : MakeArrayView(Protocol->ReplicationStateDescriptors, Protocol->ReplicationStateCount))
	{
		for (const FReplicationStateMemberFunctionDescriptor& FunctionDescriptor : MakeArrayView(Descriptor->MemberFunctionDescriptors, Descriptor->FunctionCount))
		{
			if (FunctionDescriptor.Function == Function)
			{
				OutFunctionLocator.DescriptorIndex = static_cast<uint16>(&Descriptor - Protocol->ReplicationStateDescriptors);
				OutFunctionLocator.FunctionIndex = static_cast<uint16>(&FunctionDescriptor - Descriptor->MemberFunctionDescriptors);
				OutFunctionDescriptor = &FunctionDescriptor;
				return true;
			}
		}
	}

	return false;
}

static UObject* NetRPC_GetObject(FNetSerializationContext& Context, const FNetObjectReference& RootObjectReference, const FNetObjectReference& SubObjectReference)
{
	return NetRPC_GetObject(Context, SubObjectReference.IsValid() ? SubObjectReference : RootObjectReference);
}

static UObject* NetRPC_GetObject(FNetSerializationContext& Context, const FNetObjectReference& ObjectReference)
{
	FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();
	return InternalContext->ObjectReferenceCache->ResolveObjectReference(ObjectReference, InternalContext->ResolveContext);
}

static UObject* NetRPC_GetRootObject(FNetSerializationContext& Context, const FNetObjectReference& ObjectReference)
{
	FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();
	const FNetRefHandleManager& NetRefHandleManager = InternalContext->ReplicationSystem->GetReplicationSystemInternal()->GetNetRefHandleManager();
	
	FNetRefHandle OwnerRefHandle = ObjectReference.GetRefHandle();
	const FInternalNetRefIndex InternalIndex = NetRefHandleManager.GetInternalIndex(OwnerRefHandle);
	if (!ensureMsgf(InternalIndex != FNetRefHandleManager::InvalidInternalIndex, TEXT("Unable to find InternalIndex for object reference %s"), ToCStr(ObjectReference.ToString())))
	{
		return nullptr;
	}

	const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager.GetReplicatedObjectDataNoCheck(InternalIndex);
	if (ObjectData.SubObjectRootIndex != FNetRefHandleManager::InvalidInternalIndex)
	{
		const FNetRefHandleManager::FReplicatedObjectData& RootObjectData = NetRefHandleManager.GetReplicatedObjectDataNoCheck(ObjectData.SubObjectRootIndex);
		OwnerRefHandle = RootObjectData.RefHandle;
	}

	UObject* RootObject = InternalContext->ObjectReferenceCache->ResolveObjectReferenceHandle(OwnerRefHandle, InternalContext->ResolveContext);
	return RootObject;
}

static bool NetRPC_GetFunctionAndObject(FNetSerializationContext& Context, const FNetObjectReference& ObjectReference, const FNetObjectReference& SubObjectReference, const FNetRPC::FFunctionLocator& FunctionLocator, const FReplicationStateMemberFunctionDescriptor*& OutFunctionDescriptor, TWeakObjectPtr<UObject>& OutObject)
{
	const UReplicationSystem* ReplicationSystem = Context.GetInternalContext()->ReplicationSystem;
	const UObjectReplicationBridge* Bridge = Cast<UObjectReplicationBridge>(ReplicationSystem->GetReplicationBridge());
	if (!ensure(Bridge != nullptr))
	{
		Context.SetError(NetError_InvalidNetObjectReference);
		return false;
	}

	UObject* RefObject = NetRPC_GetObject(Context, ObjectReference, SubObjectReference);
	if (!RefObject)
	{
		// Ignore this RPC and continue processing the rest of the data
        return false;
	}

	const FReplicationProtocol* Protocol = ReplicationSystem->GetReplicationProtocol(ObjectReference.GetRefHandle());
	if (!ensureMsgf(Protocol != nullptr, TEXT("ReplicationProtocol doesn't exist for %s (Connection %u). Ignoring RPC (%u|%u)"), 
		*GetNameSafe(RefObject), Context.GetLocalConnectionId(), FunctionLocator.DescriptorIndex, FunctionLocator.FunctionIndex))
	{
		Context.SetError(NetError_InvalidNetObjectReference);
		return false;
	}

	if (!ensure(FunctionLocator.DescriptorIndex < Protocol->ReplicationStateCount))
	{
		Context.SetError(NetError_UnknownFunction);
		return false;
	}

	const FReplicationStateDescriptor* Descriptor = Protocol->ReplicationStateDescriptors[FunctionLocator.DescriptorIndex];
	if (!ensure(FunctionLocator.FunctionIndex < Descriptor->FunctionCount))
	{
		Context.SetError(NetError_UnknownFunction);
		return false;
	}

	OutFunctionDescriptor = &Descriptor->MemberFunctionDescriptors[FunctionLocator.FunctionIndex];

	OutObject = RefObject;

	return true;
}

static FString NetRPC_GetDebugFunctionName(const UReplicationSystem* ReplicationSystem, const FNetObjectReference& ObjectReference, const FNetRPC::FFunctionLocator& FunctionLocator)
{
	const FReplicationProtocol* Protocol = ReplicationSystem->GetReplicationProtocol(ObjectReference.GetRefHandle());
	if (!Protocol)
	{
		return TEXT("ProtocolNotFound");
	}

	if (FunctionLocator.DescriptorIndex >= Protocol->ReplicationStateCount)
	{
		return TEXT("InvalidDescriptorIndex");
	}

	const FReplicationStateDescriptor* Descriptor = Protocol->ReplicationStateDescriptors[FunctionLocator.DescriptorIndex];
	if (FunctionLocator.FunctionIndex >= Descriptor->FunctionCount)
	{
		return TEXT("InvalidFunctionIndex");
	}

	return Descriptor->MemberFunctionDescriptors[FunctionLocator.FunctionIndex].Function->GetName();
}

static FString NetRPC_GetDebugObjectRefName(FNetSerializationContext& Context, const FNetObjectReference& ObjectReference, UObject* ObjectPtr)
{
	if (!ObjectPtr)
	{
		ObjectPtr = NetRPC_GetObject(Context, ObjectReference);
	}
	return FString::Printf(TEXT("%s (%s)"), *GetNameSafe(ObjectPtr), *ObjectReference.ToString());
}

}
