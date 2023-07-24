// Copyright Epic Games, Inc. All Rights Reserved.
#include "ProxyTable.h"

FLookupProxy::FLookupProxy()
{
	ProxyTable.InitializeAs(FProxyTableContextProperty::StaticStruct());
}

static UObject* FindProxyObject(const UProxyTable* Table, FName Key, const UObject* ContextObject)
{
	for (const FProxyEntry& Entry : Table->Entries)
	{
		if (Entry.Key == Key && Entry.ValueStruct.IsValid())
		{
			const FObjectChooserBase& EntryValue = Entry.ValueStruct.Get<FObjectChooserBase>();
			return EntryValue.ChooseObject(ContextObject);
		}
	}

	// search parent tables (uncooked data only)
	for (const TObjectPtr<UProxyTable> ParentTable : Table->InheritEntriesFrom)
	{
		if (UObject* Value = FindProxyObject(ParentTable, Key, ContextObject))
		{
			return Value;
		}
	}

	return nullptr;
}

UObject* FLookupProxy::ChooseObject(const UObject* ContextObject) const
{
	if (ProxyTable.IsValid())
	{
		const UProxyTable* Table;
		if (ProxyTable.Get<FChooserParameterProxyTableBase>().GetValue(ContextObject, Table))
		{
			if (Table)
			{
				if (UObject* Value = FindProxyObject(Table, Key, ContextObject))
				{
					return Value;
				}
			}
		}
	}
	
	return nullptr;
}

UProxyTable::UProxyTable(const FObjectInitializer& Initializer)
	:Super(Initializer)
{

}

bool FProxyTableContextProperty::GetValue(const UObject* ContextObject, const UProxyTable*& OutResult) const
{
	UStruct* StructType = ContextObject->GetClass();
	const void* Container = ContextObject;
	
	if (UE::Chooser::ResolvePropertyChain(Container, StructType, PropertyBindingChain))
	{
		if (const FObjectProperty* Property = FindFProperty<FObjectProperty>(StructType, PropertyBindingChain.Last()))
		{
			OutResult = *Property->ContainerPtrToValuePtr<UProxyTable*>(Container);
			return true;
		}
	}

	return false;
}