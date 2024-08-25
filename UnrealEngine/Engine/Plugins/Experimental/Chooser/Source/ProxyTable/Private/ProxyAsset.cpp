// Copyright Epic Games, Inc. All Rights Reserved.
#include "ProxyAsset.h"
#include "ProxyTableFunctionLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ChooserPropertyAccess.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/Package.h"
#include "LookupProxy.h"

FName UProxyAsset::TypeTagName = "ProxyType";

void UProxyAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	FString ResultTypeName;
	if (Type)
	{
		ResultTypeName = Type.GetName();
	}
	Context.AddTag(FAssetRegistryTag(TypeTagName, ResultTypeName, FAssetRegistryTag::TT_Alphabetical));
	
	Super::GetAssetRegistryTags(Context);
}

#if WITH_EDITOR
void UProxyAsset::PostEditUndo()
{
	UObject::PostEditUndo();

	if (CachedPreviousType != Type || CachedPreviousResultType != ResultType)
	{
		OnTypeChanged.Broadcast(Type);
		CachedPreviousType = Type;
		CachedPreviousResultType = ResultType;
	}
	
	OnContextClassChanged.Broadcast();
}

void UProxyAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
	
	static FName TypeName = "Type";
	static FName ResultTypeName = "ResultType";
	if (PropertyChangedEvent.Property->GetName() == TypeName)
	{
		if (CachedPreviousType != Type)
		{
			OnTypeChanged.Broadcast(Type);
		}
		CachedPreviousType = Type;
	}
	else if (PropertyChangedEvent.Property->GetName() == ResultTypeName)
	{
		if (CachedPreviousResultType != ResultType)
		{
			OnTypeChanged.Broadcast(Type);
			CachedPreviousResultType = ResultType;
		}
	}
	else
	{
		OnContextClassChanged.Broadcast();
	}
}

#endif

#if WITH_EDITORONLY_DATA

/////////////////////////////////////////////////////////////////////////////////////////
// Proxy Asset

void UProxyAsset::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	CachedPreviousType = Type;
	CachedPreviousResultType = ResultType;
#endif
	
	if (ContextClass_DEPRECATED)
	{
		
		ContextData.SetNum(1);
		ContextData[0].InitializeAs<FContextObjectTypeClass>();
		FContextObjectTypeClass& Context = ContextData[0].GetMutable<FContextObjectTypeClass>();
		Context.Class = ContextClass_DEPRECATED;
		Context.Direction = EContextObjectDirection::ReadWrite;
		ContextClass_DEPRECATED = nullptr;
	}

	if (!Guid.IsValid())
	{
		// if we load a ProxyAsset that was created before the Guid, assign it a deterministic guid based on the name and path.
		Guid.A = GetTypeHash(GetName());
		Guid.B = GetTypeHash(GetPackage()->GetPathName());
	}

	if (ProxyTable.IsValid())
	{
		// compile property access for Proxy Table fallback codepath
		if (FChooserParameterProxyTableBase* ProxyReference = ProxyTable.GetMutablePtr<FChooserParameterProxyTableBase>())
		{
			ProxyReference->Compile(this, false);
		}
	}
}

void UProxyAsset::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	UObject::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		// create a new guid when duplicating
		Guid = FGuid::NewGuid();
	}
}

#endif

UProxyAsset::UProxyAsset(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	ProxyTable.InitializeAs(FProxyTableContextProperty::StaticStruct());
}

UObject* UProxyAsset::FindProxyObject(FChooserEvaluationContext& Context) const
{
	if (ProxyTable.IsValid())
	{
		const UProxyTable* Table;
		if (ProxyTable.Get<FChooserParameterProxyTableBase>().GetValue(Context, Table))
		{
			if(Table)
			{
				if (UObject* Value = Table->FindProxyObject(Guid, Context))
				{
					return Value;
				}
			}
		}
	}
	
	return nullptr;
}

FObjectChooserBase::EIteratorStatus UProxyAsset::FindProxyObjectMulti(FChooserEvaluationContext &Context, FObjectChooserBase::FObjectChooserIteratorCallback Callback) const
{
	if (ProxyTable.IsValid())
	{
		const UProxyTable* Table;
		if (ProxyTable.Get<FChooserParameterProxyTableBase>().GetValue(Context, Table))
		{
			if(Table)
			{
				return Table->FindProxyObjectMulti(Guid, Context, Callback);
			}
		}
	}
	
	return FObjectChooserBase::EIteratorStatus::Continue;
}
