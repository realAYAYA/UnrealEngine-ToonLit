// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IObjectChooser.h"
#include "ChooserPropertyAccess.h"
#include "IChooserParameterProxyTable.h"
#include "InstancedStruct.h"
#include "ProxyTable.generated.h"


USTRUCT()
struct FProxyEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Data")
	FName Key;
	
	UPROPERTY()
	TScriptInterface<IObjectChooser> Value;
	
	UPROPERTY(DisplayName="Value", EditAnywhere, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ObjectChooserBase"), Category = "Data")
	FInstancedStruct ValueStruct;
	
};

UCLASS(MinimalAPI,BlueprintType)
class UProxyTable : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UProxyTable() {}

	UPROPERTY(EditAnywhere, Category = "Hidden")
	TArray<FProxyEntry> Entries;
	
	UPROPERTY(EditAnywhere, Category = "Inheritance")
	TArray<TObjectPtr<UProxyTable>> InheritEntriesFrom;
};


struct FBindingChainElement;


USTRUCT()
struct PROXYTABLE_API FProxyTableContextProperty :  public FChooserParameterProxyTableBase
{
	GENERATED_BODY()
public:

	UPROPERTY()
	TArray<FName> PropertyBindingChain;
	
	virtual bool GetValue(const UObject* ContextObject, const UProxyTable*& OutResult) const override;

#if WITH_EDITOR
	static bool CanBind(const FProperty& Property)
	{
		return Property.GetCPPType() == "UProxyTable*";
	}

	void SetBinding(const TArray<FBindingChainElement>& InBindingChain)
	{
		UE::Chooser::CopyPropertyChain(InBindingChain, PropertyBindingChain);
	}
#endif
};

USTRUCT()
struct PROXYTABLE_API FLookupProxy : public FObjectChooserBase
{
	GENERATED_BODY()
	virtual UObject* ChooseObject(const UObject* ContextObject) const final override;
	
	public:
	FLookupProxy();
	
	UPROPERTY(EditAnywhere, Meta = (ExcludeBaseStruct, BaseStruct ="/Script/ProxyTable.ChooserParameterProxyTableBase"), Category = "Input")
	FInstancedStruct ProxyTable;
	
	UPROPERTY(EditAnywhere, Category="Parameters")
	FName Key;
};

// deprecated classes for upgrading old data

UCLASS(ClassGroup = "LiveLink", deprecated)
class PROXYTABLE_API UDEPRECATED_ChooserParameterProxyTable_ContextProperty :  public UObject, public IChooserParameterProxyTable
{
	GENERATED_BODY()
public:

	UPROPERTY()
	TArray<FName> PropertyBindingChain;

	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const override
	{
		OutInstancedStruct.InitializeAs(FProxyTableContextProperty::StaticStruct());
		FProxyTableContextProperty& Property = OutInstancedStruct.GetMutable<FProxyTableContextProperty>();
		Property.PropertyBindingChain = PropertyBindingChain;
	}
};

UCLASS(ClassGroup = "LiveLink", deprecated)
class PROXYTABLE_API UDEPRECATED_ObjectChooser_LookupProxy : public UObject, public IObjectChooser
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TScriptInterface<IChooserParameterProxyTable> ProxyTable;
	
	UPROPERTY(EditAnywhere, Category="Parameters")
	FName Key;
	
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const override
   	{
   		OutInstancedStruct.InitializeAs(FLookupProxy::StaticStruct());
   		FLookupProxy& AssetChooser = OutInstancedStruct.GetMutable<FLookupProxy>();
		
		if (IChooserParameterProxyTable* ProxyTableInterface = ProxyTable.GetInterface())
		{
			ProxyTableInterface->ConvertToInstancedStruct(AssetChooser.ProxyTable);
		}
		AssetChooser.Key = Key;
   	}
};