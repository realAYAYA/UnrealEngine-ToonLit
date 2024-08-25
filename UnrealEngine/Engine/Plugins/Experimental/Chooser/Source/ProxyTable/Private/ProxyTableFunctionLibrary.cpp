// Copyright Epic Games, Inc. All Rights Reserved.
#include "ProxyTableFunctionLibrary.h"

#include "LookupProxy.h"
#include "Misc/StringBuilder.h"
#include "ProxyTable.h"

/////////////////////////////////////////////////////////////////////////////////////
// Blueprint Library Functions

UObject* UProxyTableFunctionLibrary::EvaluateProxyAsset(const UObject* ContextObject, const UProxyAsset* Proxy, TSubclassOf<UObject> ObjectClass)
{
	UObject* Result = nullptr;
	if (Proxy)
	{
		FChooserEvaluationContext Context(const_cast<UObject*>(ContextObject));
		
		Result = Proxy->FindProxyObject(Context);
		if (ObjectClass && Result && !Result->IsA(ObjectClass))
		{
			return nullptr;
		}
	}
	return Result;
	
}

// fallback for FName based Keys:
UObject* UProxyTableFunctionLibrary::EvaluateProxyTable(const UObject* ContextObject, const UProxyTable* ProxyTable, FName Key)
{
	if (ProxyTable)
	{
		FGuid Guid;
		Guid.A = GetTypeHash(WriteToString<128>(Key).ToView()); // Make sure this matches FProxyEntry::GetGuid
		FChooserEvaluationContext Context(const_cast<UObject*>(ContextObject));
		if (UObject* Value = ProxyTable->FindProxyObject(Guid, Context))
		{
			return Value;
		}
	}
	
	return nullptr;
}


FInstancedStruct UProxyTableFunctionLibrary::MakeLookupProxy(UProxyAsset* Proxy)
{
 	FInstancedStruct Struct;
 	Struct.InitializeAs(FLookupProxy::StaticStruct());
 	Struct.GetMutable<FLookupProxy>().Proxy = Proxy;
 	return Struct;
}

FInstancedStruct UProxyTableFunctionLibrary::MakeLookupProxyWithOverrideTable(UProxyAsset* Proxy, UProxyTable* ProxyTable)
{
 	FInstancedStruct Struct;
 	Struct.InitializeAs(FLookupProxyWithOverrideTable::StaticStruct());
 	FLookupProxyWithOverrideTable& Data = Struct.GetMutable<FLookupProxyWithOverrideTable>();
	Data.Proxy = Proxy;
	Data.OverrideProxyTable = ProxyTable;
 	return Struct;
}

