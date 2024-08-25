// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGCrc.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"

#include "PCGData.generated.h"

class FArchiveCrc32;
class UPCGMetadata;
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

	virtual bool HasCachedLastSelector() const { return false; }
	virtual FPCGAttributePropertyInputSelector GetCachedLastSelector() const { return FPCGAttributePropertyInputSelector{}; }
	virtual void SetLastSelector(const FPCGAttributePropertySelector& InSelector) {};

	/** Return a copy of the data, with Metadata inheritence for spatial data. */
	virtual UPCGData* DuplicateData(bool bInitializeMetadata = true) const;

	// ~Begin UObject interface
	virtual void PostDuplicate(bool bDuplicateForPIE) override { InitUID(); }
	// ~End UObject interface

	// Metadata ops, to be implemented if data supports Metadata
	virtual UPCGMetadata* MutableMetadata() { return nullptr; }
	virtual const UPCGMetadata* ConstMetadata() const { return nullptr; }
	virtual void Flatten();

protected:
	/** Computes Crc for this and any connected data. */
	virtual FPCGCrc ComputeCrc(bool bFullDataCrc) const;

	/** Adds this data to Crc. Fallback implementation writes object instance UID. */
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const;

	/** Whether intersection, union, difference combine Crc values from operands. If false they fall back to using data UID. */
	bool PropagateCrcThroughBooleanData() const;

	void AddUIDToCrc(FArchiveCrc32& Ar) const;

private:
	void InitUID();

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

	/** The label of the pin that this data was either emitted from or received on. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data)
	FName Pin = NAME_None;

	// Special flag for data that are forwarded to other nodes, but without a pin. Useful for internal data.
	UPROPERTY()
	bool bPinlessData = false;

	bool operator==(const FPCGTaggedData& Other) const;
	bool operator!=(const FPCGTaggedData& Other) const;

	FPCGCrc ComputeCrc(bool bFullDataCrc) const;
};

USTRUCT(BlueprintType)
struct PCG_API FPCGDataCollection
{
	GENERATED_BODY()

	/** Returns all spatial data in the collection */
	TArray<FPCGTaggedData> GetInputs() const;
	/** Returns all data on a given pin. */
	TArray<FPCGTaggedData> GetInputsByPin(const FName& InPinLabel) const;
	/** Returns all spatial data on a given pin */
	TArray<FPCGTaggedData> GetSpatialInputsByPin(const FName& InPinLabel) const;

	/** Returns all data and corresponding cached data CRCs for a given pin. */
	template<typename AllocatorType1, typename AllocatorType2>
	void GetInputsAndCrcsByPin(const FName& InPinLabel, TArray<FPCGTaggedData, AllocatorType1>& OutData, TArray<FPCGCrc, AllocatorType2>& OutDataCrcs) const
	{
		if (!ensure(TaggedData.Num() == DataCrcs.Num()))
		{
			// CRCs are not up to date. Error recovery - add 0 CRCs.
			const_cast<TArray<FPCGCrc>&>(DataCrcs).SetNumZeroed(TaggedData.Num());
		}

		for (int I = 0; I < TaggedData.Num(); ++I)
		{
			if (ensure(TaggedData[I].Data) && TaggedData[I].Pin == InPinLabel)
			{
				OutData.Add(TaggedData[I]);
				OutDataCrcs.Add(DataCrcs[I]);
			}
		}
	}

	/** Gets number of data items on a given pin */
	int32 GetInputCountByPin(const FName& InPinLabel) const;
	/** Gets number of spatial data items on a given pin */
	int32 GetSpatialInputCountByPin(const FName& InPinLabel) const;
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
	UE_DEPRECATED(5.4, "Was not supposed to be used anyway, you should query the data per pin using GetParamsByPin")
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
	void AddReferences(FReferenceCollector& Collector);

	/** Computes CRCs for all data items. */
	void ComputeCrcs(bool bFullDataCrc);

	/** Add data and CRCs to collection. */
	void AddData(const TConstArrayView<FPCGTaggedData>& InData, const TConstArrayView<FPCGCrc>& InDataCrcs);
	/** Add data and CRCs to collection with pin label combined into the CRC. */
	void AddDataForPin(const TConstArrayView<FPCGTaggedData>& InData, const TConstArrayView<FPCGCrc>& InDataCrcs, uint32 InputPinLabelCrc);

	/** Cleans up the collection, but does not unroot any previously rooted data. */
	void Reset();

	/** Strips all empty point data from the collection. */
	int32 StripEmptyPointData();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data)
	TArray<FPCGTaggedData> TaggedData;

	/** Deprecated - Will be removed in 5.4 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data)
	bool bCancelExecutionOnEmpty = false;

	/** This flag is used to cancel further computation or for the debug/isolate feature */
	bool bCancelExecution = false;

	/** Per-data CRC which will capture tags, data, output pin and in some cases input pin too. */
	TArray<FPCGCrc> DataCrcs;

	/** After the task is complete, bit j is set if output pin index j is deactivated. Stored here so that it can be retrieved from the cache. */
	uint64 InactiveOutputPinBitmask = 0;
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
	/** Gets all inputs of the given class type, returning matching tagged data in the OutTaggedData value too */
	UFUNCTION(BlueprintCallable, Category = Data, meta = (ScriptMethod, DeterminesOutputType = "InDataTypeClass"))
	static TArray<UPCGData*> GetTypedInputs(const FPCGDataCollection& InCollection, TArray<FPCGTaggedData>& OutTaggedData, TSubclassOf<UPCGData> InDataTypeClass = nullptr);

	/** Gets all inputs of the given class type and on the given pin, returning matching tagged data in the OutTaggedData value too */
	UFUNCTION(BlueprintCallable, Category = Data, meta = (ScriptMethod, DeterminesOutputType = "InDataTypeClass"))
	static TArray<UPCGData*> GetTypedInputsByPin(const FPCGDataCollection& InCollection, const FPCGPinProperties& InPin, TArray<FPCGTaggedData>& OutTaggedData, TSubclassOf<UPCGData> InDataTypeClass = nullptr);

	/** Gets all inputs of the given class type and on the given pin label, returning matching tagged data in the OutTaggedData value too */
	UFUNCTION(BlueprintCallable, Category = Data, meta = (ScriptMethod, DeterminesOutputType = "InDataTypeClass"))
	static TArray<UPCGData*> GetTypedInputsByPinLabel(const FPCGDataCollection& InCollection, FName InPinLabel, TArray<FPCGTaggedData>& OutTaggedData, TSubclassOf<UPCGData> InDataTypeClass = nullptr);

	/** Gets all inputs of the given class type and having the provided tag, returning matching tagged data in the OutTaggedData value too */
	UFUNCTION(BlueprintCallable, Category = Data, meta = (ScriptMethod, DeterminesOutputType = "InDataTypeClass"))
	static TArray<UPCGData*> GetTypedInputsByTag(const FPCGDataCollection& InCollection, const FString& InTag, TArray<FPCGTaggedData>& OutTaggedData, TSubclassOf<UPCGData> InDataTypeClass = nullptr);

	/** Adds a data object to a given collection, simpler usage than making a PCGTaggedData object. InTags can be empty. */
	UFUNCTION(BlueprintCallable, Category = Data, meta = (ScriptMethod, AutoCreateRefTerm = "InTags"))
	static void AddToCollection(UPARAM(ref) FPCGDataCollection& InCollection, const UPCGData* InData, FName InPinLabel, TArray<FString> InTags);

	// Blueprint methods to support interaction with FPCGDataCollection
	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetInputs(const FPCGDataCollection& InCollection);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetInputsByPinLabel(const FPCGDataCollection& InCollection, const FName InPinLabel);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetInputsByTag(const FPCGDataCollection& InCollection, const FString& InTag);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetParams(const FPCGDataCollection& InCollection);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetParamsByPinLabel(const FPCGDataCollection& InCollection, const FName InPinLabel);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetParamsByTag(const FPCGDataCollection& InCollection, const FString& InTag);

	UFUNCTION(BlueprintCallable, Category = Data)
	static TArray<FPCGTaggedData> GetAllSettings(const FPCGDataCollection& InCollection);

protected:
	static TArray<UPCGData*> GetInputsByPredicate(const FPCGDataCollection& InCollection, TArray<FPCGTaggedData>& OutTaggedData, TFunctionRef<bool(const FPCGTaggedData&)> InPredicate);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "PCGModule.h"
#endif
