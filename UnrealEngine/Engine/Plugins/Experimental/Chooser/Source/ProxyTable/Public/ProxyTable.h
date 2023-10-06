// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IChooserParameterProxyTable.h"
#include "InstancedStruct.h"
#include "InstancedStructContainer.h"
#include "Misc/Guid.h"
#include "ProxyAsset.h"
#include "ProxyTable.generated.h"

USTRUCT()
struct PROXYTABLE_API FProxyStructOutput
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Data")
	FChooserStructPropertyBinding Binding;
	
	UPROPERTY(EditAnywhere, NoClear, Category = "Data")
	FInstancedStruct Value;
};

USTRUCT()
struct PROXYTABLE_API FProxyEntry
{
	GENERATED_BODY()

	bool operator== (const FProxyEntry& Other) const;
	bool operator< (const FProxyEntry& Other) const;
	
	UPROPERTY(EditAnywhere, Category = "Data")
	TObjectPtr<UProxyAsset> Proxy;

	// temporarily leaving this property for backwards compatibility with old content which used FNames rather than UProxyAsset
	UPROPERTY(EditAnywhere, Category = "Data")
	FName Key;
	
	UPROPERTY(DisplayName="Value", EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ObjectChooserBase"), Category = "Data")
	FInstancedStruct ValueStruct;

	UPROPERTY(EditAnywhere, NoClear, Category = "Data")
	TArray<FProxyStructOutput> OutputStructData;

	const FGuid GetGuid() const;
};

#if WITH_EDITORONLY_DATA
inline uint32 GetTypeHash(const FProxyEntry& Entry) { return GetTypeHash(Entry.GetGuid()); }
#endif

DECLARE_MULTICAST_DELEGATE(FProxyTableChanged);

USTRUCT()
struct PROXYTABLE_API FRuntimeProxyValue
{
	GENERATED_BODY()

	UPROPERTY()
	FInstancedStruct Value;

	UPROPERTY()
	TArray<FProxyStructOutput> OutputStructData;
};

UCLASS(MinimalAPI,BlueprintType)
class UProxyTable : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UProxyTable() {}

	UPROPERTY()
	TArray<FGuid> Keys;

	UPROPERTY()
	TArray<FRuntimeProxyValue> RuntimeValues;
	
	UObject* FindProxyObject(const FGuid& Key, FChooserEvaluationContext& Context) const;

#if WITH_EDITORONLY_DATA
public:
	FProxyTableChanged OnProxyTableChanged;
	
	UPROPERTY(EditAnywhere, Category = "Hidden")
	TArray<FProxyEntry> Entries;
	
	UPROPERTY(EditAnywhere, Category = "Inheritance")
	TArray<TObjectPtr<UProxyTable>> InheritEntriesFrom;

	virtual void PostLoad() override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
private:
	void BuildRuntimeData();
	TArray<TWeakObjectPtr<UProxyTable>> TableDependencies;
	TArray<TWeakObjectPtr<UProxyAsset>> ProxyDependencies;
#endif

};
