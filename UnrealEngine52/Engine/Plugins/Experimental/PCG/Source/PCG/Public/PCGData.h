// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGCrc.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "PCGData.generated.h"

class FArchiveCrc32;
class UPCGNode;
class UPCGParamData;
class UPCGSettings;
class UPCGSettingsInterface;
class UPCGSpatialData;

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
	UPCGData(const FObjectInitializer& ObjectInitializer);
	virtual EPCGDataType GetDataType() const { return EPCGDataType::None; }

	/** Returns a Crc for this and any connected data. */
	FPCGCrc GetOrComputeCrc(bool bFullDataCrc) const;

	/** Executes a lambda over all connected data objects. */
	virtual void VisitDataNetwork(TFunctionRef<void(const UPCGData*)> Action) const;

	/** Unique ID for this object instance. */
	UPROPERTY(Transient)
	uint64 UID = 0;

	/** CRC for this object instance. */
	mutable FPCGCrc Crc;

protected:
	/** Computes Crc for this and any connected data. */
	virtual FPCGCrc ComputeCrc(bool bFullDataCrc) const;

	/** Adds this data to Crc. Fallback implementation writes object instance UID. */
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const;

	/** Whether intersection, union, difference combine Crc values from operands. If false they fall back to using data UID. */
	bool PropagateCrcThroughBooleanData() const;

private:
	/** Serves unique ID values to instances of this object. */
	static inline std::atomic<uint64> UIDCounter{ 1 };
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

	// Special flag for data that are forwarded to other nodes, but without a pin. Useful for internal data.
	UPROPERTY()
	bool bPinlessData = false;

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
private:
	void AddInternal(UObject* InObject);
	void RemoveInternal(UObject* InObject);
};

USTRUCT(BlueprintType)
struct PCG_API FPCGDataCollection
{
	GENERATED_BODY()

	/** Returns all spatial data in the collection */
	TArray<FPCGTaggedData> GetInputs() const;
	/** Returns all data on a given pin */
	TArray<FPCGTaggedData> GetInputsByPin(const FName& InPinLabel) const;
	/** Returns spatial union of all data on a given pin, returns null if no such data exists. bOutUnionDataCreated indicates if new data created that may need rooting. */
	const UPCGSpatialData* GetSpatialUnionOfInputsByPin(const FName& InPinLabel, bool& bOutUnionDataCreated) const;
	/** Returns all spatial data in the collection with the given tag */
	TArray<FPCGTaggedData> GetTaggedInputs(const FString& InTag) const;
	/** Returns all settings in the collection */
	TArray<FPCGTaggedData> GetAllSettings() const;
	/** Returns all params in the collection */
	TArray<FPCGTaggedData> GetAllParams() const;
	/** Returns all params in the collection with a given tag */
	TArray<FPCGTaggedData> GetTaggedParams(const FString& InTag) const;
	/** Returns all params on a given pin */
	TArray<FPCGTaggedData> GetParamsByPin(const FName& InPinLabel) const;

	/** Returns all data in the collection with the given tag and given type */
	template <typename PCGDataType>
	TArray<FPCGTaggedData> GetTaggedTypedInputs(const FString& InTag) const;

	UE_DEPRECATED(5.2, "GetParams is deprecated, please use GetParamsByPin or GetFirstParamsOnParamsPin.")
	/** Returns the first params found in the collection */
	UPCGParamData* GetParams() const;

	// Only used as a temporary solution for old graph with nodes that didn't have params pins.
	// Should NOT be used with new nodes.
	UPCGParamData* GetParamsWithDeprecation(const UPCGNode* Node) const;

	/** Returns the first/only param found on the default params pin */
	UPCGParamData* GetFirstParamsOnParamsPin() const;

	const UPCGSettingsInterface* GetSettingsInterface() const;
	const UPCGSettingsInterface* GetSettingsInterface(const UPCGSettingsInterface* InDefaultSettingsInterface) const;

	/** Memory size calculation. Forward call to the data objects in the collection. */
	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const;

	template<typename SettingsType>
	const SettingsType* GetSettings() const;

	const UPCGSettings* GetSettings(const UPCGSettings* InDefaultSettings) const;

	bool operator==(const FPCGDataCollection& Other) const;
	bool operator!=(const FPCGDataCollection& Other) const;
	void AddToRootSet(FPCGRootSet& RootSet) const;
	void RemoveFromRootSet(FPCGRootSet& RootSet) const;

	/** Computes Crc for this data. */
	FPCGCrc ComputeCrc(bool bFullDataCrc);

	/** Cleans up the collection, but does not unroot any previously rooted data. */
	void Reset();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data)
	TArray<FPCGTaggedData> TaggedData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data)
	bool bCancelExecutionOnEmpty = false;

	/** This flag is used to cancel further computation or for the debug/isolate feature */
	bool bCancelExecution = false;

	/** A snapshot of the internal state of the data. If any dependency (setting, node input or external data) changes then this value should change. */
	FPCGCrc Crc;
};

template<typename SettingsType>
inline const SettingsType* FPCGDataCollection::GetSettings() const
{
	const FPCGTaggedData* MatchingData = TaggedData.FindByPredicate([](const FPCGTaggedData& Data) {
		return Cast<const SettingsType>(Data.Data) != nullptr;
		});

	return MatchingData ? Cast<const SettingsType>(MatchingData->Data) : nullptr;
}

template <typename PCGDataType>
inline TArray<FPCGTaggedData> FPCGDataCollection::GetTaggedTypedInputs(const FString& InTag) const
{
	return TaggedData.FilterByPredicate([&InTag](const FPCGTaggedData& Data) {
		return Data.Tags.Contains(InTag) && Cast<PCGDataType>(Data.Data);
	});
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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "PCGModule.h"
#endif
