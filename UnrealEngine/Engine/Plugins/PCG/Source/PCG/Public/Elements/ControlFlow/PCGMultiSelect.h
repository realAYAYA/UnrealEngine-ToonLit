// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGControlFlow.h"

#include "PCGMultiSelect.generated.h"

/**
 * Selects data from any number of input pins, based on a static selection criteria (Int/String/Enum) 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), meta=(Keywords = "if multi switch enum"))
class UPCGMultiSelectSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject interface

	//~Begin UPCGSettings interface
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Select (Multi)")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::ControlFlow; }
	virtual bool HasDynamicPins() const override { return true; }
#endif // WITH_EDITOR

	virtual EPCGDataType GetCurrentPinTypes(const UPCGPin* Pin) const override;
	virtual FString GetAdditionalTitleInformation() const override;
	virtual bool HasFlippedTitleLines() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Determines the type of value to be used to select an input. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	EPCGControlFlowSelectionMode SelectionMode = EPCGControlFlowSelectionMode::Integer;

	/** Determines which input will be selected if the selection mode is Integer. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable, EditConditionHides, EditCondition="SelectionMode == EPCGControlFlowSelectionMode::Integer"))
	int32 IntegerSelection = 0;

	/** Determines the available input pin selection options. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(EditConditionHides, EditCondition="SelectionMode == EPCGControlFlowSelectionMode::Integer"))
	TArray<int32> IntOptions = {0};

	/** Determines which input will be selected if the selection mode is String. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable, EditConditionHides, NoResetToDefault, EditCondition="SelectionMode == EPCGControlFlowSelectionMode::String"))
	FString StringSelection;

	/** Determines the available input pin selection options. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(EditConditionHides, EditCondition="SelectionMode == EPCGControlFlowSelectionMode::String"))
	TArray<FString> StringOptions;

	/** Determines which input pin will be selected if the selection mode is Enum. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable, EditConditionHides, EditCondition="SelectionMode == EPCGControlFlowSelectionMode::Enum"))
	FEnumSelector EnumSelection;

	/** Cached pin labels for use during the selection process. */
	UPROPERTY(Transient)
	TArray<FName> CachedPinLabels;

	/** Returns true if the integer value exists in the user defined array. */
	bool IsValuePresent(int32 Value) const;

	/** Returns true if the string value exists in the user defined array. */
	bool IsValuePresent(const FString& Value) const;

	/** Returns true if the enum value exists within the enum class. */
	bool IsValuePresent(int64 Value) const;

	/** Helper function to use the appropriate selection value to determine the current selection. Returns true if it succeeds or false if the value is invalid. */
	bool GetSelectedPinLabel(FName& OutSelectedPinLabel) const;

	void CachePinLabels();
};

class FPCGMultiSelectElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
