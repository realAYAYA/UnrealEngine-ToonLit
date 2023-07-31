// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "PCGModule.h"
#include "PCGCommon.h"

#include "PCGData.generated.h"

class UPCGSettings;
class UPCGParamData;

/**
* Base class for any "data" class in the PCG framework.
* This is an intentionally vague base class so we can have the required
* flexibility to pass in various concrete data types, settings, and more.
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGData : public UObject
{
	GENERATED_BODY()

public:
	virtual EPCGDataType GetDataType() const { return EPCGDataType::None; }
};

USTRUCT(BlueprintType)
struct PCG_API FPCGTaggedData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data)
	TObjectPtr<const UPCGData> Data = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data)
	TSet<FString> Tags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data)
	FName Pin = NAME_None;

	bool operator==(const FPCGTaggedData& Other) const;
	bool operator!=(const FPCGTaggedData& Other) const;
};

struct FPCGRootSet
{
	~FPCGRootSet() { Clear(); }
	void Clear();
	void Add(UObject* InObject);
	void Remove(UObject* InObject);

	TMap<UObject*, int32> RootSet;
};

USTRUCT(BlueprintType)
struct PCG_API FPCGDataCollection
{
	GENERATED_BODY()

	TArray<FPCGTaggedData> GetInputs() const;
	TArray<FPCGTaggedData> GetInputsByPin(const FName& InPinLabel) const;
	TArray<FPCGTaggedData> GetTaggedInputs(const FString& InTag) const;
	TArray<FPCGTaggedData> GetAllSettings() const;
	TArray<FPCGTaggedData> GetAllParams() const;
	TArray<FPCGTaggedData> GetTaggedParams(const FString& InTag) const;
	TArray<FPCGTaggedData> GetParamsByPin(const FName& InPinLabel) const;
	UPCGParamData* GetParams() const;

	template<typename SettingsType>
	const SettingsType* GetSettings() const;

	const UPCGSettings* GetSettings(const UPCGSettings* InDefaultSettings) const;

	bool operator==(const FPCGDataCollection& Other) const;
	bool operator!=(const FPCGDataCollection& Other) const;
	void AddToRootSet(FPCGRootSet& RootSet) const;
	void RemoveFromRootSet(FPCGRootSet& RootSet) const;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data)
	TArray<FPCGTaggedData> TaggedData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data)
	bool bCancelExecutionOnEmpty = false;

	/** This flag is used to cancel further computation or for the debug/isolate feature */
	bool bCancelExecution = false;
};

template<typename SettingsType>
inline const SettingsType* FPCGDataCollection::GetSettings() const
{
	const FPCGTaggedData* MatchingData = TaggedData.FindByPredicate([](const FPCGTaggedData& Data) {
		return Cast<const SettingsType>(Data.Data) != nullptr;
		});

	return MatchingData ? Cast<const SettingsType>(MatchingData->Data) : nullptr;
}

UCLASS()
class PCG_API UPCGDataFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Blueprint methods to support interaction with FPCGDataCollection
	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetInputs(const FPCGDataCollection& InCollection);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetInputsByPin(const FPCGDataCollection& InCollection, const FName& InPinLabel);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetTaggedInputs(const FPCGDataCollection& InCollection, const FString& InTag);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetParams(const FPCGDataCollection& InCollection);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetParamsByPin(const FPCGDataCollection& InCollection, const FName& InPinLabel);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetTaggedParams(const FPCGDataCollection& InCollection, const FString& InTag);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetAllSettings(const FPCGDataCollection& InCollection);
};
