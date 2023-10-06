// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/ObjectNetSerializer.h"
#include "Iris/Core/NetObjectReference.h"
#include "Iris/Core/IrisLog.h"
#include "Net/Core/Trace/NetTrace.h"
#include "Iris/Serialization/InternalNetSerializer.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/ReplicationSystem/NetRefHandleManager.h"
#include "Iris/ReplicationSystem/ObjectReferenceCache.h"
#include "Iris/Serialization/NetBitStreamUtil.h"

namespace UE::Net
{

void WriteNetRefHandle(FNetSerializationContext& Context, const FNetRefHandle Handle)
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	if (Handle.IsValid())
	{
		Writer->WriteBool(true);
		WritePackedUint64(Writer, Handle.GetId());
	}
	else
	{
		Writer->WriteBool(false);
	}
}

FNetRefHandle ReadNetRefHandle(FNetSerializationContext& Context)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	if (Reader->ReadBool())
	{
		const uint64 NetId = ReadPackedUint64(Reader);
		if (!Reader->IsOverflown())
		{
			return Private::FNetRefHandleManager::MakeNetRefHandleFromId(NetId);
		}
	}
	return FNetRefHandle();
}

void ReadFullNetObjectReference(FNetSerializationContext& Context, FNetObjectReference& Reference)
{
	// Read full ref for now
	Context.GetInternalContext()->ObjectReferenceCache->ReadFullReference(Context, Reference);
}

void WriteFullNetObjectReference(FNetSerializationContext& Context, const FNetObjectReference& Reference)
{
	// Write full ref for now
	Context.GetInternalContext()->ObjectReferenceCache->WriteFullReference(Context, Reference);
}

}

namespace UE::Net::Private
{

static void ReadNetObjectReference(FNetSerializationContext& Context, FNetObjectReference& Reference)
{
	FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();

	if (InternalContext->bInlineObjectReferenceExports == 0U)
	{
		InternalContext->ObjectReferenceCache->ReadReference(Context, Reference);
	}
	else
	{
		InternalContext->ObjectReferenceCache->ReadFullReference(Context, Reference);
	}
}

static void WriteNetObjectReference(FNetSerializationContext& Context, const FNetObjectReference& Reference)
{
	FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();

	if (InternalContext->bInlineObjectReferenceExports == 0U)
	{
		InternalContext->ObjectReferenceCache->WriteReference(Context, Reference);
	}
	else
	{
		InternalContext->ObjectReferenceCache->WriteFullReference(Context, Reference);
	}
}

template<typename T>
struct FObjectNetSerializerBase
{
	typedef T SourceType;
	typedef FNetObjectReference QuantizedType;

	static void Serialize(FNetSerializationContext&, const FNetSerializeArgs& Args);
	static void Deserialize(FNetSerializationContext&, const FNetDeserializeArgs& Args);

	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args);

	static bool IsEqual(FNetSerializationContext&, const FNetIsEqualArgs& Args);

protected:
	static UObject* GetValue(UObject* Value) { return Value; }
	static void SetValue(UObject*& Dst, UObject* Value) { Dst = Value; }

	static UObject* GetValue(const TWeakObjectPtr<UObject>& Value) { return Value.Get(); }
	static void SetValue(TWeakObjectPtr<UObject>& Dst, UObject* Value) { Dst = TWeakObjectPtr<UObject>(Value); }

	// For FScriptInterface we have a custom Dequantize so we only provide a value getter.
	static UObject* GetValue(const FScriptInterface& Value) { return Value.GetObject(); }

	static UObject* ResolveObjectReference(FNetSerializationContext&, const FNetObjectReference&);
};

}

namespace UE::Net
{

struct FObjectNetSerializer : public Private::FObjectNetSerializerBase<UObject*>
{
	static const uint32 Version = 0;
	typedef FObjectNetSerializerConfig ConfigType;

	inline static const ConfigType DefaultConfig;
};

UE_NET_IMPLEMENT_SERIALIZER_INTERNAL(FObjectNetSerializer);

struct FWeakObjectNetSerializer : public Private::FObjectNetSerializerBase<TWeakObjectPtr<UObject>>
{
	static const uint32 Version = 0;
	typedef FWeakObjectNetSerializerConfig ConfigType;

	inline static const ConfigType DefaultConfig;
};

UE_NET_IMPLEMENT_SERIALIZER_INTERNAL(FWeakObjectNetSerializer);

struct FScriptInterfaceNetSerializer : public Private::FObjectNetSerializerBase<FScriptInterface>
{
	static const uint32 Version = 0;
	typedef FScriptInterfaceNetSerializerConfig ConfigType;

	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs&);

	static bool Validate(FNetSerializationContext&, const FNetValidateArgs&);
};

UE_NET_IMPLEMENT_SERIALIZER_INTERNAL(FScriptInterfaceNetSerializer);

}

namespace UE::Net::Private
{

template<typename T>
void FObjectNetSerializerBase<T>::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
{
	const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);

	WriteNetObjectReference(Context, Source);
}

template<typename T>
void FObjectNetSerializerBase<T>::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
{
	QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);

	// Read full ref for now
	ReadNetObjectReference(Context, Target);
}

template<typename T>
void FObjectNetSerializerBase<T>::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
{
	QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
	UObject* Source = GetValue(*reinterpret_cast<T*>(Args.Source));

	const FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();
	Target = InternalContext->ObjectReferenceCache ? InternalContext->ObjectReferenceCache->GetOrCreateObjectReference(Source) : QuantizedType();
}

template<typename T>
void FObjectNetSerializerBase<T>::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
{
	const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
	T& Target = *reinterpret_cast<T*>(Args.Target);
	
	UObject* DequantizedObject = ResolveObjectReference(Context, Source);
	SetValue(Target, DequantizedObject);
}

template<typename T>
bool FObjectNetSerializerBase<T>::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		const QuantizedType& Value0 = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& Value1 = *reinterpret_cast<const QuantizedType*>(Args.Source1);

		return Value0 == Value1;
	}
	else
	{
		const SourceType Value0 = *reinterpret_cast<const SourceType*>(Args.Source0);
		const SourceType Value1 = *reinterpret_cast<const SourceType*>(Args.Source1);

		return Value0 == Value1;
	}
}

template<typename T>
UObject* FObjectNetSerializerBase<T>::ResolveObjectReference(FNetSerializationContext& Context, const FNetObjectReference& NetObjectReference)
{
	const FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();

	UObject* Object = InternalContext->ObjectReferenceCache ? InternalContext->ObjectReferenceCache->ResolveObjectReference(NetObjectReference, InternalContext->ResolveContext) : nullptr;
	return Object;
}

}

namespace UE::Net
{

// FScriptInterfaceNetSerializer implementation
void FScriptInterfaceNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
{
	const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
	SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);

	UObject* Object = ResolveObjectReference(Context, Source);
	if (Object != nullptr)
	{
		const FScriptInterfaceNetSerializerConfig& Config = *static_cast<const FScriptInterfaceNetSerializerConfig*>(Args.NetSerializerConfig);
		void* Interface = Object->GetInterfaceAddress(Config.InterfaceClass);
		if (Interface == nullptr)
		{
			Context.SetError(GNetError_InvalidValue);
			return;
		}

		Target.SetObject(Object);
		Target.SetInterface(Interface);
	}
	else
	{
		// Setting a null object clears the interface as well.
		Target.SetObject(Object);
	}
}

bool FScriptInterfaceNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
{
	const SourceType& Value = *reinterpret_cast<const SourceType*>(Args.Source);
	if (UObject* Object = Value.GetObject())
	{
		void* Interface = Value.GetInterface();
		if (Interface == nullptr)
		{
			return false;
		}

		const FScriptInterfaceNetSerializerConfig& Config = *static_cast<const FScriptInterfaceNetSerializerConfig*>(Args.NetSerializerConfig);
		if (Interface != Object->GetInterfaceAddress(Config.InterfaceClass))
		{
			return false;
		}
	}
	else
	{
		if (Value.GetInterface() != nullptr)
		{
			return false;
		}
	}

	return true;
}

}
