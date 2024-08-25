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

	CHOOSER_PARAMETER_BOILERPLATE();
};

USTRUCT()
struct PROXYTABLE_API FLookupProxy : public FObjectChooserBase
{
	GENERATED_BODY()
	virtual EIteratorStatus ChooseMulti(FChooserEvaluationContext &Context, FObjectChooserIteratorCallback Callback) const final override;
	virtual UObject* ChooseObject(FChooserEvaluationContext& Context) const final override;

	FLookupProxy();
	
	virtual void Compile(IHasContextClass* HasContext, bool bForce) override;

	virtual void GetDebugName(FString& OutName) const override;
	
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
	virtual EIteratorStatus ChooseMulti(FChooserEvaluationContext &Context, FObjectChooserIteratorCallback Callback) const final override;
	virtual UObject* ChooseObject(FChooserEvaluationContext& Context) const final override;
	
	public:
	
	UPROPERTY(EditAnywhere, Category="Parameters")
	TObjectPtr<UProxyAsset> Proxy;
	
	UPROPERTY(EditAnywhere, Category="Parameters")
	TObjectPtr<UProxyTable> OverrideProxyTable;
};