// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetBlob/NetRPC.h"

#include "Iris/Core/BitTwiddling.h"
#include "Net/Core/Trace/NetTrace.h"
#include "Iris/ReplicationSystem/ObjectReferenceCache.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Iris/Serialization/NetReferenceCollector.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/ReplicationSystem/ReplicationOperationsInternal.h"
#include "Iris/ReplicationSystem/ReplicationProtocol.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
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
static UObject* NetRPC_GetObject(FNetSerializationContext& Context, const FNetObjectReference& ObjectReference, const FNetObjectReference& SubObjectReference);							                
static bool NetRPC_GetFunctionAndObject(FNetSerializationContext& Context, const FNetObjectReference& ObjectReference, const FNetObjectReference& SubObjectReferece, FNetRPC::FFunctionLocator& FunctionLocator, const FReplicationStateMemberFunctionDescriptor*& OutFunctionDescriptor, TWeakObjectPtr<UObject>& OutObject);							                

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

void FNetRPC::SerializeWithObject(FNetSerializationContext& Context, FNetHandle NetHandle) const
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

	InternalSerializeSubObjectReference(Context, NetHandle);
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

void FNetRPC::DeserializeWithObject(FNetSerializationContext& Context, FNetHandle NetHandle)
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

	// $IRIS TODO Fix. May need to send subobject information
	InternalDeserializeSubObjectReference(Context, NetHandle);
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

		// Stop deserializing and seek past the entire payload if the Resolve failed
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
	FunctionLocator.DescriptorIndex = Reader->ReadBits(NibbleCount*4U);
	FunctionLocator.FunctionIndex = Reader->ReadBits(NibbleCount*4U);
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

void FNetRPC::InternalSerializeSubObjectReference(FNetSerializationContext& Context, FNetHandle NetHandle) const
{
	UE_NET_TRACE_SCOPE(TargetObject, *(Context.GetBitStreamWriter()), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
	SerializeSubObjectReference(Context, NetHandle);
}

void FNetRPC::InternalDeserializeSubObjectReference(FNetSerializationContext& Context, FNetHandle NetHandle)
{
	UE_NET_TRACE_SCOPE(TargetObject, *(Context.GetBitStreamReader()), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
	DeserializeSubObjectReference(Context, NetHandle);
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
	// At this point we need a valid NetHandle and FunctionLocator
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

	// Set the BlobDescriptor even if it has zero size so that we can trace with a meaningful name.
	BlobDescriptor = FunctionDescriptor->Descriptor;

	if (FunctionDescriptor->Descriptor->InternalSize)
	{
		uint8* StateBuffer = static_cast<uint8*>(GMalloc->Malloc(FunctionDescriptor->Descriptor->InternalSize, FunctionDescriptor->Descriptor->InternalAlignment));
		FMemory::Memzero(StateBuffer, FunctionDescriptor->Descriptor->InternalSize);
		QuantizedBlobState.Reset(StateBuffer);
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
	uint8* QuantizedBlobState = nullptr;

	// Don't spend CPU cycles on quantizing zero parameters
	if (BlobDescriptor->InternalSize)
	{
		uint8* StateBuffer = static_cast<uint8*>(GMalloc->Malloc(BlobDescriptor->InternalSize, BlobDescriptor->InternalAlignment));
		FMemory::Memzero(StateBuffer, BlobDescriptor->InternalSize);
		QuantizedBlobState = StateBuffer;

		// Setup Context
		FNetSerializationContext Context;
		FInternalNetSerializationContext InternalContext(ReplicationSystem);
		
		Context.SetInternalContext(&InternalContext);

		// Quantize the function parameters
		FReplicationStateOperations::Quantize(Context, StateBuffer, static_cast<const uint8*>(FunctionParameters), BlobDescriptor);
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
				FReplicationStateOperationsInternal::CollectReferences(LocalContext, Collector, InitStateChangeMaskInfo, QuantizedBlobState, BlobDescriptor);

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

		NetRPC->SetState(BlobDescriptor, TUniquePtr<uint8>(QuantizedBlobState));
	}

	return NetRPC;
}

void FNetRPC::CallFunction(FNetSerializationContext& Context)
{
#if UE_NET_IRIS_CSV_STATS
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(HandleRPC);
#endif

	// Check whether we are ok with calling the function
	UObject* Object = ObjectPtr.Get();

	if (Object == nullptr)
	{
		Object = NetRPC_GetObject(Context, NetObjectReference, TargetObjectReference);
	}

	if (Object == nullptr || Function == nullptr)
	{
		UE_LOG(LogIrisRpc, Error, TEXT("Rejected RPC function due missing object or function."));
		return;
	}

	const UReplicationSystem* ReplicationSystem = Context.GetInternalContext()->ReplicationSystem;
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
		if (Function->GetName().Contains(TEXT("ServerUpdateCamera"))) bLogRpc = false;
		if (Function->GetName().Contains(TEXT("ClientAckGoodMove"))) bLogRpc = false;
		if (Function->GetName().Contains(TEXT("ServerMove"))) bLogRpc = false;
		
		if (bLogRpc)
		{
			UE_LOG(LogIrisRpc, Verbose, TEXT("Calling %s RPC function %s for %s : %s"), (Function->FunctionFlags & FUNC_NetReliable ? TEXT("reliable") : TEXT("unreliable")), ToCStr(Function->GetName()), *NetObjectReference.ToString(), *Object->GetFullName());
		}
	}

	// Call the function
	if (Function->ParmsSize == 0)
	{
		Object->ProcessEvent(const_cast<UFunction*>(Function), nullptr);
	}
	else
	{
		check(BlobDescriptor.IsValid());

		uint8* FunctionParameters = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
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
		
		FReplicationStateOperations::Dequantize(Context, FunctionParameters, QuantizedBlobState.Get(), BlobDescriptor);
		Object->ProcessEvent(const_cast<UFunction*>(Function), FunctionParameters);

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
	const FNetHandle Handle = NetObjectReference.GetRefHandle();
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
					OutFunctionLocator.DescriptorIndex = &Descriptor - Protocol->ReplicationStateDescriptors;
					OutFunctionLocator.FunctionIndex = &FunctionDescriptor - Descriptor->MemberFunctionDescriptors;
					OutFunctionDescriptor = &FunctionDescriptor;
					return true;
				}
			}
		}

		return false;
	}

	static UObject* NetRPC_GetObject(FNetSerializationContext& Context, const FNetObjectReference& ObjectReference, const FNetObjectReference& SubObjectReference)
	{
		FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();
		return InternalContext->ObjectReferenceCache->ResolveObjectReference(SubObjectReference.IsValid() ? SubObjectReference : ObjectReference, InternalContext->ResolveContext);
	}

	static bool NetRPC_GetFunctionAndObject(FNetSerializationContext& Context, const FNetObjectReference& ObjectReference, const FNetObjectReference& SubObjectReference, FNetRPC::FFunctionLocator& FunctionLocator, const FReplicationStateMemberFunctionDescriptor*& OutFunctionDescriptor, TWeakObjectPtr<UObject>& OutObject)
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

}
