// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IObjectChooser.h"
#include "ChooserPropertyAccess.h"
#include "IChooserParameterProxyTable.h"
#include "InstancedStructContainer.h"
#include "ProxyTable.h"
#include "LookupProxy.generated.h"

struct FBindingChainElement;

USTRUCT()
struct PROXYTABLE_API FProxyTableContextProperty :  public FChooserParameterProxyTableBase
{
	GENERATED_BODY()
public:
	
	UPROPERTY(EditAnywhere, Meta = (BindingType = "UProxyTable*"), Category = "Binding")
	FChooserPropertyBinding Binding;

	virtual bool GetValue(FChooserEvaluationContext& Context, const UProxyTable*& OutResult) const override;

#if WITH_EDITOR
	static bool CanBind(const FProperty& Property)
	{
		return Property.GetCPPType() == "UProxyTable*";
	}

	void SetBinding(const TArray<FBindingChainElement>& InBindingChain)
	{
		UE::Chooser::CopyPropertyChain(InBindingChain, Binding);
	}
#endif
};

USTRUCT()
struct PROXYTABLE_API FLookupProxy : public FObjectChooserBase
{
	GENERATED_BODY()
	virtual UObject* ChooseObject(FChooserEvaluationContext& Context) const final override;

	FLookupProxy();
	
	public:
	
	UPROPERTY(EditAnywhere, Category="Parameters")
	TObjectPtr<UProxyAsset> Proxy;

	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/ProxyTable.ChooserParameterProxyTableBase"), Category = "Parameters")
	FInstancedStruct ProxyTable;
};

USTRUCT(meta=(Hidden))
struct PROXYTABLE_API FLookupProxyWithOverrideTable : public FObjectChooserBase
{
	GENERATED_BODY()
	virtual UObject* ChooseObject(FChooserEvaluationContext& Context) const final override;
	
	public:
	
	UPROPERTY(EditAnywhere, Category="Parameters")
	TObjectPtr<UProxyAsset> Proxy;
	
	UPROPERTY(EditAnywhere, Category="Parameters")
	TObjectPtr<UProxyTable> OverrideProxyTable;
};