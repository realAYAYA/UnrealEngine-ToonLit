// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "PCGPin.generated.h"

class UPCGNode;
class UPCGEdge;

UENUM(BlueprintType)
enum class EPCGPinUsage : uint8
{
	/** Normal usage pin, will pass all data as is. */
	Normal = 0,
	/** When used in a loop subgraph node, will separate each data from that pin into separate subgraph executions. */
	Loop,
	/** When used in a loop subgraph node, will pass data on the feedback pins to the next iteration only if the data is passed from a previous iteration (or the original subgraph call). */
	Feedback,
	DependencyOnly UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EPCGPinStatus : uint8
{
	/** Normal usage pin. */
	Normal = 0,
	/** Only for input pins, mark this pin as required.If set on an output pin, behave as Normal. */
	Required,
	/** Advanced pin will be hidden by default in the UIand will be shown only if the user extends the node(in the UI) to see advanced pins.Pins can't be required and advanced at the same time. */
	Advanced
};

USTRUCT(BlueprintType, meta=(HasNativeBreak="/Script/PCG.PCGBlueprintPinHelpers.BreakPinProperty", HasNativeMake="/Script/PCG.PCGBlueprintPinHelpers.MakePinProperty"))
struct PCG_API FPCGPinProperties
{
	GENERATED_BODY()

	FPCGPinProperties() = default;
	explicit FPCGPinProperties(const FName& InLabel, EPCGDataType InAllowedTypes = EPCGDataType::Any, bool bInAllowMultipleConnections = true, bool bAllowMultipleData = true, const FText& InTooltip = FText::GetEmpty());

	bool operator==(const FPCGPinProperties& Other) const;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName Label = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGPinUsage Usage = EPCGPinUsage::Normal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGDataType AllowedTypes = EPCGDataType::Any;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAllowMultipleData = true;

	/** Define the status of a given pin (Normal, Required or Advanced) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGPinStatus PinStatus = EPCGPinStatus::Normal;

	UPROPERTY(BlueprintReadWrite, Category = Settings)
	bool bInvisiblePin = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Settings)
	FText Tooltip;
#endif

	// Multiple connections are only possible if we support multi data.
	bool AllowsMultipleConnections() const { return bAllowMultipleData && bAllowMultipleConnections; }

	// Allowing multiple connections will automatically enable multi data.
	void SetAllowMultipleConnections(bool bInAllowMultipleConnectons);

	bool IsAdvancedPin() const { return PinStatus == EPCGPinStatus::Advanced; }
	void SetAdvancedPin() { PinStatus = EPCGPinStatus::Advanced; }

	bool IsRequiredPin() const { return PinStatus == EPCGPinStatus::Required; }
	void SetRequiredPin() { PinStatus = EPCGPinStatus::Required; }

	bool IsNormalPin() const { return PinStatus == EPCGPinStatus::Normal; }
	void SetNormalPin() { PinStatus = EPCGPinStatus::Normal; }

	// Convert the bIsAdvanced boolean to PinStatus for deprecation purposes.
	void PostSerialize(const FArchive& Ar);

private:
#if WITH_EDITORONLY_DATA
	/* Advanced pin will be hidden by default in the UI and will be shown only if the user extend the node (in the UI) to see advanced pins. */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage = "Use IsAdvancedPin function or PinStatus property."))
	bool bAdvancedPin_DEPRECATED = false;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (AllowPrivateAccess = "true", EditCondition = "bAllowMultipleData", DisplayAfter = "bAllowMultipleData"))
	bool bAllowMultipleConnections = true;
};

template<>
struct TStructOpsTypeTraits<FPCGPinProperties> : public TStructOpsTypeTraitsBase2<FPCGPinProperties>
{
	enum
	{
		WithPostSerialize = true,
	};
};

UENUM()
enum class EPCGTypeConversion : uint8
{
	NoConversionRequired,
	CollapseToPoint,
	Filter,
	MakeConcrete,
	SplineToSurface,
	Failed,
};

UCLASS()
class PCG_API UPCGBlueprintPinHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="PCG|Pins", meta = (NativeBreakFunc))
	static void BreakPinProperty(const FPCGPinProperties& PinProperty, FName& Label, bool& bAllowMultipleData, bool& bAllowMultipleConnections, bool& bIsAdvancedPin, EPCGExclusiveDataType& AllowedType)
	{
		Label = PinProperty.Label;

		const UEnum* DataTypeEnum = StaticEnum<EPCGDataType>();
		const UEnum* ExclusiveDataTypeEnum = StaticEnum<EPCGExclusiveDataType>();

		AllowedType = EPCGExclusiveDataType::Other;

		if (DataTypeEnum && ExclusiveDataTypeEnum)
		{
			FName DataTypeName = DataTypeEnum->GetNameByValue(static_cast<__underlying_type(EPCGDataType)>(PinProperty.AllowedTypes));
			if (DataTypeName != NAME_None)
			{
				const int64 MatchingType = ExclusiveDataTypeEnum->GetValueByName(DataTypeName);

				if (MatchingType != INDEX_NONE)
				{
					AllowedType = static_cast<EPCGExclusiveDataType>(MatchingType);
				}
			}
		}

		bAllowMultipleData = PinProperty.bAllowMultipleData;
		bAllowMultipleConnections = PinProperty.AllowsMultipleConnections();
		bIsAdvancedPin = PinProperty.IsAdvancedPin();
	}

	UFUNCTION(BlueprintPure, Category="PCG|Pins", meta = (NativeMakeFunc))
	static FPCGPinProperties MakePinProperty(FName Label, bool bAllowMultipleData, bool bAllowMultipleConnections, bool bIsAdvancedPin, EPCGExclusiveDataType AllowedType = EPCGExclusiveDataType::Any)
	{
		const UEnum* DataTypeEnum = StaticEnum<EPCGDataType>();
		const UEnum* ExclusiveDataTypeEnum = StaticEnum<EPCGExclusiveDataType>();

		EPCGDataType DataType = EPCGDataType::Other;

		if (DataTypeEnum && ExclusiveDataTypeEnum)
		{
			FName ExclusiveDataTypeName = ExclusiveDataTypeEnum->GetNameByValue(static_cast<__underlying_type(EPCGExclusiveDataType)>(AllowedType));
			if (ensure(ExclusiveDataTypeName != NAME_None))
			{
				const int64 MatchingType = DataTypeEnum->GetValueByName(ExclusiveDataTypeName);
				if (ensure(MatchingType != INDEX_NONE))
				{
					DataType = static_cast<EPCGDataType>(MatchingType);
				}
			}
		}

		FPCGPinProperties PinProperties = FPCGPinProperties(Label, DataType, bAllowMultipleConnections, bAllowMultipleData);
		if (bIsAdvancedPin)
		{
			PinProperties.SetAdvancedPin();
		}

		return PinProperties;
	}
};

UCLASS(ClassGroup = (Procedural))
class PCG_API UPCGPin : public UObject
{
	GENERATED_BODY()

public:
	UPCGPin(const FObjectInitializer& ObjectInitializer);

	// ~Begin UObject interface
	virtual void PostLoad() override;
	// ~End UObject interface

	UPROPERTY(BlueprintReadOnly, Category = Properties)
	TObjectPtr<UPCGNode> Node = nullptr;

	UPROPERTY()
	FName Label_DEPRECATED = NAME_None;

	UPROPERTY(BlueprintReadOnly, TextExportTransient, Category = Properties)
	TArray<TObjectPtr<UPCGEdge>> Edges;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
	FPCGPinProperties Properties;

	UFUNCTION(BlueprintCallable, Category = Settings)
	FText GetTooltip() const;
	
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetTooltip(const FText& InTooltip);

	bool AllowsMultipleConnections() const;
	bool AllowsMultipleData() const;
	bool IsCompatible(const UPCGPin* OtherPin) const;
	bool CanConnect(const UPCGPin* OtherPin) const;

	/** Adds edge to pin if edge does not exist. Optionally returns list of affected nodes (including the pin owner). */
	bool AddEdgeTo(UPCGPin* OtherPin, TSet<UPCGNode*>* InTouchedNodes = nullptr);

	/** Breaks edge to pin if edge exists. Optionally returns list of affected nodes (including the pin owner). */
	bool BreakEdgeTo(UPCGPin* OtherPin, TSet<UPCGNode*>* InTouchedNodes = nullptr);

	/** Breaks all connected edges. Optionally returns list of affected nodes (including the pin owner). */
	bool BreakAllEdges(TSet<UPCGNode*>* InTouchedNodes = nullptr);

	/** Break all edges the connect pins of invalid type. Optionally returns list of affected nodes (including the pin owner). */
	bool BreakAllIncompatibleEdges(TSet<UPCGNode*>* InTouchedNodes = nullptr);

	bool IsConnected() const;
	bool IsOutputPin() const;
	int32 EdgeCount() const;

	/** Returns the current pin types, which can either be the static types from the pin properties, or a dynamic type based on connected edges. */
	EPCGDataType GetCurrentTypes() const;

	EPCGTypeConversion GetRequiredTypeConversion(const UPCGPin* InOtherPin) const;
};

#if WITH_EDITOR
namespace PCGPinPropertiesHelpers
{
	bool GetDefaultPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip);
	bool GetDefaultPinExtraIcon(const FPCGPinProperties& InPinProperties, FName& OutExtraIcon, FText& OutTooltip);
}
#endif // WITH_EDITOR

/**
* Helper class to allow the BP to call the custom functions on FPCGPinProperties.
*/
UCLASS()
class PCG_API UPCGPinPropertiesBlueprintHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PCG|PinProperties", meta = (ScriptMethod))
	static bool AllowsMultipleConnections(UPARAM(ref) const FPCGPinProperties& PinProperties) { return PinProperties.AllowsMultipleConnections(); }

	UFUNCTION(BlueprintCallable, Category = "PCG|PinProperties", meta = (ScriptMethod))
	static void SetAllowMultipleConnections(UPARAM(ref) FPCGPinProperties& PinProperties, bool bAllowMultipleConnections) { PinProperties.SetAllowMultipleConnections(bAllowMultipleConnections); }

	UFUNCTION(BlueprintCallable, Category = "PCG|PinProperties", meta = (ScriptMethod))
	static bool IsAdvancedPin(UPARAM(ref) const FPCGPinProperties& PinProperties) { return PinProperties.IsAdvancedPin(); }

	UFUNCTION(BlueprintCallable, Category = "PCG|PinProperties", meta = (ScriptMethod))
	static void SetAdvancedPin(UPARAM(ref) FPCGPinProperties& PinProperties) { PinProperties.SetAdvancedPin(); }

	UFUNCTION(BlueprintCallable, Category = "PCG|PinProperties", meta = (ScriptMethod))
	static bool IsRequiredPin(UPARAM(ref) const FPCGPinProperties& PinProperties) { return PinProperties.IsRequiredPin(); }

	UFUNCTION(BlueprintCallable, Category = "PCG|PinProperties", meta = (ScriptMethod))
	static void SetRequiredPin(UPARAM(ref) FPCGPinProperties& PinProperties) { PinProperties.SetRequiredPin(); }

	UFUNCTION(BlueprintCallable, Category = "PCG|PinProperties", meta = (ScriptMethod))
	static bool IsNormalPin(UPARAM(ref) const FPCGPinProperties& PinProperties) { return PinProperties.IsNormalPin(); }

	UFUNCTION(BlueprintCallable, Category = "PCG|PinProperties", meta = (ScriptMethod))
	static void SetNormalPin(UPARAM(ref) FPCGPinProperties& PinProperties) { PinProperties.SetNormalPin(); }
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
